#include "application.h"
#include "util/logger.h"
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    // Initialize logger
    fkLogger->init(LogLevel::TRACE, "delaycam.log");
    fkInfo("Starting DelayCam");

    // Configure OpenGL ES 2.0 as the renderable type
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setMajorVersion(2);
    format.setMinorVersion(0);
    format.setProfile(QSurfaceFormat::NoProfile);
    QSurfaceFormat::setDefaultFormat(format);

    // Create and run application
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    Application a(argc, argv);
    return a.exec();
}
