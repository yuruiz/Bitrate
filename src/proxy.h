#ifndef PROXY_H
#define PROXY_H


typedef struct proxy {
    struct sockaddr_in myaddr;
    char *dns_ip;
    char *dns_port;
} proxy_t;

char* getfakeip();
char* getwwwip();
double getAlpha();

#endif