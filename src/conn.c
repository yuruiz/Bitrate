#include <netdb.h>
#include <sndfile.h>
#include "conn.h"

#define MAXLINE 8192
#define MAXBUF 8192

void init_pool(int http_fd, pool *p) {

    p->list_head = NULL;
    p->list_tail = NULL;
    p->cgi_head  = NULL;
    p->cgi_tail  = NULL;

    FD_ZERO(&p->ready_set);
    FD_ZERO(&p->read_set);

    p->maxfd = http_fd;
    FD_SET(http_fd, &p->read_set);
}

void remove_node(conn_node *node, pool *p) {
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    else {
        p->list_head = node->next;
        if (node->next != NULL) {
            node->next->prev = NULL;
        }
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    else {
        p->list_tail = node->prev;
        if (node->prev != NULL) {
            node->prev->next = NULL;
        }
    }

    free(node);
}

void freelist(hdNode *head) {
    hdNode *prev = head;
    hdNode *cur  = head;

    while (cur != NULL) {
        cur = cur->next;
        free(prev);
        prev = cur;
    }
}

int add_conn(int connfd, pool *p, struct sockaddr_in *cli_addr) {
    p->nconn--;

    if (p->ndp == FD_SETSIZE) {
        logging("add_conn error: Too many clients");
        return -1;
    }

    p->ndp++;

    if (p->maxfd < connfd) {
        p->maxfd = connfd;
    }

    FD_SET(connfd, &p->read_set);

    conn_node *new_node = malloc(sizeof(conn_node));

    new_node->clientfd   = connfd;
    new_node->serverfd   = -1;
    new_node->clientaddr = inet_ntoa(cli_addr->sin_addr);
    new_node->prev       = NULL;
    new_node->next       = NULL;

    if (p->list_head == NULL) {
        p->list_head = new_node;
        p->list_tail = new_node;
    }
    else {
        p->list_tail->next = new_node;
        new_node->prev     = p->list_tail;
        p->list_tail       = new_node;
    }

    return 0;
}

int open_clientfd_r(char *hostname, char *port) {
    int clientfd;
    struct addrinfo *addlist, *p;
    int rv;

    /* Create the socket descriptor */
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("cannot open the socket\n");
        return -1;
    }

    /* Get a list of addrinfo structs */
    if ((rv = getaddrinfo(hostname, port, NULL, &addlist)) != 0) {
        printf("%s %s\n", hostname, gai_strerror(rv));
        return -2;
    }

    //todo set fake ip

    /* Walk the list, using each addrinfo to try to connect */
    for (p = addlist; p; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            if (connect(clientfd, p->ai_addr, p->ai_addrlen) == 0) {
                break; /* success */
            }
        }
    }

    /* Clean up */
    freeaddrinfo(addlist);
    if (!p) { /* all connects failed */
        close(clientfd);
        return -1;
    }
    else { /* one of the connects succeeded */
        return clientfd;
    }
}


static int parse_uri(char *uri, char *host, char *port, char *path) {

    char tempath[MAXLINE];
    memset(tempath, 0, MAXLINE);
    memset(host, 0, MAXLINE);
    memset(path, 0, MAXLINE);
    memset(port, 0, MAXLINE);

    int temport = 0;
    int status  = 0;

    strcpy(path, "/");

    if (strstr(uri, "http://")) {
        if (sscanf(uri, "http://%8192[^:]:%i/%8192[^\n]", host, &temport, tempath) == 3) {status = 1;}
        else
            if (sscanf(uri, "http://%8192[^/]/%8192[^\n]", host, tempath) == 2) {status = 2;}
            else
                if (sscanf(uri, "http://%8192[^:]:%i[^\n]", host, &temport) == 2) {status = 3;}
                else if (sscanf(uri, "http://%8192[^/]", host) == 1) {status = 4;}

        strcat(path, tempath);
    }
    else {
        strcpy(path, uri);
    }

    if (temport != 0) {sprintf(port, "%d", temport);}
    else {strcpy(port, "80");}


    return status;
}


int sendtoServer(conn_node *cur_node, pool *p) {
    ssize_t object_size = 0;
    char    buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], dest_port[MAXLINE], dest_host[MAXLINE], path[MAXLINE];

    printf("Start sending request to server\n");
    /* Read request line and headers */
    if (httpreadline(cur_node->clientfd, buf, MAXLINE) < 0) {
        printf("Cannot read content from ");
        return -1;
    }

    memset(method, 0, MAXLINE);
    memset(path, 0, MAXLINE);
    memset(uri, 0, MAXLINE);
    memset(version, 0, MAXLINE);

    sscanf(buf, "%s %s %s", method, uri, version);

    printf("%s", buf);

    parse_uri(uri, dest_host, dest_port, path);

    if (cur_node->serverfd < 0) {
        if ((cur_node->serverfd = open_clientfd_r(dest_host, dest_port)) <= 0) {
            printf("Create connection to server failed\n");
            return -1;
        }

        FD_SET(cur_node->serverfd, &p->read_set);
        if (cur_node->serverfd > p->maxfd) {
            p->maxfd = cur_node->serverfd;
        }
        printf("Adding server port %s at fd %d\n", dest_port, cur_node->serverfd);
    }


    memset(buf, 0, MAXLINE);

    sprintf(buf, "%s %s %s\r\n", method, path, version);
    write(cur_node->serverfd, buf, strlen(buf));
    printf("%s", buf);

    memset(buf, 0, MAXLINE);

    int n;
    while ((n = httpreadline(cur_node->clientfd, buf, MAXLINE)) > 0) {
        write(cur_node->serverfd, buf, n);
        printf("%s", buf);
        if (!strcmp(buf, "\r\n")) {
            break;
        }
    }

    if (n == 0) {
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

void sendtoClient(conn_node *cur_node) {

    printf("Now send to client\n");
    ssize_t content_size = 0;
    char content[MAXLINE];
    memset(content, 0, MAXLINE);

    while ((content_size = recv(cur_node->serverfd, content, MAXLINE, MSG_DONTWAIT)) > 0) {
        write(cur_node->clientfd, content, content_size);
        printf("%s", content);
    }

}

void conn_handle(pool *p) {
    conn_node *cur_node;
    cur_node = p->list_head;

    printf("Start Connection Handling procedure\n");

    /*Handle the http connections*/
    while (cur_node != NULL && p->nconn > 0) {

        if (cur_node->serverfd > 0 && FD_ISSET(cur_node->serverfd, &p->ready_set)) {
            p->nconn--;
            sendtoClient(cur_node);
        }

        if (FD_ISSET(cur_node->clientfd, &p->ready_set)) {
            p->nconn--;
            if(sendtoServer(cur_node, p) < 0){
                conn_node* temp = cur_node;
                cur_node = cur_node->next;
                remove_node(temp, p);
            }
        }
        cur_node = cur_node->next;
    }
}


