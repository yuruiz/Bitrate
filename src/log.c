#include <unistd.h>
#include "log.h"

static FILE *_logfd;

void loginit(char *logfile) {
    _logfd = fopen(logfile, "w");

    if (_logfd == NULL) {
        printf("log file initialization error\n");
        exit(EXIT_FAILURE);
    }
}

void logging(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(_logfd, format, args);
    fflush(_logfd);
    va_end(args);
}

