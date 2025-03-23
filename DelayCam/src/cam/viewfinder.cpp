#include "cam/viewfinder.h"
#include "cam/image.h"
#include "util/logger.h"

#include <assert.h>

#include <QByteArray>
#include <QFile>

//#define DEBUG_FPS

static const QList<libcamera::PixelFormat> supportedFormats {
    // YUV - packed (single plane)
    libcamera::formats::UYVY, // *
    libcamera::formats::VYUY, // *
    libcamera::formats::YUYV, // *
    libcamera::formats::YVYU, // *
    // YUV - semi planar (two planes)
    libcamera::formats::NV12, // *
    libcamera::formats::NV21, // *
    libcamera::formats::NV16,
    libcamera::formats::NV61,
    libcamera::formats::NV24,
    libcamera::formats::NV42,
    // YUV - fully planar (three planes)
    libcamera::formats::YUV420, // *
    libcamera::formats::YVU420, // *
    // RGB
    libcamera::formats::ABGR8888,
    libcamera::formats::ARGB8888,
    libcamera::formats::BGRA8888,
    libcamera::formats::RGBA8888,
    libcamera::formats::BGR888, // *
    libcamera::formats::RGB888, // *
    // * = Supported on ArduCAM 64mp
    // Also 24bit RGB formats (*888) will run very sluggish!
};

ViewFinder::ViewFinder(QWidget *parent) :
    QOpenGLWidget(parent),
    image_(nullptr),
    buffer_(nullptr),
    vertexShaderFile_(":identity.vert"),
    vertexBuffer_(QOpenGLBuffer::VertexBuffer)
{
}

ViewFinder::~ViewFinder()
{
    stop();
    removeShader();

    // TODO: There is no OpenGL context anymore here
    // So -> QOpenGLTexturePrivate::destroy() called without a current context
    // https://bugreports.qt.io/browse/AUTOSUITE-220
}

void ViewFinder::setFormat(const libcamera::PixelFormat &format, const QSize &size, uint stride)
{
    // Check if format is new
    if (format != format_) {

        // Remove and create new fragment if it already exists
        if (shaderProgram_.isLinked()) {
            shaderProgram_.release();
            shaderProgram_.removeShader(fragmentShader_.get());
            fragmentShader_.reset();
        }

        // Select new format
        if (selectFormat(format))
            format_ = format;
        else fkWarning(QString("Unsupported format") + format.toString().c_str() + "!");
    }

    // Set and update geometry
    size_ = size;
    stride_ = stride;
    updateGeometry();
}

void ViewFinder::render(libcamera::FrameBuffer *buffer, Image *image)
{
    // Calculate and print fps
#ifdef DEBUG_FPS
    static uint8_t counter = 0;
    static uint64_t lastBufferTime = 0;
    const libcamera::FrameMetadata &metadata = buffer->metadata();
    double fps = metadata.timestamp - lastBufferTime;
    fps = lastBufferTime && fps ? 1000000000.0 / fps : 0.0;
    lastBufferTime = metadata.timestamp;
    if (++counter % 10 == 0)
        fkDebug(QString::number(fps));
#endif

    // Release buffer
    if (buffer_)
        renderComplete(buffer);

    // Set image and repaint
    image_ = image;
    update();
    buffer_ = buffer;
}

void ViewFinder::stop()
{
    // Release buffer
    // renderComplete() is currently just connected to queueRequest()
    // to instantly request the next frame
    if (buffer_) {
        renderComplete(buffer_);
        buffer_ = nullptr;
        image_ = nullptr;
    }
}

void ViewFinder::initializeGL()
{
    // Initialize once before paintGL
    initializeOpenGLFunctions();
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);

    static const GLfloat coordinates[2][4][2]{
        {
            // Vertex coordinates
            { -1.0f, -1.0f },
            { -1.0f, +1.0f },
            { +1.0f, +1.0f },
            { +1.0f, -1.0f },
        },
        {
            // Texture coordinates
            { 0.0f, 1.0f },
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 1.0f, 1.0f },
        },
    };

    vertexBuffer_.create();
    vertexBuffer_.bind();
    vertexBuffer_.allocate(coordinates, sizeof(coordinates));

    // Create Vertex Shader
    if (!createVertexShader())
        fkWarning("Failed to create vertex shader!");

    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
}

void ViewFinder::paintGL()
{
    // Create fragment shader once
    if (!fragmentShader_)
        if (!createFragmentShader())
            fkWarning("Failed to create fragment shader!");

    // Render image
    if (image_) {
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        doRender();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}

void ViewFinder::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

QSize ViewFinder::sizeHint() const
{
    return size_.isValid() ? size_ : QSize(640, 480);
}

bool ViewFinder::selectFormat(const libcamera::PixelFormat &format)
{
    // Default values
    textureMinMagFilters_ = GL_LINEAR;
    vertexShaderFile_ = ":identity.vert";
    fragmentShaderDefines_.clear();

    // Setup the chosen format
    switch (format) {
    case libcamera::formats::NV12:
        horzSubSample_ = 2;
        vertSubSample_ = 2;
        fragmentShaderDefines_.append("#define YUV_PATTERN_UV");
        fragmentShaderFile_ = ":YUV_2_planes.frag";
        break;
    case libcamera::formats::NV21:
        horzSubSample_ = 2;
        vertSubSample_ = 2;
        fragmentShaderDefines_.append("#define YUV_PATTERN_VU");
        fragmentShaderFile_ = ":YUV_2_planes.frag";
        break;
    case libcamera::formats::NV16:
        horzSubSample_ = 2;
        vertSubSample_ = 1;
        fragmentShaderDefines_.append("#define YUV_PATTERN_UV");
        fragmentShaderFile_ = ":YUV_2_planes.frag";
        break;
    case libcamera::formats::NV61:
        horzSubSample_ = 2;
        vertSubSample_ = 1;
        fragmentShaderDefines_.append("#define YUV_PATTERN_VU");
        fragmentShaderFile_ = ":YUV_2_planes.frag";
        break;
    case libcamera::formats::NV24:
        horzSubSample_ = 1;
        vertSubSample_ = 1;
        fragmentShaderDefines_.append("#define YUV_PATTERN_UV");
        fragmentShaderFile_ = ":YUV_2_planes.frag";
        break;
    case libcamera::formats::NV42:
        horzSubSample_ = 1;
        vertSubSample_ = 1;
        fragmentShaderDefines_.append("#define YUV_PATTERN_VU");
        fragmentShaderFile_ = ":YUV_2_planes.frag";
        break;
    case libcamera::formats::YUV420:
        horzSubSample_ = 2;
        vertSubSample_ = 2;
        fragmentShaderFile_ = ":YUV_3_planes.frag";
        break;
    case libcamera::formats::YVU420:
        horzSubSample_ = 2;
        vertSubSample_ = 2;
        fragmentShaderFile_ = ":YUV_3_planes.frag";
        break;
    case libcamera::formats::UYVY:
        fragmentShaderDefines_.append("#define YUV_PATTERN_UYVY");
        fragmentShaderFile_ = ":YUV_packed.frag";
        break;
    case libcamera::formats::VYUY:
        fragmentShaderDefines_.append("#define YUV_PATTERN_VYUY");
        fragmentShaderFile_ = ":YUV_packed.frag";
        break;
    case libcamera::formats::YUYV:
        fragmentShaderDefines_.append("#define YUV_PATTERN_YUYV");
        fragmentShaderFile_ = ":YUV_packed.frag";
        break;
    case libcamera::formats::YVYU:
        fragmentShaderDefines_.append("#define YUV_PATTERN_YVYU");
        fragmentShaderFile_ = ":YUV_packed.frag";
        break;
    case libcamera::formats::ABGR8888:
        fragmentShaderDefines_.append("#define RGB_PATTERN rgb");
        fragmentShaderFile_ = ":RGB.frag";
        break;
    case libcamera::formats::ARGB8888:
        fragmentShaderDefines_.append("#define RGB_PATTERN bgr");
        fragmentShaderFile_ = ":RGB.frag";
        break;
    case libcamera::formats::BGRA8888:
        fragmentShaderDefines_.append("#define RGB_PATTERN gba");
        fragmentShaderFile_ = ":RGB.frag";
        break;
    case libcamera::formats::RGBA8888:
        fragmentShaderDefines_.append("#define RGB_PATTERN abg");
        fragmentShaderFile_ = ":RGB.frag";
        break;
    case libcamera::formats::BGR888:
        fragmentShaderDefines_.append("#define RGB_PATTERN rgb");
        fragmentShaderFile_ = ":RGB.frag";
        break;
    case libcamera::formats::RGB888:
        fragmentShaderDefines_.append("#define RGB_PATTERN bgr");
        fragmentShaderFile_ = ":RGB.frag";
        break;
    default:
        return false;
    };

    return true;
}

void ViewFinder::configureTexture(QOpenGLTexture &texture)
{
    glBindTexture(GL_TEXTURE_2D, texture.textureId());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureMinMagFilters_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureMinMagFilters_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool ViewFinder::createFragmentShader()
{
    // Create fragment shader
    fragmentShader_ = std::make_unique<QOpenGLShader>(QOpenGLShader::Fragment, this);

    // Load fragment shader from file
    QFile file(fragmentShaderFile_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fkWarning(fragmentShaderFile_ + "not found!");
        return false;
    }

    // Prepend #define macros stored in fragmentShaderDefines_ to source code
    QString defines = fragmentShaderDefines_.join('\n') + "\n";
    QByteArray src = file.readAll();
    src.prepend(defines.toUtf8());

    // Compile fragment shader
    if (!fragmentShader_->compileSourceCode(src)) {
        fkWarning(fragmentShader_->log());
        return false;
    }

    // Add and link shader
    shaderProgram_.addShader(fragmentShader_.get());
    if (!shaderProgram_.link()) {
        fkWarning(shaderProgram_.log());
        close();
    }

    // Bind shader pipeline for use
    if (!shaderProgram_.bind()) {
        fkWarning(shaderProgram_.log());
        close();
    }

    // Set attributes of vertex and textures
    int attributeVertex = shaderProgram_.attributeLocation("vertexIn");
    int attributeTexture = shaderProgram_.attributeLocation("textureIn");
    shaderProgram_.enableAttributeArray(attributeVertex);
    shaderProgram_.setAttributeBuffer(attributeVertex, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    shaderProgram_.enableAttributeArray(attributeTexture);
    shaderProgram_.setAttributeBuffer(attributeTexture, GL_FLOAT, 8 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));
    textureUniformY_ = shaderProgram_.uniformLocation("tex_y");
    textureUniformU_ = shaderProgram_.uniformLocation("tex_u");
    textureUniformV_ = shaderProgram_.uniformLocation("tex_v");
    textureUniformStep_ = shaderProgram_.uniformLocation("tex_step");
    textureUniformStrideFactor_ = shaderProgram_.uniformLocation("stride_factor");

    // Create the textures
    for (std::unique_ptr<QOpenGLTexture> &texture : textures_) {
        if (texture)
            continue;

        texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        texture->create();
    }

    return true;
}

bool ViewFinder::createVertexShader()
{
    // Create and compile vertex shader
    vertexShader_ = std::make_unique<QOpenGLShader>(QOpenGLShader::Vertex, this);
    if (!vertexShader_->compileSourceFile(vertexShaderFile_)) {
        fkWarning(vertexShader_->log());
        return false;
    }

    // Add shader if successful
    shaderProgram_.addShader(vertexShader_.get());
    return true;
}

void ViewFinder::removeShader()
{
    // Release and remove shaders
    if (shaderProgram_.isLinked()) {
        shaderProgram_.release();
        shaderProgram_.removeAllShaders();
    }
}

void ViewFinder::doRender()
{
    // Stride of the first plane, in pixels
    unsigned int stridePixels;

    switch (format_) {
    case libcamera::formats::NV12:
    case libcamera::formats::NV21:
    case libcamera::formats::NV16:
    case libcamera::formats::NV61:
    case libcamera::formats::NV24:
    case libcamera::formats::NV42:
        // Activate texture Y
        glActiveTexture(GL_TEXTURE0);
        configureTexture(*textures_[0]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 stride_,
                 size_.height(),
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 image_->data(0).data());
        shaderProgram_.setUniformValue(textureUniformY_, 0);

        // Activate texture UV/VU
        glActiveTexture(GL_TEXTURE1);
        configureTexture(*textures_[1]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE_ALPHA,
                 stride_ / horzSubSample_,
                 size_.height() / vertSubSample_,
                 0,
                 GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE,
                 image_->data(1).data());
        shaderProgram_.setUniformValue(textureUniformU_, 1);

        stridePixels = stride_;
        break;

    case libcamera::formats::YUV420:
        // Activate texture Y
        glActiveTexture(GL_TEXTURE0);
        configureTexture(*textures_[0]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 stride_,
                 size_.height(),
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 image_->data(0).data());
        shaderProgram_.setUniformValue(textureUniformY_, 0);

        // Activate texture U
        glActiveTexture(GL_TEXTURE1);
        configureTexture(*textures_[1]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 stride_ / horzSubSample_,
                 size_.height() / vertSubSample_,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 image_->data(1).data());
        shaderProgram_.setUniformValue(textureUniformU_, 1);

        // Activate texture V
        glActiveTexture(GL_TEXTURE2);
        configureTexture(*textures_[2]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 stride_ / horzSubSample_,
                 size_.height() / vertSubSample_,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 image_->data(2).data());
        shaderProgram_.setUniformValue(textureUniformV_, 2);

        stridePixels = stride_;
        break;

    case libcamera::formats::YVU420:
        // Activate texture Y
        glActiveTexture(GL_TEXTURE0);
        configureTexture(*textures_[0]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 stride_,
                 size_.height(),
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 image_->data(0).data());
        shaderProgram_.setUniformValue(textureUniformY_, 0);

        // Activate texture V
        glActiveTexture(GL_TEXTURE2);
        configureTexture(*textures_[2]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 stride_ / horzSubSample_,
                 size_.height() / vertSubSample_,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 image_->data(1).data());
        shaderProgram_.setUniformValue(textureUniformV_, 2);

        // Activate texture U
        glActiveTexture(GL_TEXTURE1);
        configureTexture(*textures_[1]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 stride_ / horzSubSample_,
                 size_.height() / vertSubSample_,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 image_->data(2).data());
        shaderProgram_.setUniformValue(textureUniformU_, 1);

        stridePixels = stride_;
        break;

    case libcamera::formats::UYVY:
    case libcamera::formats::VYUY:
    case libcamera::formats::YUYV:
    case libcamera::formats::YVYU:
        // Packed YUV formats are stored in a RGBA texture to match the
        // OpenGL texel size with the 4 bytes repeating pattern in YUV.
        // The texture width is thus half of the image_ with.
        glActiveTexture(GL_TEXTURE0);
        configureTexture(*textures_[0]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 stride_ / 4,
                 size_.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 image_->data(0).data());
        shaderProgram_.setUniformValue(textureUniformY_, 0);

        // The shader needs the step between two texture pixels in the
        // horizontal direction, expressed in texture coordinate units
        // ([0, 1]). There are exactly width - 1 steps between the
        // leftmost and rightmost texels.
        shaderProgram_.setUniformValue(textureUniformStep_,
                           1.0f / (size_.width() / 2 - 1),
                           1.0f /* not used */);

        stridePixels = stride_ / 2;
        break;

    case libcamera::formats::ABGR8888:
    case libcamera::formats::ARGB8888:
    case libcamera::formats::BGRA8888:
    case libcamera::formats::RGBA8888:
        glActiveTexture(GL_TEXTURE0);
        configureTexture(*textures_[0]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 stride_ / 4,
                 size_.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 image_->data(0).data());
        shaderProgram_.setUniformValue(textureUniformY_, 0);

        stridePixels = stride_ / 4;
        break;

    case libcamera::formats::BGR888:
    case libcamera::formats::RGB888:
        glActiveTexture(GL_TEXTURE0);
        configureTexture(*textures_[0]);
        glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 stride_ / 3,
                 size_.height(),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 image_->data(0).data());
        shaderProgram_.setUniformValue(textureUniformY_, 0);

        stridePixels = stride_ / 3;
        break;

    default:
        stridePixels = size_.width();
        break;
    };

    // Compute the stride factor for the vertex shader, to map the horizontal
    // texture coordinate range [0.0, 1.0] to the active portion of the image.
    shaderProgram_.setUniformValue(textureUniformStrideFactor_,
        static_cast<float>(size_.width() - 1) / (stridePixels - 1));
}
