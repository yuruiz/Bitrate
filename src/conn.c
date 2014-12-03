#include <netdb.h>
#include <sys/time.h>
#include "conn.h"
#include "mydns.h"
#include "proxy.h"
#include "response.h"
#include "bitrate.h"

extern proxy_t proxy;
void init_pool(int http_fd, pool *p) {

    p->list_head = NULL;
    p->list_tail = NULL;


    FD_ZERO(&p->ready_set);
    FD_ZERO(&p->read_set);

    p->maxfd = http_fd;
    FD_SET(http_fd, &p->read_set);
}

void remove_node(conn_node *node, pool *p) {
    freequeue(node->reqq);
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    else {
        p->list_head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    else {
        p->list_tail = node->prev;
    }

    free(node);
}

void initReqStatus(req_status *reqStatus){
    reqStatus->frag = 0;
    reqStatus->seg = 0;
    reqStatus->bitrate = 0;
    reqStatus->method = NOT_SUPPORT;
    reqStatus->reqtype = OTHER;
    reqStatus->connclose = false;
    reqStatus->resloc = NULL;
    reqStatus->reqlen = 0;
    reqStatus->firstlen = 0;

    memset(reqStatus->uri, 0, MAXLINE);
    memset(reqStatus->version, 0, MAXLINE);
    memset(reqStatus->buf, 0, MAXLINE);
    return;
}

void initResStatus(res_status *resStatus){
    resStatus->contentlen = 0;
    resStatus->rec_len = 0;
    resStatus->curStatus = HEADER;
    resStatus->content = NULL;
    resStatus->hdsize = 0;

    memset(resStatus->buf, 0, MAXLINE);
}

int add_conn(int connfd, pool *p, struct sockaddr_in *cli_addr) {
    p->nconn--;

    if (p->ndp == FD_SETSIZE) {
        return -1;
    }

    p->ndp++;

    if (p->maxfd < connfd) {
        p->maxfd = connfd;
    }

    FD_SET(connfd, &p->read_set);

    conn_node *new_node = malloc(sizeof(conn_node));

    new_node->clientfd = connfd;
    new_node->serverfd = -1;
    new_node->clientaddr = inet_ntoa(cli_addr->sin_addr);
    new_node->prev = NULL;
    new_node->next = NULL;
    new_node->reqq = newqueue();

    initResStatus(&new_node->response_status);
    initReqStatus(&new_node->request_status);

    if (p->list_head == NULL) {
        p->list_head = new_node;
        p->list_tail = new_node;
    }
    else {
        p->list_tail->next = new_node;
        new_node->prev = p->list_tail;
        p->list_tail = new_node;
    }

    return 0;
}


int openserverfd(conn_node* node) {

    int server_fd;
    struct addrinfo *server_info;
    struct sockaddr_in addr;
    char *fake_ip, *www_ip;
    unsigned short rand_port = 0;

    fake_ip = getfakeip();
    www_ip = getwwwip();
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(fake_ip);
    addr.sin_port = htons(rand_port);


    if (www_ip == NULL) {
        if(init_mydns(proxy.dns_ip, atoi(proxy.dns_port)) < 0){
            fprintf(stderr, "Init DNS failed\n");
            return -1;
        }
        if (resolve("video.cs.cmu.edu", "8080", NULL, &server_info) != 0) {
            fprintf(stderr, "resolve failed\n");
            return -1;
        }
        if (server_info == NULL) {
            fprintf(stderr, "no such server\n");
            return -1;
        }
    } else {
        char *server_ip = www_ip;
        char *server_port = "8080";
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        if (getaddrinfo(server_ip, server_port, &hints, &server_info) != 0) {
            fprintf(stderr, "getaddrinfo error\n");
            return -1;
        }
    }

    if ((server_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) == -1) {
        fprintf(stderr, "Socket failed\n");
        return -1;
    }

    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(addr))) {
        fprintf(stderr, "Failed binding socket\n");
        return -1;
    }

    if (connect(server_fd, server_info->ai_addr, server_info->ai_addrlen)) {
        fprintf(stderr, "Connect failed\n");
        return -1;
    }

    struct sockaddr_in tmpaddr =  *((struct sockaddr_in*)server_info->ai_addr);
    char* serverIP = inet_ntoa(tmpaddr.sin_addr);
    memset(node->serveraddr, 0, MINLINE);
    strcpy(node->serveraddr, serverIP);

    /* Clean up */
    freeaddrinfo(server_info);

    return server_fd;
}

void closeConnection(conn_node* node, pool *p){
    close(node->clientfd);

    FD_CLR(node->clientfd, &p->read_set);
    if (node->serverfd > 0) {
        close(node->serverfd);
        FD_CLR(node->serverfd, &p->read_set);
    }
}

int processReq(conn_node *cur_node, pool *p) {

    char* buf = cur_node->request_status.buf;
    char linebuf[MAXLINE];
    int n;

//    printf("Start request processing\n");

    memset(linebuf, 0, MAXLINE);

    /* Read request line and headers */
    while ((n = httpreadline(cur_node->clientfd, linebuf, MAXLINE)) > 0) {
        cur_node->request_status.reqlen += n;

        if (cur_node->request_status.reqlen > MAXLINE) {
            printf("Request Header is too large to fit in buffer\n");
            return -1;
        }

        strcat(buf, linebuf);
        memset(linebuf, 0, MAXLINE);

        if (strstr(buf, "\r\n\r\n") != NULL) {
            break;
        }
    }

    if (n == -1) {
        return 0;
    }else if (n == 0) {
        return -1;
    }

//    printf("%s", buf);

    /*parse the request line*/
    if (parse_uri(buf, &cur_node->request_status) < 0) {
        printf("Parse the request error\n");
        printf("%s", buf);
        return -1;
    }

    /*create the server socket if it has not been created*/
    if (cur_node->serverfd < 0) {
        if ((cur_node->serverfd = openserverfd(cur_node)) <= 0) {
            printf("Create connection to server failed\n");
            return -1;
        }

        FD_SET(cur_node->serverfd, &p->read_set);
        if (cur_node->serverfd > p->maxfd) {
            p->maxfd = cur_node->serverfd;
        }
        printf("Adding server at fd %d\n", cur_node->serverfd);
    }

    /*send the request to server*/
    if (sendRequset(cur_node, &cur_node->request_status) < 0) {
        return -1;
    }

    initReqStatus(&cur_node->request_status);

//    printf("Sending request to server finished\n");

    return 0;


}

int processResp(conn_node *cur_node, pool *p) {

    res_status *resStatus = &cur_node->response_status;
    ssize_t n;

    if (resStatus->curStatus == HEADER) {
        printf("Start processing response header\n");
        /*Parse the response header*/
        if((resStatus->hdsize = parseServerHd(cur_node, resStatus)) <= 0) {

            if(resStatus->hdsize == 0){
                if (resStatus->content != NULL) {
                    free(resStatus->content);
                }
                printf("Connection Closed\n");
                return -1;
            }else {
                printf("unable to read\n");
                return 0;
            }
        }


        printf("Contentlen is %d\n", resStatus->contentlen);


        if (resStatus->contentlen > 0) {
            resStatus->curStatus = PAYLOAD;
        }else{
            write(cur_node->clientfd, resStatus->buf, resStatus->hdsize);
            initResStatus(resStatus);

            req_t *req = (req_t *) dequeue(cur_node->reqq);

            if(req!= NULL){
                free(req);
            }
        }

        printf("Processing response header finished\n");
    }
    else {


//        printf("Start processing response payload\n");

        size_t left = resStatus->contentlen - resStatus->rec_len;

        if (resStatus->contentlen <= resStatus->rec_len) {
            free(resStatus->content);
            return -1;
        }

        /*read the response content*/
        n = recv(cur_node->serverfd, resStatus->content + resStatus->rec_len, left, MSG_DONTWAIT);

        if (n == 0) {
            free(resStatus->content);
            printf("Read Response payload error %d\n", n);
            return -1;
        }else if (n == -1) {
            return 0;
        }

        resStatus->rec_len += n;

        if (resStatus->rec_len < resStatus->contentlen) {
            return 0;
        }


        /*parse the response content*/
        req_t *req = (req_t *) dequeue(cur_node->reqq);

        if (req == NULL) {
            printf("ERROR! reqest records empty!\n ");
            free(resStatus->content);
            return -1;
        }

        ssize_t writelen = 0;
        struct timeval curT;
        switch (req->reqtype) {
            case MANIFEST:
                printf("Manifest response received\n");
                break;
            case VIDEO:
//                printf("Video response received\n");
//                printf("The content length is %d\n", resStatus->contentlen);
                gettimeofday(&curT, NULL);
                updateBitrate(req->timeStamp, curT.tv_sec * 1000 + curT.tv_usec / 1000,
                        (int) resStatus->contentlen, req->bitrate, req->chunkname, cur_node->serveraddr);
            case OTHER:
                /*send the response header to client*/
                send(cur_node->clientfd, resStatus->buf, resStatus->hdsize, 0);
                /*Send the payload*/
                writelen = send(cur_node->clientfd, resStatus->content, resStatus->contentlen, 0);
                break;
            default:
                printf("Error! unknow request record\n");
                return -1;
        }

//        printf("Sending Response finished\n");


        if (req->reqtype != MANIFEST && writelen != resStatus->contentlen) {
            printf("Sending response to client error\n");
            free(req);
            free(resStatus->content);
            return -1;
        }

//        printf("Start cleaning\n");
        free(req);
        free(resStatus->content);

        initResStatus(resStatus);

        printf("Processing response payload finished\n");
    }

    return 0;
}

void conn_handle(pool *p) {
    conn_node *cur_node;
    cur_node = p->list_head;

//    printf("Start Connection Handling procedure\n");

    /*Handle the http connections*/
    while (cur_node != NULL && p->nconn > 0) {

//        printf("***********************************************************\n");

        if (cur_node->serverfd > 0 && FD_ISSET(cur_node->serverfd, &p->ready_set)) {
            p->nconn--;
            if (processResp(cur_node, p) < 0) {
                closeConnection(cur_node, p);
                conn_node *temp = cur_node;
                cur_node = cur_node->next;
                remove_node(temp, p);
                continue;
            }
        }

        if (FD_ISSET(cur_node->clientfd, &p->ready_set)) {
            p->nconn--;
            if (processReq(cur_node, p) < 0) {
                closeConnection(cur_node, p);
                conn_node *temp = cur_node;
                cur_node = cur_node->next;
                remove_node(temp, p);
                continue;
            }
        }

        cur_node = cur_node->next;
    }
}
