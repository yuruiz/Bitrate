#ifndef CONN_H
#define CONN_H

#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include "queue.h"

#define MAXLINE 8192
#define MAXBUF 8192
#define MINLINE 200

typedef enum {
    OK, NOT_FOUND, BAD_REQUEST, LENGTH_REQUIRED, INTERNAL_SERVER_ERROR, NOT_IMPLEMENTED, SERVICE_UNAVAILABLE, HTTP_VERSION_NOT_SUPPORTED
} status_t;

typedef enum {
    CONNECTION_CLOSE, CONNECTION_ALIVE, TIME, SERVER, CONTENT_LEN, CONTENT_TYP, LAST_MDY
} field_t;

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

typedef struct _conn_node{
    int clientfd;
    int serverfd;
    char* clientaddr;
    char serveraddr[MINLINE];
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
//    int clientfd[FD_SETSIZE];  // store the file descriptor
    conn_node*list_head;
    conn_node*list_tail;
} pool;


typedef struct {
    method_t method;
    reqtype_t reqtype;
    int bitrate;
    int seg;
    int frag;
    char* resloc;
    char uri[MAXLINE];
    char version[MAXLINE];
    char response[MAXLINE];
    bool connclose;

} req_status;

typedef struct{
    size_t contentlen;
    char buf[MAXLINE];
    char* content;
} res_status;

void init_pool(int http_fd, pool *p);

int add_conn(int connfd, pool *p, struct sockaddr_in* cli_addr);

void conn_handle(pool *p);

#endif