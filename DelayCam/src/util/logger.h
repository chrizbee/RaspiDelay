#ifndef LOGGER_H
#define LOGGER_H

#include <QFile>
#include <QTextStream>

#define RST  "\033[0m"			/*!< ANSI escape sequence to reset. */
#define BLK  "\033[0;30m"		/*!< ANSI escape sequence for black foreground. */
#define RED  "\033[0;31m"		/*!< ANSI escape sequence for red foreground. */
#define GRN  "\033[0;32m"		/*!< ANSI escape sequence for green foreground. */
#define YLW  "\033[0;33m"		/*!< ANSI escape sequence for yellow foreground. */
#define BLU  "\033[0;34m"		/*!< ANSI escape sequence for blue foreground. */
#define MGT  "\033[0;35m"		/*!< ANSI escape sequence for magenta foreground. */
#define CYN  "\033[0;36m"		/*!< ANSI escape sequence for cyan foreground. */
#define WHT  "\033[0;37m"		/*!< ANSI escape sequence for white foreground. */

#define BBLK "\033[0;37;40m"	/*!< ANSI escape sequence for black background. */
#define BRED "\033[0;30;41m"	/*!< ANSI escape sequence for red background. */
#define BGRN "\033[0;30;42m"	/*!< ANSI escape sequence for green background. */
#define BYLW "\033[0;30;43m"	/*!< ANSI escape sequence for yellow background. */
#define BBLU "\033[0;30;44m"	/*!< ANSI escape sequence for blue background. */
#define BMGT "\033[0;30;45m"	/*!< ANSI escape sequence for magenta background. */
#define BCYN "\033[0;30;46m"	/*!< ANSI escape sequence for cyan background. */
#define BWHT "\033[0;30;47m"	/*!< ANSI escape sequence for white background. */

#define fkLogger     Logger::instance()
#define fkTrace(x)   Logger::instance()->log(fileName(__FILE__), __LINE__, LogLevel::TRACE, x, BWHT, WHT)
#define fkDebug(x)   Logger::instance()->log(fileName(__FILE__), __LINE__, LogLevel::DEBUG, x, BCYN, CYN)
#define fkInfo(x)    Logger::instance()->log(fileName(__FILE__), __LINE__, LogLevel::INFO, x, BGRN, GRN)
#define fkWarning(x) Logger::instance()->log(fileName(__FILE__), __LINE__, LogLevel::WARNING, x, BYLW, YLW)
#define fkError(x)   Logger::instance()->log(fileName(__FILE__), __LINE__, LogLevel::CRITICAL, x, BRED, RED)

constexpr const char* fileName(const char* path) {
    const char* file = path;
    while (*path++)
        if (*path == '/' || *path == '\\')
            file = path + 1;
    return file;
}

enum class LogLevel : uint {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    CRITICAL
};

class Logger
{
    Q_DISABLE_COPY(Logger)

public:
    static Logger *instance();
    bool init(LogLevel level, const QString &filepath);
    void close();
    void log(
        const char *fileInfo,
        int lineInfo,
        LogLevel level,
        const QString &message,
        const char *bgnd,
        const char *fgnd
    );

private:
    static Logger *instance_;
    Logger();
    LogLevel level_;
    QFile *file_;
    QTextStream *cmdstream_;
    QTextStream *filestream_;
};

QString lvl2str(LogLevel level);

#endif // LOGGER_H
