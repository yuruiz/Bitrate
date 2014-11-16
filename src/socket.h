#ifndef SOCKET_H
#define SOCKET_H

#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include "log.h"

int close_socket(int sock);
int open_port(int port, struct sockaddr_in * addr);
#endif