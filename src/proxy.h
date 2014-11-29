#ifndef PROXY_H
#define PROXY_H


typedef struct proxy {
    struct sockaddr_in myaddr;
    struct sockaddr_in toaddr;
} proxy_t;

char* getfakeip();
char* getwwwip();
double getAlpha();

#endif