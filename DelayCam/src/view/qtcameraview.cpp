#include "qtcameraview.h"
#include "util/logger.h"

#include <QMediaDevices>
#include <QCameraFormat>

QtCameraView::QtCameraView(QWidget *parent) :
    CameraView(parent),
    camera_(nullptr),
    viewFinder_(new QVideoWidget(this)),
    captureSession_(new QMediaCaptureSession(this)),
    capturer_(new QImageCapture(this))
{
    // Setup viewfinder
    // It seems that the behavior has changed in Qt6:
    // QVideoWidget actually adds a private QVideoWindow (a private QWindow subclass)
    // and adds it using createWindowContainer(), which automatically puts the created
    // widget on top of the stacked order and as an opaque box, meaning that there is
    // no way to put anything above it.
    // -> TODO: Use custom widget:
    // https://stackoverflow.com/questions/71777390/make-qgraphicsvideoitem-fill-qwidget
    // TODO: While at it, also flip the image based on camera.orientation config
    viewFinder_->setAspectRatioMode(Qt::IgnoreAspectRatio);
    addViewFinder(viewFinder_);

    // Setup capture session
    captureSession_->setVideoOutput(viewFinder_);
    captureSession_->setImageCapture(capturer_);

    // Connect signals
    connect(capturer_, &QImageCapture::imageSaved, this, [=](int, const QString &path) { onImageSaved(path); });
    connect(&devices_, &QMediaDevices::videoInputsChanged, this, &QtCameraView::onCameraDevicesChanged);
}

QtCameraView::~QtCameraView()
{
}

bool QtCameraView::initCamera()
{
    // Test all cameras - default first
    QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    devices.insert(0, QMediaDevices::defaultVideoInput());
    for (const QCameraDevice &device : devices) {

        // Create camera if valid
        if (!device.isNull()) {
            fkInfo("Initializing camera: " + device.description());
            camera_ = new QCamera(device, this);
            captureSession_->setCamera(camera_);

            // Get format with nearest resolution
            const QList<QCameraFormat> formats = device.videoFormats();
            QCameraFormat format = nearestFormat(formats, frameSize_);
            camera_->setCameraFormat(format);

            // Connect error signal and return
            connect(camera_, &QCamera::errorOccurred, this, [=](QCamera::Error error, const QString &errorString) {
                if (error != QCamera::Error::NoError) {
                    fkWarning("Camera error: " + QString::number(error) + ": " + errorString);
                    releaseCamera();
                }
            });
            return true;
        }
    }
    return false;
}

void QtCameraView::startCamera()
{
    if (camera_ != nullptr)
        camera_->start();
}

void QtCameraView::capturePicture()
{
    // Capture picture and increase filename counter
    if (camera_ != nullptr)
        capturer_->captureToFile(imagePath());
}

void QtCameraView::autoFocus()
{
    // Trigger autofocus
    // TODO: This has to be tested with a proper camera
    if (camera_ != nullptr) {
        fkInfo("Autofocus triggered");
        camera_->setFocusMode(QCamera::FocusModeAuto);
    }
}

void QtCameraView::manualFocus(const QPointF &focus)
{
    // Manually focus to point
    if (camera_ != nullptr) {
        fkInfo("Manual focus triggered");
        camera_->setFocusMode(QCamera::FocusModeManual);
        camera_->setCustomFocusPoint(focus);
    }
}

void QtCameraView::stopCamera()
{
    if (camera_ != nullptr)
        camera_->stop();
}

void QtCameraView::releaseCamera()
{
    // Cleanup camera
    if (camera_ != nullptr) {
        delete camera_;
        camera_ = nullptr;
    }
}

void QtCameraView::onCameraDevicesChanged()
{
    // Release camera if it was lost
    if (camera_ != nullptr)
        if (!QMediaDevices::videoInputs().contains(camera_->cameraDevice()))
            releaseCamera();

    // (Re)init camera
    initCamera();
}
