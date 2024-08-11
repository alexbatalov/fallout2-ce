#include "logger.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace fallout {

// just a debugger class for me to use
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::redirectStdio() {
#ifdef __SWITCH__
    fsInitialize();

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char logFileName[64];
    strftime(logFileName, sizeof(logFileName), "sdmc:/switch/fallout2/debug_%Y%m%d_%H%M%S.log", t);

    // Open log file
    logFile = fopen(logFileName, "w");
    if (logFile == NULL) {
        fprintf(stderr, "Failed to open log file for writing! Error: %s\n", strerror(errno));
        return;
    }

    // Redirect stdout and stderr to log file
    stdout = logFile;
    stderr = logFile;

    // Ensure log is written out immediately
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

#endif
}

void Logger::logWithTimestamp(const char* format, ...) {
    va_list args;
    va_start(args, format);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);

    printf("[%s] ", timeStr);
    vprintf(format, args);
    printf("\n");

    va_end(args);
    fflush(stdout);
}

} // namespace fallout
