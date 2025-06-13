#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>

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
#include <QTimer>
#include <QStackedWidget>

class Image;
class ViewFinder;
class ProgressWidget;
class FramePool;

class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int &argc, char **argv);
    ~Application();
    bool initCamera();
    bool startCamera();
    void stopCamera();
    void releaseCamera();
    bool event(QEvent *e) override;

private:
    void parseSettings();
    void parseCommandline();
    bool configureCamera();
    bool start(bool isPreview);
    void requestComplete(libcamera::Request *request);
    void processCaptureEvent();
    void queueRequest(libcamera::FrameBuffer *buffer);

private:
    QStackedWidget *window_;
    ProgressWidget *progressWidget_;
    ViewFinder *viewFinder_;
    QTimer autoFocusTimer_;
    std::atomic_bool isCapturing_;
    float frameRate_;
    float delaySeconds_;
    int buttonPin_;
    bool alwaysAutoFocus_;
    bool poolWasFull_;

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
    QMutex requestsMutex_; // Protects doneQueue_

    // Frame pool for storing frames to delay stream
    std::unique_ptr<FramePool> pool_;
};

#endif // APPLICATION_H
