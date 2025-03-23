#include "cameraview.h"
#include "preview.h"
#include "util/config.h"
#include "util/logger.h"
#include "ui/floatingbuttons.h"
#include "ui/countdownwidget.h"

#include <QKeyEvent>
#include <QBoxLayout>
#include <QFileInfo>
#include <QDir>

CameraView::CameraView(QWidget *parent) :
    QWidget{parent},
    frameRate_(30),
    frameSize_(1024, 600),
    fileNameCounter_(1),
    isInitialized_(false),
    focusOnCapture_(true)
{
    // Setup ui and set default values
    setupUi();
    setFocusPolicy(Qt::StrongFocus); // For hotkeys

    // Setup preview timer
    previewTimer_.setInterval(CFG.read<double>("counters.showPreviewTime", 5.0) * 1000);
    previewTimer_.setSingleShot(true);

    // Connect signals to slots
    connect(buttons_, &FloatingButtons::focusPressed, this, &CameraView::autoFocus);
    connect(buttons_, &FloatingButtons::backPressed, this, &CameraView::onBackPressed);
    connect(buttons_, &FloatingButtons::capturePressed, this, &CameraView::startCountdown);
    connect(&previewTimer_, &QTimer::timeout, this, &CameraView::displayViewFinder);
    connect(preview_, &Preview::animationFinished, this, &CameraView::startCamera);
    connect(countdownWidget_, &CountdownWidget::countdownFinished, this, &CameraView::capturePicture);
    connect(countdownWidget_, &CountdownWidget::keyValueReached, this, [=] {
        if (focusOnCapture_)
            autoFocus();
    });
    connect(buttons_, &FloatingButtons::deletePressed, this, [=] {
        if (showingPreview()) {
            emit deleteLastPressed();
            --fileNameCounter_;
        }
        displayViewFinder();
    });
}

void CameraView::configure(uint frameRate, const QSize &frameSize)
{
    frameRate_ = frameRate;
    frameSize_ = frameSize;
}

void CameraView::init()
{
    // Init camera
    fkInfo("Initializing camera");
    isInitialized_ = initCamera();
    if (!isInitialized_)
        releaseAndRetry();

    // Check if image path exists
    if (!QDir(CFG.imageDirectory()).exists()) {
        fkWarning("Image path does not exist!");
        isInitialized_ = false;
    }

    // Disable capturing if not initialized
    buttons_->enableButton(Buttons::CaptureButton, isInitialized_);
}

void CameraView::start()
{
    // Start camera
    fkInfo("Starting camera");
    buttons_->showButtons(Buttons::BackButton | Buttons::CaptureButton);
    displayViewFinder();
    startCamera();
}

void CameraView::stop()
{
    // Stop camera and reset
    fkInfo("Stopping camera");
    stopCamera();
    previewTimer_.stop();
    countdownWidget_->stopCountdown();
    buttons_->enableButton(Buttons::CaptureButton, isInitialized_);
}

void CameraView::focusOnce()
{
    // Start camera
    fkInfo("Starting camera to focus once");
    buttons_->showButtons(Buttons::BackButton | Buttons::FocusButton);
    displayViewFinder();
    startCamera();
}

void CameraView::releaseAndRetry()
{
    // Release camera and retry in a bit
    fkWarning("No valid camera detected. Retrying...");
    releaseCamera();
    static const double retryConnectTime = CFG.read<double>("counters.retryConnectTime", 1.0);
    QTimer::singleShot(retryConnectTime * 1000, this, &CameraView::init);
}

void CameraView::displayViewFinder()
{
    // Show viewfinder, change buttons
    // Viewfinder has to be created by child class first!
    if (stackedWidget_->count() >= 2) {
        previewTimer_.stop();
        stackedWidget_->setCurrentIndex(1);
        buttons_->showButton(Buttons::DeleteButton, false);
    }
}

void CameraView::displayPreview()
{
    // Show preview and change buttons
    stackedWidget_->setCurrentIndex(0);
    buttons_->showButton(Buttons::DeleteButton, true);
}

void CameraView::startCountdown()
{
    // Get counter start value once
    static const uint c = CFG.read<int>("counters.counter", 3);
    static const uint kv = CFG.read<int>("counters.focusAtTime", 2);

    // Start countdown, autofocus and switch on light
    if (isInitialized_ && isVisible()) {
        emit enableLeds(true);
        displayViewFinder();
        buttons_->enableButton(Buttons::CaptureButton, false);
        countdownWidget_->startCountdown(c, kv); // count from c, focus at kv
    }
}

bool CameraView::focusing() const
{
    return contains(buttons_->buttons(), Buttons::FocusButton);
}

bool CameraView::showingPreview() const
{
    return stackedWidget_->currentIndex() == 0;
}

bool CameraView::isInitialized() const
{
    return isInitialized_;
}

bool CameraView::focusOnCapture() const
{
    return focusOnCapture_;
}

QString CameraView::imagePath() const
{
    return CFG.imageDirectory() + settings::imageName.arg(fileNameCounter_, 4, 10, QChar('0')) + ".jpg";
}

int CameraView::fileNameCounter() const
{
    return fileNameCounter_;
}

void CameraView::setFocusOnCapture(bool focusOnCapture)
{
    focusOnCapture_ = focusOnCapture;
}

void CameraView::setFileNameCounter(int counter)
{
    fileNameCounter_ = counter;
}

void CameraView::onBackPressed()
{
    // Go back to viewfinder if preview
    if (showingPreview()) {
        displayViewFinder();
        previewTimer_.stop();

    // Stop and emit signal if viewfinder
    } else {
        stop();
        emit backPressed();
    }
}

void CameraView::onImageSaved(const QString &path)
{
    // Re-enable button
    buttons_->enableButton(Buttons::CaptureButton, isInitialized_);

    // Move image to correct path (workaround for gphoto2)
    QFileInfo info(path);
    QString newImagePath = path;
    if (QDir(CFG.imageDirectory()).absolutePath() != info.absolutePath()) {
        newImagePath = CFG.imageDirectory() + settings::imageName.arg(fileNameCounter_, 4, 10, QChar('0')) + "." + info.suffix();
        QDir().rename(info.filePath(), newImagePath);
    }

    // Show preview
    // Camera stream will be resumed after preview start animation finished
    // TODO: Load async -> Show capture animation -> Show preview without zoom
    preview_->loadImage(newImagePath);
    displayPreview();
    preview_->zoomAndReverse(2.0, 200);
    previewTimer_.start();

    // Disable LEds
    emit enableLeds(false);

    // Emit signal and increment filename counter
    fkInfo("Image captured");
    emit captured(newImagePath);
    ++fileNameCounter_;
}

void CameraView::setupUi()
{
    // Check if layout exists already
    if (layout() != nullptr)
        return;

    // Create widgets
    stackedWidget_ = new QStackedWidget(this);
    preview_ = new Preview(this);
    stackedWidget_->addWidget(preview_);

    // Add viewfinder to layout
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setSpacing(0);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(stackedWidget_);
    setLayout(hLayout);

    // Create floating buttons and countdown widget
    buttons_ = new FloatingButtons(Buttons::BackButton | Buttons::CaptureButton, settings::spacing, this);
    buttons_->enableButton(Buttons::CaptureButton, isInitialized_);
    countdownWidget_ = new CountdownWidget(this);
}

void CameraView::addViewFinder(QWidget *viewFinder)
{
    // Add viewfinder to stacked widget
    stackedWidget_->addWidget(viewFinder);
}

void CameraView::resizeEvent(QResizeEvent *)
{
    // Set floating widgets size and viewfinder size
    // frameSize_ is the size that is requested from the camera
    buttons_->setGeometry(rect());
    countdownWidget_->setGeometry(rect());
    frameSize_ = rect().size();
}

void CameraView::keyPressEvent(QKeyEvent *event)
{
    int k = event->key();

    // Trigger autofocus on 'F'
    if (k == Qt::Key_F) {
        autoFocus();

    // Restart stream on 'R'
    } else if (k == Qt::Key_R) {
        stopCamera();
        startCamera();

    // Else pass event to parent
    } else QWidget::keyPressEvent(event);
}

QSize smallestResolution(const QList<QSize> &sizes)
{
    // Return invalid size if list is empty
    if (sizes.isEmpty())
        return QSize();

    // Get smallest size
    QSize smallest = sizes.first();
    for (const QSize &s : sizes) {
        if (s.width() * s.height() < smallest.width() * smallest.height())
            smallest = s;
    }

    fkTrace("Smallest resolution: " + QString("%1 x %2").arg(smallest.width()).arg(smallest.height()));
    return smallest;
}

QSize nearestResolution(const QList<QSize> &sizes, const QSize &ref)
{
    // Return invalid size if list is empty
    if (sizes.isEmpty())
        return QSize();

    // Get nearest size
    QSize nearest = sizes.first();
    int nearestDiff = std::abs(nearest.width() - ref.width()) + std::abs(nearest.height() - ref.height());
    for (const QSize &s : sizes) {
        int diff = std::abs(s.width() - ref.width()) + std::abs(s.height() - ref.height());
        if (diff < nearestDiff) {
            nearestDiff = diff;
            nearest = s;
        }
    }

    fkTrace("Nearest resolution: " + QString("%1 x %2").arg(nearest.width()).arg(nearest.height()));
    return nearest;
}

QCameraFormat nearestFormat(const QList<QCameraFormat> &formats, const QSize &ref)
{
    // Return invalid format if list is empty
    if (formats.isEmpty())
        return QCameraFormat();

    // Get nearest size
    QCameraFormat nearest = formats.first();
    int nearestDiff = std::abs(nearest.resolution().width() - ref.width()) +
                      std::abs(nearest.resolution().height() - ref.height());
    for (const QCameraFormat &f : formats) {
        int diff = std::abs(f.resolution().width() - ref.width()) +
                   std::abs(f.resolution().height() - ref.height());
        if (diff < nearestDiff) {
            nearestDiff = diff;
            nearest = f;
        }
    }

    fkTrace("Nearest resolution: " + QString("%1 x %2").arg(
        nearest.resolution().width()).arg(
        nearest.resolution().height()));
    return nearest;
}
