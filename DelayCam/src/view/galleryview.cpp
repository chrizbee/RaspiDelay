#include "galleryview.h"
#include "preview.h"
#include "ui/thumbnailwidget.h"
#include "ui/floatingbuttons.h"
#include "util/imageloader.h"
#include "util/scroller.h"
#include "util/config.h"
#include "util/logger.h"
#include "util/usbdetector.h"
#include <QFutureWatcher>
#include <QScrollBar>
#include <QScroller>
#include <QDir>
#ifdef __linux__
#include <cstdlib>  // system()
#endif

GalleryView::GalleryView(QWidget *parent) :
    QGraphicsView{parent},
    scene_(new QGraphicsScene(this)),
    scroller_(new Scroller(this)),
    thumbnailWidth_(0),
    lastFileIndex_(0)
{
    // Setup ui
    setupUi();

    // Initialize usb detector
#ifdef __linux__
    QString mountPath = QString(CFG.read<std::string>("paths.mountPath", "").c_str());
#else
    QString mountPath;
#endif
    QStringList blacklist;
    std::vector<std::string> stdBlacklist = CFG.read<std::vector<std::string>>("paths.blacklist", std::vector<std::string>());
    for (const std::string &black : stdBlacklist)
        blacklist.append(QString(black.c_str()));
    usbDetector_ = new UsbDetector(blacklist, mountPath, this);
    buttons_->showButton(Buttons::UsbButton, usbDetector_->atleastOne());

    // Connect signals to slots
    connect(buttons_, &FloatingButtons::backPressed, this, &GalleryView::onBackPressed);
    connect(buttons_, &FloatingButtons::usbPressed, this, &GalleryView::copyGalleryToUsb);
    connect(scroller_, &Scroller::scrollBy, this, [=](int diff) {
        auto vbar = verticalScrollBar();
        vbar->setValue(vbar->value() + diff);
    });
    connect(usbDetector_, &UsbDetector::driveAdded, this, [=](const QString &path) { buttons_->showButton(Buttons::UsbButton, true); });
    connect(usbDetector_, &UsbDetector::driveRemoved, this, [=](const QString &path) { buttons_->showButton(Buttons::UsbButton, usbDetector_->atleastOne()); });
    connect(&watcher_, &QFutureWatcher<int>::started, this, [=]() { buttons_->enableButton(Buttons::UsbButton, false); });
    connect(&watcher_, &QFutureWatcher<int>::finished, this, [=]() { buttons_->enableButton(Buttons::UsbButton, true); });

    // Start detecting usb hotplugs
    usbDetector_->start();
}

GalleryView::~GalleryView()
{
    // Stop usb detector
    usbDetector_->interrupt();
}

void GalleryView::setImageDirectory(const QString &path)
{
    // Get a list of all images in path
    QDir imageDir(path);
    if (imageDir.exists()) {
        QStringList images = imageDir.entryList(QStringList() << "IMG*.jpg" << "IMG*.JPG", QDir::Files, QDir::Name);
        fkInfo(QString("Found %1 existing images in %2").arg(images.count()).arg(path));

        // Check if images were found
        if (!images.isEmpty()) {

            // Add thumbnails for images
            // This is critical if there are many images (~100µs/thumb on i7-13k)
            // Currently only loading the actual thumbnails is done async
            for (const QString &image : images)
                createThumbnailWidget(imageDir.absoluteFilePath(image));

            // Find out the last index
            QString last = images.last();
            last.remove("IMG_").remove(".jpg", Qt::CaseInsensitive);
            lastFileIndex_ = last.toInt();
        } else lastFileIndex_ = 0;
    }
}

void GalleryView::addImage(const QString &path)
{
    createThumbnailWidget(path);
}

void GalleryView::deleteLastImage()
{
    // Remove last image from gallery
    if (!thumbnails_.isEmpty()) {
        ThumbnailWidget *last = thumbnails_.takeLast();
        contentLayout_->removeItem(last);
        scene_->removeItem(last);
        --currentCell_;

        // Remove last image from file system
        QFile::remove(last->filePath());
        delete last;
    }
}

void GalleryView::resizeThumbnails(int galleryWidth)
{
    // Calculate width depending on view width
    double m = 28;
    double s = contentLayout_->columnSpacing(0);
    contentLayout_->getContentsMargins(&m, nullptr, nullptr, nullptr);
    int cc = CFG.read<int>("gallery.columnCount", 4);
    thumbnailWidth_ = (galleryWidth - 2 * m - (cc - 1) * s) / cc;

    // Set size for each thumbnail
    for (ThumbnailWidget *w : std::as_const(thumbnails_)) {
        if (w->fixedSize().width() != thumbnailWidth_) {
            w->setFixedWidth(thumbnailWidth_);
            imageLoader.load(w->filePath(), w->fixedSize(), w, &ThumbnailWidget::setThumbnail);
        }
    }

    // Reload layout so it's centered again
    contentLayout_->invalidate();
    contentLayout_->activate();
}

void GalleryView::setColors(const QColor &background, const QColor &shadow)
{
    // Set new background and shadow color
    setBackgroundBrush(background);
    shadowColor_ = shadow;
    for (ThumbnailWidget *w : std::as_const(thumbnails_))
        w->setShadowColor(shadowColor_);
}

int GalleryView::lastFileIndex() const
{
    return lastFileIndex_;
}

void GalleryView::setupUi()
{
    // Setup graphics view and scene
    setFrameShape(QFrame::NoFrame);
    setScene(scene_);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//    setTransformationAnchor(QGraphicsView::NoAnchor); // Needed for free translate
    shadowColor_ = QColor(CFG.read<std::string>("colors.shadowColor", "#444455").c_str());

    // Add content with layout to scene
    content_ = new QGraphicsWidget;
    contentLayout_ = new QGraphicsGridLayout;
    double m = CFG.read<int>("gallery.galleryMargins", 28);
    contentLayout_->setSpacing(CFG.read<int>("gallery.gallerySpacing", 16));
    contentLayout_->setContentsMargins(m, m, m, m);
    content_->setLayout(contentLayout_);
    scene_->addItem(content_);

    // Set column count
    currentCell_.columnCount = CFG.read<int>("gallery.columnCount", 4);

    // Create preview
    double maximumSwipeTime = CFG.read<double>("gallery.maximumSwipeTime", 1.4);
    int minimumSwipeDistance = CFG.read<int>("gallery.minimumSwipeDistance", 60);
    int swipeAnimationDuration = CFG.read<int>("gallery.swipeAnimationDuration", 100);
    preview_ = new Preview(this);
    preview_->setupSwipe(maximumSwipeTime, minimumSwipeDistance);
    preview_->setSwipeAnimationDuration(swipeAnimationDuration);
    preview_->raise();
    preview_->hide();

    // Create floating buttons
    buttons_ = new FloatingButtons(Buttons::BackButton, settings::spacing, this);
}

QStringList GalleryView::imagePaths() const
{
    // Return a list of all image paths
    QStringList paths;
    for (ThumbnailWidget *thumbnail : std::as_const(thumbnails_))
        paths.append(thumbnail->filePath());
    return paths;
}

void GalleryView::createThumbnailWidget(const QString &path)
{
    // Remember some settings and the current column
    static const int shadowRadius = CFG.read<int>("gallery.galleryShadowRadius", 18);
    static const double maximumPressDistance = CFG.read<double>("gallery.maximumPressDistance", 8);
    static const QPoint shadowOffset(CFG.read<int>("gallery.galleryShadowOffsetX", 3), CFG.read<int>("gallery.galleryShadowOffsetY", 5));

    // Create thumbnail widget and generate thumbnail for it
    ThumbnailWidget *w = new ThumbnailWidget(path);
    connect(w, &ThumbnailWidget::pressed, this, &GalleryView::onThumbnailPressed);
    w->setMaximumPressDistance(maximumPressDistance);
    w->setShadowEffect(shadowOffset, shadowRadius);
    w->setShadowColor(shadowColor_);
    if (thumbnailWidth_ > 0) {
        w->setFixedWidth(thumbnailWidth_);
        imageLoader.load(w->filePath(), w->fixedSize(), w, &ThumbnailWidget::setThumbnail);
    }

    // Add it to scene, layout and list and increment current cell
    scene_->addItem(w);
    contentLayout_->addItem(w, currentCell_.row, currentCell_.column); // Takes longest!
    thumbnails_.append(w);
    ++currentCell_;
}

void GalleryView::onThumbnailGenerated(const QString &filePath, std::shared_ptr<QPixmap> thumbnail)
{
    // Set thumbnail for ThumbnailWidget
    // No mutex needed since this is connected with Qt::QueuedConnection
    for (ThumbnailWidget *w : std::as_const(thumbnails_))
        if (w->filePath() == filePath)
            w->setThumbnail(thumbnail);
}

void GalleryView::onThumbnailPressed(ThumbnailWidget *thumbnail)
{
    // Remember animation duration
    static const int animationDuration = CFG.read<int>("gallery.animationDuration", 100);

    // Setup animation to go from thumbnail rect to fullscreen
    QStringList paths = imagePaths();
    QRect thumbnailRect = mapFromScene(thumbnail->geometry()).boundingRect();

    // Temporary set thumbnail image before bigger image is loaded async
    preview_->setImage(thumbnail->thumbnail());
    preview_->setImagePaths(paths);
    preview_->setCurrentImageIndex(paths.indexOf(thumbnail->filePath()));
    preview_->zoomFrom(thumbnailRect, animationDuration);
    preview_->show();
}

void GalleryView::onBackPressed()
{
    // Hide preview or go back to start view
    if (preview_->isVisible())
        preview_->hide();
    else emit backPressed();
}

void GalleryView::copyGalleryToUsb()
{
    // Check if a path exists
    if (usbDetector_->atleastOne()) {
        QFuture<int> future = QtConcurrent::run(copySourceToTarget, CFG.imageDirectory(), usbDetector_->drives().constLast().absolutePath());
        watcher_.setFuture(future);
    }
}

void GalleryView::mousePressEvent(QMouseEvent *event)
{
    // Notify scroller and send event to parent, so thumbnail widgets can receive it
    scroller_->press(event->pos().y());
    QGraphicsView::mousePressEvent(event);
}

void GalleryView::mouseMoveEvent(QMouseEvent *event)
{
    // Notify scroller and send event to parent, so thumbnail widgets can receive it
    scroller_->move(event->pos().y());
    QGraphicsView::mouseMoveEvent(event);
}

void GalleryView::mouseReleaseEvent(QMouseEvent *event)
{
    // Notify scroller and send event to parent, so thumbnail widgets can receive it
    scroller_->release(event->pos().y());
    QGraphicsView::mouseReleaseEvent(event);
}

void GalleryView::resizeEvent(QResizeEvent *)
{
    // Resize preview, floating buttons and thumbnails
    preview_->setGeometry(rect());
    buttons_->setGeometry(rect());
    resizeThumbnails(width());
}

Cell::Cell() :
    Cell(0, 0, 4)
{
}

Cell::Cell(int row, int column) :
    Cell(row, column, 4)
{
}

Cell::Cell(int row, int column, int columnCount) :
    row(row),
    column(column),
    columnCount(columnCount)
{
}

Cell &Cell::operator++()
{
    // Prefix increment cell
    if (++column >= columnCount) {
        column = 0;
        ++row;
    }
    return *this;
}

Cell &Cell::operator--()
{
    // Prefix decrement cell
    if (--column < 0) {
        if (--row < 0) {
            column = 0;
            row = 0;
        } else column = columnCount - 1;
    }
    return *this;
}

void copySourceToTarget(QPromise<int> &promise, const QString &sourceFolder, const QString &targetFolder)
{
    // Get source and target directories
    QDir source(sourceFolder);
    QDir target(targetFolder);
    if (source.exists() && target.exists()) {
        target.mkdir(source.dirName());
        target.cd(source.dirName());

        // Create a list of <source path, target path>
        QString sourcePath(source.absolutePath() + "/");
        QString targetPath(target.absolutePath() + "/");
        QStringList images = source.entryList(QStringList() << "IMG*.jpg" << "IMG*.JPG", QDir::Files);

        // Setup promise
        int count = images.count();
        fkInfo("Copying " + QString::number(count) + " images to " + targetPath);
        promise.setProgressRange(0, count);
        promise.start();

        // Copy files and report progress
        for (int i = 0; i < count; ++i) {
            QString file = images.at(i);
            if (!QFile::copy(sourcePath + file, targetPath + file))
                fkWarning("Failed to copy file " + file + "!");
            promise.setProgressValue(i + 1);
        }

#ifdef __linux__
        // Sync the filesystem to ensure all data is written
        if (system("sync") != 0)
            fkWarning("Failed to sync filesystem - eject manually!");
#endif
        promise.finish();
        fkInfo("Finished copying files to USB drive");
    } else fkError("Failed to copy to USB drive: Path doesn't exist!");
}
