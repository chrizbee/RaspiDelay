#ifndef CSICAMVIEW_H
#define CSICAMVIEW_H

#include <array>
#include <memory>

#include "util/undefkeywords.h"
#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

class Image;

class ViewFinder : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    ViewFinder(QWidget *parent);
    ~ViewFinder();

private:
    friend class Application;
    void setFormat(const libcamera::PixelFormat &format, const QSize &size, uint stride);
    void render(libcamera::FrameBuffer *buffer, Image *image);
    void stop();

Q_SIGNALS:
    void renderComplete(libcamera::FrameBuffer *buffer);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    QSize sizeHint() const override;

private:
    bool selectFormat(const libcamera::PixelFormat &format);
    void configureTexture(QOpenGLTexture &texture);
    bool createFragmentShader();
    bool createVertexShader();
    void removeShader();
    void doRender();

private:
    // Sizes and buffers
    QSize size_;
    uint stride_;
    Image *image_;
    libcamera::PixelFormat format_;
    libcamera::FrameBuffer *buffer_;

    // Shaders
    QOpenGLShaderProgram shaderProgram_;
    std::unique_ptr<QOpenGLShader> vertexShader_;
    std::unique_ptr<QOpenGLShader> fragmentShader_;
    QString vertexShaderFile_;
    QString fragmentShaderFile_;
    QStringList fragmentShaderDefines_;

    // Vertex buffer and textures
    QOpenGLBuffer vertexBuffer_;
    std::array<std::unique_ptr<QOpenGLTexture>, 3> textures_;

    // Common texture parameters
    GLuint textureMinMagFilters_;

    // YUV texture parameters
    GLuint textureUniformU_;
    GLuint textureUniformV_;
    GLuint textureUniformY_;
    GLuint textureUniformStep_;
    GLuint textureUniformStrideFactor_;
    unsigned int horzSubSample_;
    unsigned int vertSubSample_;
};

#endif // CSICAMVIEW_H
