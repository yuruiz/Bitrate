#include <netdb.h>
#include <sndfile.h>
#include "conn.h"
#include "mydns.h"
#include "proxy.h"
#include "response.h"
#include "bitrate.h"


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


void initReqStatus(req_status *reqStatus){
    reqStatus->frag = 0;
    reqStatus->seg = 0;
    reqStatus->bitrate = 0;
    reqStatus->method = NOT_SUPPORT;
    reqStatus->reqtype = OTHER;
    reqStatus->connclose = false;
    reqStatus->resloc = NULL;

    memset(reqStatus->uri, 0, MAXLINE);
    memset(reqStatus->version, 0, MAXLINE);

    return;
}

void initResStatus(res_status *resStatus){
    resStatus->contentlen = 0;
    resStatus->content = NULL;

    memset(resStatus->buf, 0, MAXLINE);
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
//        if (resolve("video.cs.cmu.edu", "8080", NULL, &server_info) != 0) {
//            fprintf(stderr, "getaddrinfo error in proxy_process\n");
//            exit(-1);
//        }
        return -1;
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


int sendtoServer(conn_node *cur_node, pool *p) {
    char buf[MAXLINE];
    req_status reqStatus;

    printf("Start sending request to server\n");

    initReqStatus(&reqStatus);

    /* Read request line and headers */
    if (httpreadline(cur_node->clientfd, buf, MAXLINE) < 0) {
        printf("Cannot read content from ");
        return -1;
    }

    /*parse the request line*/
    if (parse_uri(buf, &reqStatus) < 0) {
        printf("Parse the request error\n");
        printf("%s\n", buf);
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
    if (sendRequset(cur_node, &reqStatus) < 0) {
        close(cur_node->clientfd);
        FD_CLR(cur_node->clientfd, &p->read_set);
        if (cur_node->serverfd > 0) {
            close(cur_node->serverfd);
            FD_CLR(cur_node->serverfd, &p->read_set);
        }

        return -1;
    }

    printf("Sending request to server finished\n");

    return 0;


}

int sendtoClient(conn_node *cur_node,  pool *p) {

    res_status resStatus;
    int hdsize = 0;

    printf("Start sending response to client\n");

    initResStatus(&resStatus);

    /*Parse the response header*/
    if((hdsize = parseServerHd(cur_node, &resStatus)) < 0) {
        if (resStatus.content != NULL) {
            free(resStatus.content);
            close(cur_node->clientfd);
            FD_CLR(cur_node->clientfd, &p->read_set);
            close(cur_node->serverfd);
            FD_CLR(cur_node->serverfd, &p->read_set);
            return -1;
        }
    }

    /*send the response header to client*/
    write(cur_node->clientfd, resStatus.buf, hdsize);

    /*read the response content*/
    if(write(cur_node->serverfd, resStatus.content, resStatus.contentlen) != resStatus.contentlen){
        free(resStatus.content);
        close(cur_node->clientfd);
        FD_CLR(cur_node->clientfd, &p->read_set);
        close(cur_node->serverfd);
        FD_CLR(cur_node->serverfd, &p->read_set);
        return -1;
    }

    /*parse the response content*/
    req_t* req = (req_t*)dequeue(cur_node->reqq);
    if (req == NULL) {
        printf("ERROR! reqest records empty!\n ");
        free(resStatus.content);
        close(cur_node->clientfd);
        FD_CLR(cur_node->clientfd, &p->read_set);
        close(cur_node->serverfd);
        FD_CLR(cur_node->serverfd, &p->read_set);
        return -1;
    }

    ssize_t writelen;
    struct timeval curT;
    switch (req->reqtype) {
        case MANIFEST:
            //todo update manifest
            break;
        case VIDEO:
            gettimeofday(&curT, NULL);
            updateBitrate(req->timeStamp, curT.tv_sec * 1000 + curT.tv_usec / 1000, (int)resStatus.contentlen, req->bitrate,req->chunkname, cur_node->serveraddr);
        case OTHER:
            writelen = write(cur_node->clientfd, resStatus.content, resStatus.contentlen);
            break;
        default:
            printf("Error! unknow request record\n");
            return -1;
    }

    if (req->reqtype != MANIFEST && writelen != resStatus.contentlen) {
        free(resStatus.content);
        close(cur_node->clientfd);
        FD_CLR(cur_node->clientfd, &p->read_set);
        close(cur_node->serverfd);
        FD_CLR(cur_node->serverfd, &p->read_set);
        return -1;
    }


    free(resStatus.content);

    return 0;

}

void conn_handle(pool *p) {
    conn_node *cur_node;
    cur_node = p->list_head;

    printf("Start Connection Handling procedure\n");

    /*Handle the http connections*/
    while (cur_node != NULL && p->nconn > 0) {

        if (cur_node->serverfd > 0 && FD_ISSET(cur_node->serverfd, &p->ready_set)) {
            p->nconn--;

            if (sendtoClient(cur_node, p) < 0) {
                conn_node *temp = cur_node;
                cur_node = cur_node->next;
                remove_node(temp, p);
                continue;
            }
        }

        if (FD_ISSET(cur_node->clientfd, &p->ready_set)) {
            p->nconn--;
            if (sendtoServer(cur_node, p) < 0) {
                conn_node *temp = cur_node;
                cur_node = cur_node->next;
                remove_node(temp, p);
                continue;
            }
        }
        cur_node = cur_node->next;
    }
}


