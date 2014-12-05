#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "log.h"

static FILE *_logfd;
static FILE *_dns_logfd;

void loginit(char *logfile) {
    _logfd = fopen(logfile, "w");

    if (_logfd == NULL) {
        printf("log file initialization error\n");
        exit(EXIT_FAILURE);
    }
}

void dns_loginit(char *logfile) {
    _dns_logfd = fopen(logfile, "w");

    if (_dns_logfd == NULL) {
        printf("dns log file initialization error\n");
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

void dns_logging(struct sockaddr_in *addr, const char *qname, const char *res) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    fprintf(_dns_logfd, "%lu %s %s %s\n",
            ((tv.tv_sec * 1000) + (tv.tv_usec / 1000)) / 1000,
            inet_ntoa(addr->sin_addr),
            qname,
            res);
    fflush(_dns_logfd);
    return;
}
