#ifndef CONN_H
#define CONN_H

#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include "parse.h"
#include "response.h"

typedef struct {
    int maxfd;            // record the file descriptor with highest index
    fd_set read_set;      // the set prepared for select()
    fd_set ready_set;     // the set that actually set as select() parameter
    int nconn;            // the number of connection ready to read
    int ndp;              // the (mas index of descriptor stored in pool) - 1
//    int clientfd[FD_SETSIZE];  // store the file descriptor
    conn_node*list_head;
    conn_node*list_tail;
    cgi_node* cgi_head;
    cgi_node* cgi_tail;
} pool;

void init_pool(int http_fd, pool *p);

int add_conn(int connfd, pool *p, struct sockaddr_in* cli_addr);

int add_ssl(int connfd, pool *p, SSL_CTX *ssl_context, struct sockaddr_in* cli_addr);

void conn_handle(pool *p);

#endif