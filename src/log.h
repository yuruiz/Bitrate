#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

void loginit(char *logfile);
void logging(float duration, float tput, float avg_tput, int bitrate, char *client_ip, char *chunkname);
void dns_loginit(char *logfile);
void dns_logging(struct sockaddr_in *addr, char *qname, char *res);
//int getlogfd();
#endif