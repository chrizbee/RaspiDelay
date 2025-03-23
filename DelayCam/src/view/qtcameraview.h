#ifndef QTCAMERAVIEW_H
#define QTCAMERAVIEW_H

#include "cameraview.h"
#include <QMediaDevices>
#include <QCamera>
#include <QVideoWidget>
#include <QMediaCaptureSession>
#include <QImageCapture>

class QtCameraView : public CameraView
{
    Q_OBJECT

public:
    QtCameraView(QWidget *parent = nullptr);
    virtual ~QtCameraView();
    bool initCamera() override;
    void startCamera() override;
    void capturePicture() override;
    void autoFocus() override;
    void manualFocus(const QPointF &focus) override;
    void stopCamera() override;
    void releaseCamera() override;

protected:
    void onCameraDevicesChanged();

private:
    QMediaDevices devices_;
    QCamera *camera_;
    QVideoWidget *viewFinder_;
    QMediaCaptureSession *captureSession_;
    QImageCapture *capturer_;
};

#endif // QTCAMERAVIEW_H
