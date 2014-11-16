#ifndef LOG_H
#define LOG_H

#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void loginit(char *logfile);
void logging(const char *format, ...);
//int getlogfd();
#endif