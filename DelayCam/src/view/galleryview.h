#ifndef GALLERYVIEW_H
#define GALLERYVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsWidget>
#include <QGraphicsGridLayout>
#include <QStringList>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPromise>

class FloatingButtons;
class ThumbnailWidget;
class Scroller;
class Preview;
class UsbDetector;

struct Cell {
    Cell();
    Cell(int row, int column);
    Cell(int row, int column, int columnCount);
    Cell &operator++();
    Cell &operator--();
    int row;
    int column;
    int columnCount;
};

class GalleryView : public QGraphicsView
{
    Q_OBJECT

public:
    GalleryView(QWidget *parent = nullptr);
    ~GalleryView();
    void setImageDirectory(const QString &path);
    void addImage(const QString &path);
    void deleteLastImage();
    void resizeThumbnails(int galleryWidth);
    void setColors(const QColor &background, const QColor &shadow);
    int lastFileIndex() const;

Q_SIGNALS:
    void backPressed();

protected:
    void setupUi();
    QStringList imagePaths() const;
    void createThumbnailWidget(const QString &path);
    void onThumbnailGenerated(const QString &filePath, std::shared_ptr<QPixmap> thumbnail);
    void onThumbnailPressed(ThumbnailWidget *thumbnail);
    void onBackPressed();
    void copyGalleryToUsb();
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *) override;

private:
    QGraphicsScene *scene_;
    QGraphicsWidget *content_;
    QGraphicsGridLayout *contentLayout_;
    Cell currentCell_;
    Scroller *scroller_;

    QList<ThumbnailWidget*> thumbnails_;
    int thumbnailWidth_;
    int lastFileIndex_;

    Preview *preview_;
    FloatingButtons *buttons_;

    UsbDetector *usbDetector_;
    QFutureWatcher<int> watcher_;

    QColor shadowColor_;
};

void copySourceToTarget(QPromise<int> &promise, const QString &source, const QString &target);

#endif // GALLERYVIEW_H
