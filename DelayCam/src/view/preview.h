#ifndef PREVIEW_H
#define PREVIEW_H

#include <QOpenGLWidget>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <QElapsedTimer>
#include <utility>
#include <memory>

typedef std::shared_ptr<QPixmap> PixmapPointer;
typedef std::pair<PixmapPointer, PixmapPointer> PixmapPointerPair;

class Preview : public QOpenGLWidget
{
    Q_OBJECT

public:
    Preview(QWidget *parent = nullptr);
    void loadImage(const QString &path);
    void loadImageAsync(const QString &path);
    void setImage(std::shared_ptr<QPixmap> image, const QString &path = "");
    void setImagePaths(const QStringList &imagePaths);
    void setCurrentImageIndex(uint index);
    void previousImage();
    void nextImage();
    void zoomFrom(QRect from, int duration = 100);
    void zoomAndReverse(double percent, int duration = 100);
    void setupSwipe(double maximumSwipeTime, int minimumSwipeDistance);
    void setSwipeAnimationDuration(int duration);
    void setWaitForAnimation(bool waitForAnimation);

    void setGeometry(const QRect &g);
    void setGeometry(int x, int y, int w, int h);

protected:
    void loadNext();
    void loadPrevious();
    void startSwipeAnimation(bool next);
    void onZoomAnimationFinished();
    void onSwipeAnimationFinished();
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *) override;

Q_SIGNALS:
    void animationFinished();

private:
    PixmapPointer image_;
    PixmapPointer loadingImage_;
    PixmapPointerPair neighbours_;
    QStringList imagePaths_;
    uint currentImageIndex_;

    QElapsedTimer timer_;
    QPoint swipeStartPos_;
    double maximumSwipeTime_;
    int minimumSwipeDistance_;

    QPropertyAnimation zoomAnimation_;
    QVariantAnimation swipeAnimation_;
    bool waitForAnimation_;
};

#endif // PREVIEW_H
