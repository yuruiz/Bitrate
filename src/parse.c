#include "parse.h"


static int parseUri(char *uri, char *page);

char *getValueByKey(hdNode *head, char *key) {
    hdNode *curNode = head;


    if (head == NULL) {
        logging("The getValueByKey receive a null head!\n");
        return NULL;
    }

    while (curNode != NULL) {
        if (strcmp(key, curNode->key) == 0) {
            return curNode->value;
        }
        curNode = curNode->next;
    }
    return NULL;
}

static int readline(conn_node *node, char *buf, int length) {
    return httpreadline(node->clientfd, buf, length);
}

static int readblock(conn_node *node, char *buf, int length) {

    return (int) read(node->clientfd, buf, length);
}

hdNode *newNode(char *key, char *value) {
    hdNode *node = malloc(sizeof(hdNode));
    node->key = key;
    node->value = value;
    node->prev = NULL;
    node->next = NULL;

    return node;
}

void inserthdNode(response_t *resp, hdNode *newNode) {
    if (resp->hdhead == NULL) {
        resp->hdhead = newNode;
        resp->hdtail = newNode;
    }
    else {
        resp->hdtail->next = newNode;
        newNode->prev = resp->hdtail;
        resp->hdtail = newNode;
        resp->hdtail->next = NULL;
    }
}

static bool isCGIreq(char *uri) {
    if (strstr(uri, "/cgi/") == uri) {
        return true;
    }

    return false;
}

static int parseUri(char *uri, char *page) {
    memset(page, 0, BUFSIZE);
    logging("Parsing Uri..............\n");

    if (uri[0] != '/') {
        logging("The uri %s is invalid\n", uri);
        return -1;
    }
    else {
        strcpy(page, uri);
    }

    if (!strcmp(page, "/")) {
        strcat(page, "index.html");
    }


    logging("The uri is %s\n", uri);
    logging("The parsed page is %s\n", page);

    return 0;
}

void responseinit(response_t *resp) {

    resp->method = GET;
    resp->ishttps = false;
    resp->keepAlive = false;
    resp->conn_close = false;
    resp->status = OK;
    resp->content_len = 0;
    resp->error = false;
    resp->isCGI = false;
    resp->path = NULL;
    resp->filetype = OTHER;
    resp->postbody = NULL;
    resp->hdlineNum = 0;
    resp->postlen = 0;
    resp->cgiNode = NULL;
    resp->hdhead = NULL;
    resp->hdtail = NULL;
    resp->envphead = NULL;
    resp->envptail = NULL;
    resp->addr = NULL;
    memset(resp->header, 0, BUFSIZE);
    memset(resp->uri, 0, BUFSIZE);
    memset(resp->page, 0, BUFSIZE);

    return;
}