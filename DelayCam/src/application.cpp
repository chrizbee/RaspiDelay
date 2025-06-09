#include "application.h"
#include "progresswidget.h"
#include "cam/image.h"
#include "cam/viewfinder.h"
#include "cam/framepool.h"
#include "util/logger.h"
#include "wiringPi.h"

#include <assert.h>
#include <iomanip>
#include <string>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QMutexLocker>
#include <QStringList>
#include <QSettings>
#include <QScreen>
#include <QDir>

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
    frameRate_(30.0),
    delaySeconds_(30.0),
    buttonPin_(17),
    alwaysAutoFocus_(false),
    poolWasFull_(false)
{
    // Set app info and parse command line arguments
    setOrganizationName("chrizbee");
    setOrganizationDomain("chrizbee.github.io");
    setApplicationName("DelayCam");
    setApplicationVersion(APP_VERSION);
    parseSettings();
    parseCommandline();
    dcInfo(QString("Using GPIO %1 and %2s delay @ %3fps, autofocus: %4").arg(buttonPin_).arg(delaySeconds_).arg(frameRate_).arg(alwaysAutoFocus_));

    // Create widgets
    QString title = QString("Stream Delay = %1s").arg(delaySeconds_);
    window_ = new QStackedWidget(nullptr);
    progressWidget_ = new ProgressWidget(title, nullptr);
    viewFinder_ = new ViewFinder(nullptr);

    // Add viewfinder and progress widget to window
    window_->addWidget(progressWidget_);
    window_->addWidget(viewFinder_);

    // Initialize and start camera
    if (initCamera())
        startCamera();

    // Initialize WiringPi and debounce timer
    wiringPiSetupGpio();
    pinMode(buttonPin_, INPUT);
    pullUpDnControl(buttonPin_, PUD_UP);
    autoFocusTimer_.setSingleShot(true);
    autoFocusTimer_.setInterval(3000); // 3s

    // Show fullscreen
    // Simply showFullScreen is not working properly so we have to set geometry first
    window_->setGeometry(QGuiApplication::primaryScreen()->geometry());
    window_->showFullScreen();
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

    // Delete the window
    delete window_;
}

bool Application::initCamera()
{
    // Create and start camera manager
    cm_ = std::make_unique<CameraManager>();
    if (cm_->start()) {
        dcError("Failed to start camera manager!");
        return false;
    }

    // Get camera from manager
    if (cm_->cameras().empty()) {
        dcWarning("No camera found!");
        return false;
    }

    // Acquire camera
    // Reset camera pointer if failed, so camera == nullptr
    camera_ = cm_->cameras().front();
    if (camera_->acquire()) {
        dcWarning("Failed to acquire camera!");
        camera_.reset();
        return false;
    } else {
        QString name = QString::fromStdString(*camera_->properties().get(libcamera::properties::Model));
        dcInfo("Using camera: " + name);
    }
    return true;
}

void Application::startCamera()
{
    stopCamera();
    configureCamera();
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
    allocator_.reset();
    config_.reset();
    freeBuffers_.clear();
    doneQueue_.clear();
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

void Application::parseSettings()
{
    // Get the config file path
    QString configPath = QDir::homePath() + "/.config/delaycam.cfg";

    // Check if config file exists
    if (!QFile::exists(configPath)) {
        dcInfo("Config file not found");
        return;
    }

    // Read settings with current values as defaults
    QSettings settings(configPath, QSettings::IniFormat);
    frameRate_ = settings.value("framerate", frameRate_).toFloat();
    delaySeconds_ = settings.value("delay", delaySeconds_).toFloat();
    buttonPin_ = settings.value("buttonpin", buttonPin_).toInt();
    alwaysAutoFocus_ = settings.value("autofocus", alwaysAutoFocus_).toBool();
}

void Application::parseCommandline()
{
    // Create parser
    QCommandLineParser parser;
    parser.setApplicationDescription("Delay camera stream by x seconds");
    parser.addHelpOption();
    parser.addVersionOption();

    // Add options
    QCommandLineOption frameRateOption(QStringList() << "f" << "framerate", "Framerate in fps",        "framerate");
    QCommandLineOption delayOption(    QStringList() << "d" << "delay",     "Stream delay in seconds", "delay");
    QCommandLineOption buttonPinOption(QStringList() << "b" << "buttonpin", "Button GPIO number",      "pin");
    QCommandLineOption autoFocusOption(QStringList() << "a" << "autofocus", "Enable auto focus");
    QList<QCommandLineOption> cmdOptions{frameRateOption, delayOption, buttonPinOption, autoFocusOption};
    parser.addOptions(cmdOptions);

    // Process the command line arguments
    parser.process(*this);

    // Check if there are any command line parameters
    bool anyOptionSet = false;
    for (const auto& option : cmdOptions) {
        if (parser.isSet(option)) {
            anyOptionSet = true;
            break;
        }
    }
    if (!anyOptionSet) {
        dcInfo("No command line parameters passed");
        return;
    }

    // Update member variables if options were provided
    if (parser.isSet(frameRateOption))
        frameRate_ = parser.value(frameRateOption).toFloat();
    if (parser.isSet(delayOption))
        delaySeconds_ = parser.value(delayOption).toFloat();
    if (parser.isSet(buttonPinOption))
        buttonPin_ = parser.value(buttonPinOption).toInt();
    if (parser.isSet(autoFocusOption))
        alwaysAutoFocus_ = true;
}

bool Application::configureCamera()
{
    // Check if camera is acquried
    if (!camera_) {
        dcWarning("Initialize camera before configuration!");
        return false;
    }

    // Generate viewfinder configuration
    config_ = camera_->generateConfiguration({ StreamRole::Viewfinder });
    if (!config_ || config_->empty()) {
        dcWarning("Failed to generate camera configuration!");
        return false;
    }

    // Raspberry Pi Camera v3: 1536x864 2304x1296 4608x2592
    // libcamera will automatically pick the next best size
    QSize size = QGuiApplication::primaryScreen()->size();

    // Set orientation
    config_->orientation = libcamera::Orientation::Rotate0;

    // Edit configuration
    dcInfo("Using size " + QString::number(size.width()) + "x" + QString::number(size.height()));
    StreamConfiguration &cfg = config_->at(0);
    cfg.size.width = size.width();
    cfg.size.height = size.height();
    cfg.bufferCount = 4;

    // Use a format supported by the viewfinder
    libcamera::PixelFormat format = libcamera::formats::YUV420;
    auto camFormats = cfg.formats().pixelformats();
    if (std::find(camFormats.begin(), camFormats.end(), format) != camFormats.end())
        cfg.pixelFormat = format;
    else {
        dcWarning("Format not supported! Use one of:");
        for (auto &format : camFormats)
            dcInfo(format.toString().c_str());
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
        dcInfo(QString("Stream configuration adjusted to ") + cfg.toString().c_str());
    } else if (validation == CameraConfiguration::Invalid) {
        dcWarning("Failed to create valid camera configuration!");
        return false;
    }

    // Configure camera
    if (camera_->configure(config_.get()) < 0) {
        dcInfo("Failed to configure camera!");
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
            dcWarning("Failed to allocate capture buffers!");
            goto error;
        }

        // Map memory buffers and cache the mappings
        for (const std::unique_ptr<FrameBuffer> &buffer : allocator_->buffers(stream)) {
            std::unique_ptr<Image> image = Image::fromFrameBuffer(buffer.get(), Image::MapMode::ReadOnly);
            assert(image != nullptr);

            // Create pool from first sample image
            if (pool_ == nullptr || pool_->capacity() == 0)
                pool_ = FramePool::create(*(image.get()), delaySeconds_, frameRate_);

            // Store buffers on the free list
            mappedBuffers_[buffer.get()] = std::move(image);
            freeBuffers_[stream].enqueue(buffer.get());
        }
    }

    // Create requests and fill them with buffers from the viewfinder
    while (!freeBuffers_[stream_].isEmpty()) {
        FrameBuffer *buffer = freeBuffers_[stream_].dequeue();
        std::unique_ptr<Request> request = camera_->createRequest();
        if (!request) {
            dcWarning("Can't create request!");
            goto error;
        }
        if (request->addBuffer(stream_, buffer) < 0) {
            dcWarning("Can't set buffer for request!");
            goto error;
        }
        requests_.push_back(std::move(request));
    }

    // Start the camera
    if (camera_->start(&controls_)) {
        dcWarning("Failed to start capture!");
        goto error;
    }

    // Connect callback
    camera_->requestCompleted.connect(this, &Application::requestComplete);

    // Queue all requests
    for (std::unique_ptr<Request> &request : requests_) {
        if (camera_->queueRequest(request.get()) < 0) {
            dcWarning("Can't queue request!");
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
        QMutexLocker locker(&requestsMutex_);
        doneQueue_.enqueue(request);
    }
    QCoreApplication::postEvent(this, new CaptureEvent);
}

void Application::processCaptureEvent()
{
    static bool firstFrame = true;

    // Retrieve the next buffer from the done queue. The queue may be empty
    // if stopCapture() has been called while a CaptureEvent was posted but
    // not processed yet. Return immediately in that case.
    Request *request;
    {
        QMutexLocker locker(&requestsMutex_);
        if (!doneQueue_.isEmpty())
            request = doneQueue_.dequeue();
        else return;
    }

    // Check for button and timer state
    // One can also check if af is still scanning, but I want some extra time
    // const ControlList &metadata = completedRequest->metadata();
    // if (metadata.contains(controls::AfState))
    // int afState = metadata.get(controls::AfState)
    bool buttonIsPressed = digitalRead(buttonPin_) == LOW;
    bool timerIsRunning = autoFocusTimer_.isActive();
    bool needRealtime = buttonIsPressed || timerIsRunning;

    // Get buffer and process it
    FrameBuffer *buffer = nullptr;
    if (request->buffers().count(stream_)) {
        buffer = request->buffers().at(stream_);
        Image *imageBuffer = mappedBuffers_[buffer].get();

        // Get oldest frame and copy current frame to pool
        const PooledFrame *currentFrame = pool_->storeFrame(*imageBuffer);
        const PooledFrame *oldestFrame = pool_->getOldestFrame();

        // Use current frame if realtime is needed
        const PooledFrame *renderFrame = needRealtime ? currentFrame : oldestFrame;

        // Render frame if pool is full
        if (pool_->isFull()) {

            // Switch to viewfinder if it just became full
            if (!poolWasFull_) {
                poolWasFull_ = true;
                window_->setCurrentIndex(1);
            }

            // Render selected frame
            viewFinder_->render(renderFrame);

        // Render progress if not full yet
        } else progressWidget_->setProgress(pool_->size(), pool_->capacity());
    }

    // Reuse request right away, since we already copied the frame
    request->reuse();

    // Set autofocus if triggered
    if (firstFrame || buttonIsPressed || alwaysAutoFocus_) {
        firstFrame = false;
        request->controls().set(controls::AfMode, controls::AfModeAuto);
        request->controls().set(controls::AfTrigger, 0);
        if (buttonIsPressed)
            autoFocusTimer_.start();
    }

    // Add buffer and queue request
    if (buffer != nullptr)
        request->addBuffer(stream_, buffer);
    camera_->queueRequest(request);
}
