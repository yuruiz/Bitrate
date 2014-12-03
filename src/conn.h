#ifndef CONN_H
#define CONN_H

#include <stdio.h>
#include <arpa/inet.h>
#include "queue.h"

#define MAXLINE 8192
#define MAXBUF 8192
#define MINLINE 200

typedef enum{
    HEADER, PAYLOAD
} conn_status_t;

typedef enum {
    false,true
} bool;

typedef enum  {
    MANIFEST, VIDEO, OTHER,
}reqtype_t;

typedef enum {
    GET, HEAD, POST, NOT_SUPPORT
} method_t;

typedef struct {
    reqtype_t reqtype;
    int bitrate;
    char chunkname[MINLINE];
    long long timeStamp;
}req_t;

typedef struct{
    conn_status_t curStatus;
    int contentlen;
    ssize_t hdsize;
    int rec_len;
    char buf[MAXLINE];
    char* content;
} res_status;

typedef struct {
    method_t method;
    reqtype_t reqtype;
    int bitrate;
    int seg;
    int frag;
    int reqlen;
    int firstlen;
    char* resloc;
    char uri[MAXLINE];
    char version[MAXLINE];
    char buf[MAXLINE];
//    char response[MAXLINE];
    bool connclose;
} req_status;

typedef struct _conn_node{
    int clientfd;
    int serverfd;
    char* clientaddr;
    char serveraddr[MINLINE];
    res_status response_status;
    req_status request_status;
    queue_t* reqq;
    struct _conn_node* prev;
    struct _conn_node* next;
}conn_node;

typedef struct {
    int maxfd;            // record the file descriptor with highest index
    fd_set read_set;      // the set prepared for select()
    fd_set ready_set;     // the set that actually set as select() parameter
    int nconn;            // the number of connection ready to read
    int ndp;              // the (mas index of descriptor stored in pool) - 1
    conn_node*list_head;
    conn_node*list_tail;
} pool;


void init_pool(int http_fd, pool *p);

int add_conn(int connfd, pool *p, struct sockaddr_in* cli_addr);

void conn_handle(pool *p);

#endif