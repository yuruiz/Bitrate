#ifndef _NAMESERVER_H
#define _NAMESERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#define SERVER_NAME "video.cs.cmu.edu"
#define MAX_NODE 1024
#define NAME_LEN 64 // include '\n'
#define LINE_LEN 1024

typedef struct net_node {
    char name[NAME_LEN];
    int is_server;
    int node_pos[MAX_NODE];
    int count;
    int version;
} net_node_t;

typedef struct dns_server {
    int rr; // RR counter from 0 to numServer - 1
    struct sockaddr_in addr;
    int sock;
    net_node_t nodes[MAX_NODE];
    int node_num;
} dns_server_t;

#endif