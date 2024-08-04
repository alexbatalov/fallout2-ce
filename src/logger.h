#ifndef FALLOUT_LOGGER_H
#define FALLOUT_LOGGER_H

#include <stdio.h>
#include <time.h>
#include <cerrno>

namespace fallout {

class Logger {
public:
    static Logger& getInstance();
    void redirectStdio();
    void logWithTimestamp(const char* format, ...);

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    FILE* logFile = nullptr;
};

} // namespace fallout

#endif // FALLOUT_LOGGER_H
