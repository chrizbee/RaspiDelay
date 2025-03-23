#ifndef CAMCONTROL_H
#define CAMCONTROL_H

#include "cameraview.h"

#include <memory>
#include <vector>
#include <atomic>

#include "util/undefkeywords.h"
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/controls.h>
#include <libcamera/control_ids.h>
#include <libcamera/property_ids.h>
#include <libcamera/framebuffer.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>

#include <QObject>
#include <QImage>
#include <QQueue>
#include <QMutex>

class Image;
class ViewFinder;

class LibCameraView : public CameraView
{
    Q_OBJECT

public:
    LibCameraView(QWidget *parent = nullptr);
    ~LibCameraView();

    bool initCamera() override;
    void startCamera() override;
    void capturePicture() override;
    void autoFocus() override;
    void manualFocus(const QPointF &focus) override;
    void stopCamera() override;
    void releaseCamera() override;
    bool event(QEvent *e) override;

private:
    bool configureCamera(bool stillCapture);
    bool start(bool isPreview);
    void requestComplete(libcamera::Request *request);
    void processCaptureEvent();
    void queueRequest(libcamera::FrameBuffer *buffer);

private:
    ViewFinder *viewFinder_;
    std::atomic_bool isCapturing_;
    std::atomic_bool stillCapture_;
    std::atomic_bool afTriggered_;

    // Camera manager, camera, config and allocator
    std::unique_ptr<libcamera::CameraManager> cm_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    libcamera::ControlList controls_;
    libcamera::Stream *stream_;

    // Buffers and requests
    std::map<libcamera::FrameBuffer *, std::unique_ptr<Image>> mappedBuffers_;
    std::map<const libcamera::Stream *, QQueue<libcamera::FrameBuffer *>> freeBuffers_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
    QQueue<libcamera::Request *> doneQueue_;
    QQueue<libcamera::Request *> freeQueue_;
    QMutex mutex_; // Protects freeBuffers_, doneQueue_, and freeQueue_
};

libcamera::Orientation orientationFromString(const QString &str);

#endif // CAMCONTROL_H
