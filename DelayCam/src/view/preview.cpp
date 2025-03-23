#include "preview.h"
#include "util/imageloader.h"

#include <QPainter>
#include <QMouseEvent>

Preview::Preview(QWidget *parent) :
    QOpenGLWidget{parent},
    maximumSwipeTime_(0),
    minimumSwipeDistance_(0),
    waitForAnimation_(true)
{
    // Setup and connect animations
    zoomAnimation_.setTargetObject(this);
    zoomAnimation_.setPropertyName("geometry");
    swipeAnimation_.setStartValue(0.0);
    connect(&zoomAnimation_, &QVariantAnimation::finished, this, &Preview::onZoomAnimationFinished);
    connect(&swipeAnimation_, &QVariantAnimation::finished, this, &Preview::onSwipeAnimationFinished);
    connect(&swipeAnimation_, &QVariantAnimation::valueChanged, this, QOverload<>::of(&Preview::update));
}

void Preview::loadImage(const QString &path)
{
    // Load image (blocking)
    image_ = std::make_shared<QPixmap>(path);
    update();
}

void Preview::loadImageAsync(const QString &path)
{
    // Queue a new runnable to load the image
    imageLoader.load(path, size(), this, &Preview::setImage);
}

void Preview::loadNext()
{
    // Load next image
    if (currentImageIndex_ + 1 < static_cast<uint>(imagePaths_.size()))
        imageLoader.load(imagePaths_.at(currentImageIndex_ + 1), size(), this, &Preview::setImage);
}

void Preview::loadPrevious()
{
    // Load previous image
    if (currentImageIndex_ > 0)
        imageLoader.load(imagePaths_.at(currentImageIndex_ - 1), size(), this, &Preview::setImage);
}

void Preview::setImage(std::shared_ptr<QPixmap> image, const QString &path)
{
    // Get some flags
    uint pathCount = imagePaths_.size();
    bool isCurrent = path.isEmpty() || imagePaths_.isEmpty();
    bool maybeCurrent = currentImageIndex_ < pathCount;
    bool maybeNext = currentImageIndex_ + 1 < pathCount;
    bool maybePrevious = maybeCurrent && (currentImageIndex_ > 0);
    bool isZooming = zoomAnimation_.state() == QVariantAnimation::Running;

    // Set image and update
    if (isCurrent || (maybeCurrent && path == imagePaths_.at(currentImageIndex_))) {
        if (isZooming && waitForAnimation_)
            loadingImage_ = image;
        else image_ = image;

    // Set previous image
    } else if (maybePrevious && path == imagePaths_.at(currentImageIndex_ - 1))
        neighbours_.first = image;

    // Set next image
    else if (maybeNext && path == imagePaths_.at(currentImageIndex_ + 1))
        neighbours_.second = image;

    // Repaint preview
    update();
}

void Preview::setImagePaths(const QStringList &imagePaths)
{
    // Set image paths to load images for swiping
    imagePaths_ = imagePaths;
}

void Preview::setCurrentImageIndex(uint index)
{
    // Load specific image from path
    if (index >= 0 && index < static_cast<uint>(imagePaths_.size())) {
        currentImageIndex_ = index;
        loadImageAsync(imagePaths_.at(index));
        loadPrevious();
        loadNext();
    }
}

void Preview::previousImage()
{
    // Load previous image path
    if (currentImageIndex_ > 0)
        startSwipeAnimation(false);
}

void Preview::nextImage()
{
    // Load next image path
    if (currentImageIndex_ + 1 < static_cast<uint>(imagePaths_.size()))
        startSwipeAnimation(true);
}

void Preview::zoomFrom(QRect from, int duration)
{
    // Zoom in from rect to full size
    // Also remove any previous turn points = key values
    zoomAnimation_.setDuration(duration);
    zoomAnimation_.setKeyValues(QVariantAnimation::KeyValues());
    zoomAnimation_.setStartValue(from);
    zoomAnimation_.setEndValue(geometry());
    zoomAnimation_.start();
}

void Preview::zoomAndReverse(double percent, int duration)
{
    // Calculate rect to turn animation
    QRect startEnd = geometry();
    QRect turn(QPoint(), startEnd.size() * percent);
    turn.moveCenter(startEnd.center());

    // Zoom from full size to turn rect and back to full size
    zoomAnimation_.setDuration(duration);
    zoomAnimation_.setStartValue(startEnd);
    zoomAnimation_.setKeyValueAt(0.5, turn);
    zoomAnimation_.setEndValue(startEnd);
    zoomAnimation_.start();
}

void Preview::setupSwipe(double maximumSwipeTime, int minimumSwipeDistance)
{
    maximumSwipeTime_ = maximumSwipeTime;
    minimumSwipeDistance_ = minimumSwipeDistance;
}

void Preview::startSwipeAnimation(bool next)
{
    // Start the swipe animation to previous or next
    if (swipeAnimation_.state() != QAbstractAnimation::Running) {
        swipeAnimation_.setEndValue(next ? -1.0 : +1.0);
        swipeAnimation_.start();
    }
}

void Preview::onZoomAnimationFinished()
{
    // Set preloaded image and emit signal
    if (loadingImage_ != nullptr) {
        image_ = loadingImage_;
        loadingImage_.reset();
        update();
    }
    emit animationFinished();
}

void Preview::onSwipeAnimationFinished()
{
    // Check if to previous or to next
    double endValue = swipeAnimation_.endValue().toDouble();
    bool toPrevious = endValue == +1.0;
    bool toNext = endValue == -1.0;

    // Swap image pointers and preload new image
    if (toPrevious) {
        --currentImageIndex_;
        neighbours_.second = image_;
        image_ = neighbours_.first;
        loadPrevious();
    } else if (toNext) {
        ++currentImageIndex_;
        neighbours_.first = image_;
        image_ = neighbours_.second;
        loadNext();
    }
    update();
}

void Preview::setSwipeAnimationDuration(int duration)
{
    swipeAnimation_.setDuration(duration);
}

void Preview::setWaitForAnimation(bool waitForAnimation)
{
    waitForAnimation_ = waitForAnimation;
}

void Preview::setGeometry(const QRect &g)
{
    // Stop zooming if resized externally
    zoomAnimation_.stop();
    QOpenGLWidget::setGeometry(g);
}

void Preview::setGeometry(int x, int y, int w, int h)
{
    Preview::setGeometry(QRect(x, y, w, h));
}

void Preview::mousePressEvent(QMouseEvent *event)
{
    // Start timer and remember position
    if (minimumSwipeDistance_ > 0 && maximumSwipeTime_ > 0) {
        swipeStartPos_ = event->pos();
        timer_.start();
    }
}

void Preview::mouseReleaseEvent(QMouseEvent *event)
{
    // Check for distance and time
    QPoint diff = event->pos() - swipeStartPos_;
    bool horizontal = abs(diff.x()) > abs(diff.y());
    bool right = horizontal && diff.x() < -minimumSwipeDistance_;
    bool left = horizontal && diff.x() > minimumSwipeDistance_;

    // Go to previous or next image if it was in time
    if (timer_.elapsed() < maximumSwipeTime_ * 1000) {
        if (left)
            previousImage();
        else if (right)
            nextImage();
    }

    // Stop timer
    timer_.invalidate();
}

void Preview::paintEvent(QPaintEvent *)
{
    // Initialize painter
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // Adjust drawing rect for animation
    bool swipeRunning = swipeAnimation_.state() == QAbstractAnimation::Running;
    double swipeFactor = swipeRunning ? swipeAnimation_.currentValue().toDouble() : 0.0;
    double endFactor = swipeAnimation_.endValue().toDouble();
    QRect drawingRect = rect();
    drawingRect.translate(swipeFactor * width(), 0);
    QRect otherRect = drawingRect.translated(-1.0 * endFactor * width(), 0);

    // Draw image if valid
    if (image_ && !image_->isNull()) {
        painter.drawPixmap(drawingRect, *image_);

    // Draw loading indicator
    } else {
        auto iconFont = painter.font();
        iconFont.setPixelSize(rect().height() / 3);
        painter.setFont(iconFont);
        painter.setPen(Qt::darkGray);
        painter.fillRect(drawingRect, Qt::lightGray);
        painter.drawText(drawingRect, Qt::AlignCenter, "⌛");
    }

    // Draw next / previous
    auto other = endFactor == -1.0 ? neighbours_.second : neighbours_.first;
    if (swipeRunning && other && !other->isNull())
        painter.drawPixmap(otherRect, *other);
}
