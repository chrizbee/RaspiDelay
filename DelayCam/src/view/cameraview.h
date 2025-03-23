#ifndef CAMERAVIEW_H
#define CAMERAVIEW_H

#include <QWidget>
#include <QStackedWidget>
#include <QCameraFormat>
#include <QTimer>

class Preview;
class FloatingButtons;
class CountdownWidget;

class CameraView : public QWidget
{
    Q_OBJECT

public:
    CameraView(QWidget *parent = nullptr);
    void configure(uint frameRate, const QSize &frameSize);
    void init();
    void start();
    void stop();
    void focusOnce();
    void releaseAndRetry();
    void displayViewFinder();
    void displayPreview();
    void startCountdown();
    bool focusing() const;
    bool showingPreview() const;
    bool isInitialized() const;
    bool focusOnCapture() const;
    QString imagePath() const;
    int fileNameCounter() const;
    void setFocusOnCapture(bool focusOnCapture);
    void setFileNameCounter(int counter);

Q_SIGNALS:
    void enableLeds(bool enable);
    void captured(const QString &path);
    void deleteLastPressed();
    void backPressed();

protected:
    void setupUi();
    void addViewFinder(QWidget *viewFinder);
    void onBackPressed();
    void onImageSaved(const QString &path);
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    virtual bool initCamera() = 0;
    virtual void startCamera() = 0;
    virtual void capturePicture() = 0;
    virtual void autoFocus() = 0;
    virtual void manualFocus(const QPointF &focus) = 0;
    virtual void stopCamera() = 0;
    virtual void releaseCamera() = 0;

    uint frameRate_;
    QSize frameSize_;
    int fileNameCounter_;
    bool isInitialized_;
    bool focusOnCapture_;

private:
    FloatingButtons *buttons_;
    QStackedWidget *stackedWidget_;
    CountdownWidget *countdownWidget_;
    Preview *preview_;
    QTimer previewTimer_;
};

QSize smallestResolution(const QList<QSize> &sizes);
QSize nearestResolution(const QList<QSize> &sizes, const QSize &ref);
QCameraFormat nearestFormat(const QList<QCameraFormat> &formats, const QSize &ref);

#endif // CAMERAVIEW_H
