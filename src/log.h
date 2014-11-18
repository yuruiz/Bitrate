#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

void loginit(char *logfile);
void logging(float duration, float tput, float avg_tput, int bitrate, char *client_ip, char *chunkname);
//int getlogfd();
#endif