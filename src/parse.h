#ifndef PARSE_H
#define PARSE_H

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "log.h"
#include "io.h"

#define BUFSIZE 8193

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
    HTML, CSS, JPEG, PNG, GIF, OTHER,
}MIMEType;

typedef enum {
    GET, HEAD, POST, NOT_SUPPORT
} method_t;

typedef struct _hdNode{
    char* key;
    char* value;
    struct _hdNode* prev;
    struct _hdNode* next;
} hdNode;

typedef struct _conn_node{
    int connfd;
    char* addr;
    struct _conn_node* prev;
    struct _conn_node* next;
}conn_node;

typedef struct _cgi_node{
    int connfd;
    int pid;
    conn_node* connNode;
    struct _cgi_node* prev;
    struct _cgi_node* next;
}cgi_node;

typedef struct {
    bool error;
    bool ishttps;
    bool conn_close;
    bool keepAlive;
    method_t method;
    status_t status;
    off_t content_len;
    bool isCGI;
    char *path;
    char uri[BUFSIZE];
    char header[BUFSIZE];
    char page[BUFSIZE];
    char* postbody;
    char* addr;
    MIMEType filetype;
    time_t last_md;
    hdNode* hdhead;
    hdNode* hdtail;
    hdNode* envphead;
    hdNode* envptail;
    int hdlineNum;
    int postlen;
    cgi_node* cgiNode;
} response_t;



int parseRequest(conn_node* node, response_t *resp);

void responseinit(response_t *resp);

void inserthdNode(response_t* resp, hdNode *newNode);

char* getValueByKey(hdNode* head, char *key);

hdNode* newNode(char* key, char *value);

#endif