#include "application.h"
#include "cam/image.h"
#include "cam/viewfinder.h"
#include "util/logger.h"

#include <assert.h>
#include <iomanip>
#include <string>

#include <QCoreApplication>
#include <QMutexLocker>
#include <QStringList>
#include <QScreen>

using namespace libcamera;

class CaptureEvent : public QEvent
{
public:
    CaptureEvent() :
        QEvent(type()) {
    }

    static Type type() {
        static int type = QEvent::registerEventType();
        return static_cast<Type>(type);
    }
};

Application::Application(int &argc, char **argv) :
    QApplication{argc, argv},
    isCapturing_(false),
    afTriggered_(true),
    frameRate_(30.0)
{
    // Create and connect to viewfinder
    viewFinder_ = new ViewFinder(nullptr);
    connect(viewFinder_, &ViewFinder::renderComplete, this, &Application::queueRequest);

    // Initialize and start camera
    if (initCamera())
        startCamera();

    // Show stream fullscreen
    // Simply showFullScreen is not working properly so we have to set geometry first
    viewFinder_->setGeometry(QGuiApplication::primaryScreen()->geometry());
    viewFinder_->showFullScreen();
}

Application::~Application()
{
    // Release camera resources
    if (camera_) {
        stopCamera();
        camera_->release();
        camera_.reset();
    }

    // Stop camera manager
    if (cm_)
        cm_->stop();

    // Delete viewfinder
    delete viewFinder_;
}

bool Application::initCamera()
{
    // Create and start camera manager
    cm_ = std::make_unique<CameraManager>();
    if (cm_->start()) {
        fkError("Failed to start camera manager!");
        return false;
    }

    // Get camera from manager
    if (cm_->cameras().empty()) {
        fkWarning("No camera found!");
        return false;
    }

    // Acquire camera
    // Reset camera pointer if failed, so camera == nullptr
    camera_ = cm_->cameras().front();
    if (camera_->acquire()) {
        fkWarning("Failed to acquire camera!");
        camera_.reset();
        return false;
    } else {
        QString name = QString::fromStdString(*camera_->properties().get(libcamera::properties::Model));
        fkInfo("Using camera" + name);
    }
    return true;
}

void Application::startCamera()
{
    stopCamera();
    configureCamera();
}

void Application::autoFocus()
{
    fkInfo("Autofocus triggered");
    afTriggered_ = true;
}

void Application::manualFocus(const QPointF &focus)
{
    // Setting a point in the resulting frame that needs focus is not implemented
    // We could set the lens position like this:
    // request->controls().set(controls::AfMode, controls::AfModeManual);
    // request->controls().set(controls::LensPosition, 0.5);
    // See: https://www.libcamera.org/api-html/namespacelibcamera_1_1controls.html#acfb623408035e51db904a0f3928a863e
    fkWarning("Manual focus is not implemented yet!");
    (void)focus;
}

void Application::stopCamera()
{
    // Stop camera if capturing
    if (!isCapturing_)
        return;
    isCapturing_ = false;
    camera_->stop();
    camera_->requestCompleted.disconnect(this);

    // Clear buffers and queues
    mappedBuffers_.clear();
    requests_.clear();
    freeQueue_.clear();
    allocator_.reset();
    config_.reset();
    freeBuffers_.clear();
    doneQueue_.clear();
    viewFinder_->stop();
}

void Application::releaseCamera()
{
    stopCamera();
}

bool Application::event(QEvent *e)
{
    // Handle capture events
    if (e->type() == CaptureEvent::type()) {
        processCaptureEvent();
        return true;
    } else return QApplication::event(e);
}

bool Application::configureCamera()
{
    // Check if camera is acquried
    if (!camera_) {
        fkWarning("Initialize camera before configuration!");
        return false;
    }

    // Generate viewfinder configuration
    config_ = camera_->generateConfiguration({ StreamRole::Viewfinder });
    if (!config_ || config_->empty()) {
        fkWarning("Failed to generate camera configuration!");
        return false;
    }

    // Raspberry Pi Camera v3: 1536x864 2304x1296 4608x2592
    QSize s(1536, 864);

    // Set orientation
    config_->orientation = libcamera::Orientation::Rotate0;

    // Edit configuration
    fkInfo("Using size " + QString::number(s.width()) + "x" + QString::number(s.height()));
    StreamConfiguration &cfg = config_->at(0);
    cfg.size.width = s.width();
    cfg.size.height = s.height();
    cfg.bufferCount = 4;

    // Use a format supported by the viewfinder
    libcamera::PixelFormat format = libcamera::formats::YUV420;
    auto camFormats = cfg.formats().pixelformats();
    if (std::find(camFormats.begin(), camFormats.end(), format) != camFormats.end())
        cfg.pixelFormat = format;
    else {
        fkWarning("Format not supported! Use one of:");
        for (auto &format : camFormats)
            fkInfo(format.toString().c_str());
    }

    // Setting fixed exposure times will disable the AE algorithm
    // https://libcamera.org/api-html/namespacelibcamera_1_1controls.html#a4e1ca45653b62cd969d4d67a741076eb
    //
    // Setting fixed frame times will limit the AE algorithm
    // https://libcamera.org/api-html/namespacelibcamera_1_1controls.html#a4f3236ff99d40a3a44fcd1ad77c4458f
    //
    // Digital gains will be applied to the image captured by the sensor
    // https://libcamera.org/api-html/namespacelibcamera_1_1controls.html#a82c8beb7cf9d9f048c5007a68922a5b1
    //
    // Setting fixed analogue gains will limit the AE algorithm
    // https://libcamera.org/api-html/namespacelibcamera_1_1controls.html#ab34ebeaa9cbfb3f3fc6996b089ca52b0
    //
    // Setting the AE mode is most flexible
    // https://libcamera.org/api-html/namespacelibcamera_1_1controls.html#acc370d05c5efc0b92f2fe285a1227426

    // Set auto exposure mode
    // controls_.set(controls::AeExposureMode, controls::AeExposureModeEnum::ExposureNormal); // -Short, -Long, -Custom

    // Set frametime (min, max) [us] and thus framerate
    int64_t minFt = 1e6 / frameRate_; // 30fps -> 33333,33us
    int64_t maxFt = 1e6 / frameRate_; // 30fps -> 33333,33us
    controls_.set(controls::FrameDurationLimits, Span<const int64_t, 2>({ minFt, maxFt }));

    // Validate configuration
    CameraConfiguration::Status validation = config_->validate();
    if (validation == CameraConfiguration::Adjusted) {
        fkInfo(QString("Stream configuration adjusted to ") + cfg.toString().c_str());
    } else if (validation == CameraConfiguration::Invalid) {
        fkWarning("Failed to create valid camera configuration!");
        return false;
    }

    // Configure camera
    if (camera_->configure(config_.get()) < 0) {
        fkInfo("Failed to configure camera!");
        return false;
    }

    // Store stream allocation pointer
    stream_ = config_->at(0).stream();

    // Configure the viewfinder
    const StreamConfiguration &vfConfig = config_->at(0);
    viewFinder_->setFormat(
        vfConfig.pixelFormat,
        QSize(vfConfig.size.width, vfConfig.size.height),
        vfConfig.stride);

    // Allocate and map buffers
    allocator_ = std::make_unique<FrameBufferAllocator>(camera_);
    for (StreamConfiguration &c : *config_) {
        Stream *stream = c.stream();
        if (allocator_->allocate(stream) < 0) {
            fkWarning("Failed to allocate capture buffers!");
            goto error;
        }

        // Map memory buffers and cache the mappings
        for (const std::unique_ptr<FrameBuffer> &buffer : allocator_->buffers(stream)) {
            std::unique_ptr<Image> image = Image::fromFrameBuffer(buffer.get(), Image::MapMode::ReadOnly);
            assert(image != nullptr);
            mappedBuffers_[buffer.get()] = std::move(image);

            // Store buffers on the free list
            freeBuffers_[stream].enqueue(buffer.get());
        }
    }

    // Create requests and fill them with buffers from the viewfinder
    while (!freeBuffers_[stream_].isEmpty()) {
        FrameBuffer *buffer = freeBuffers_[stream_].dequeue();
        std::unique_ptr<Request> request = camera_->createRequest();
        if (!request) {
            fkWarning("Can't create request!");
            goto error;
        }
        if (request->addBuffer(stream_, buffer) < 0) {
            fkWarning("Can't set buffer for request!");
            goto error;
        }
        requests_.push_back(std::move(request));
    }

    // Start the camera
    if (camera_->start(&controls_)) {
        fkWarning("Failed to start capture!");
        goto error;
    }

    // Connect callback
    camera_->requestCompleted.connect(this, &Application::requestComplete);

    // Queue all requests
    for (std::unique_ptr<Request> &request : requests_) {
        if (camera_->queueRequest(request.get()) < 0) {
            fkWarning("Can't queue request!");
            goto error_disconnect;
        }
    }

    isCapturing_ = true;
    return true;

error_disconnect:
    camera_->requestCompleted.disconnect(this);
    camera_->stop();

error:
    requests_.clear();
    mappedBuffers_.clear();
    freeBuffers_.clear();
    allocator_.reset();
    return false;
}

void Application::requestComplete(libcamera::Request *request)
{
    // Check if not cancelled
    if (request->status() == Request::RequestCancelled)
        return;

    // This function is called by libcamera thread context where
    // expensive operations are not allowed. This is why we just add
    // the buffer to the done queue and post an event to be handled
    {
        QMutexLocker locker(&mutex_);
        doneQueue_.enqueue(request);
    }
    QCoreApplication::postEvent(this, new CaptureEvent);
}

void Application::processCaptureEvent()
{
    // Retrieve the next buffer from the done queue. The queue may be empty
    // if stopCapture() has been called while a CaptureEvent was posted but
    // not processed yet. Return immediately in that case.
    Request *request;
    {
        QMutexLocker locker(&mutex_);
        if (!doneQueue_.isEmpty())
            request = doneQueue_.dequeue();
        else return;
    }

    // Get buffer and process it
    if (request->buffers().count(stream_)) {
        ControlList &metadata = request->metadata();
        FrameBuffer *buffer = request->buffers().at(stream_);
        Image *imageBuffer = mappedBuffers_[buffer].get();
        viewFinder_->render(buffer, imageBuffer);
    }

    // Put request from done to free queue
    // The request will be re-queued after the viewfinder has rendered the buffer
    request->reuse();
    QMutexLocker locker(&mutex_);
    freeQueue_.enqueue(request);
}

void Application::queueRequest(libcamera::FrameBuffer *buffer)
{
    // Get request from free queue
    Request *request;
    {
        QMutexLocker locker(&mutex_);
        if (freeQueue_.isEmpty())
            return;
        request = freeQueue_.dequeue();
    }

    // Set autofocus if triggered
    if (afTriggered_) {
        request->controls().set(controls::AfMode, controls::AfModeAuto);
        request->controls().set(controls::AfTrigger, 0);
        afTriggered_ = false;
    }

    // Add buffer and queue request
    request->addBuffer(stream_, buffer);
    camera_->queueRequest(request);
}
