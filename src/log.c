#include <unistd.h>
#include <time.h>
#include "log.h"

static FILE *_logfd;

void loginit(char *logfile) {
    _logfd = fopen(logfile, "w");

    if (_logfd == NULL) {
        printf("log file initialization error\n");
        exit(EXIT_FAILURE);
    }
}
//
//void logging(const char *format, ...) {
//    va_list args;
//    va_start(args, format);
//    vfprintf(_logfd, format, args);
//    fflush(_logfd);
//    va_end(args);
//}

void logging(float duration, float tput, float avg_tput, int bitrate, char *client_ip, char *chunkname) {

    time_t timetmp = time(NULL);

    fprintf(_logfd, "%d %.3f %.3f %.3f %d %s %s\n", (int) timetmp, duration, tput, avg_tput, bitrate, client_ip, chunkname);
    fflush(_logfd);

}