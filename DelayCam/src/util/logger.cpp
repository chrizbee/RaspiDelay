#include "logger.h"

#include <QDateTime>

#define TIMESTAMP QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")

Logger *Logger::instance_ = nullptr;

Logger::Logger() :
    level_(LogLevel::WARNING),
    file_(nullptr),
    cmdstream_(nullptr),
    filestream_(nullptr)
{
}

Logger *Logger::instance()
{
    // Create a new logger if there is none
    if (instance_ == nullptr)
        instance_ = new Logger();
    return instance_;
}

bool Logger::init(LogLevel level, const QString &filepath)
{
    // Initialize logger
    if (file_ == nullptr) {
        level_ = level;
        file_ = new QFile(filepath);
        cmdstream_ = new QTextStream(stdout);
        filestream_ = new QTextStream(file_);
        return file_->open(QFile::WriteOnly | QFile::Truncate);
    } else return false;
}

void Logger::close()
{
    // Close logger
    file_->close();
    delete cmdstream_;
    delete filestream_;
    delete file_;
    delete instance_;
}

void Logger::log(
    const char *fileInfo,
    int lineInfo,
    LogLevel level,
    const QString &message,
    const char *bgnd,
    const char *fgnd)
{
    // Log only if level is big enough
    if (level >= level_) {
        QString timestamp = TIMESTAMP;
        QString lvlstr = lvl2str(level);

        // Log to command line
        *cmdstream_
            << WHT  << timestamp << WHT << " "
            << bgnd << lvlstr    << WHT << " "
            << WHT  << fileInfo  << WHT << " ["
            << MGT  << lineInfo  << WHT << "]: "
            << fgnd << message   << RST << Qt::endl;

        // Log to file
        *filestream_
            << timestamp << lvlstr
            << fileInfo  << " ["
            << lineInfo  << "]: "
            << message   << Qt::endl;
    }
}

QString lvl2str(LogLevel level)
{
    // Get string representation of level
    switch (level) {
    case LogLevel::TRACE   : return " TRACE   "; break;
    case LogLevel::DEBUG   : return " DEBUG   "; break;
    case LogLevel::INFO    : return " INFO    "; break;
    case LogLevel::WARNING : return " WARNING "; break;
    case LogLevel::CRITICAL: return " ERROR   "; break;
    default: return " LEVEL   "; break;
    }
}
