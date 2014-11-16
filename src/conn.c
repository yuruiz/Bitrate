#include <netdb.h>
#include "conn.h"

#define MAXLINE 8192
#define MAXBUF 8192

void init_pool(int http_fd, pool *p) {

    p->list_head = NULL;
    p->list_tail = NULL;
    p->cgi_head = NULL;
    p->cgi_tail = NULL;

    FD_ZERO(&p->ready_set);
    FD_ZERO(&p->read_set);

    p->maxfd = http_fd;
    FD_SET(http_fd, &p->read_set);
}

void remove_node(conn_node *node, pool* p) {
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    else{
        p->list_head = node->next;
        if (node->next != NULL) {
            node->next->prev = NULL;
        }
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    else{
        p->list_tail = node->prev;
        if (node->prev != NULL) {
            node->prev->next = NULL;
        }
    }

    free(node);
}

void freelist(hdNode *head){
    hdNode* prev = head;
    hdNode* cur = head;

    while (cur != NULL) {
        cur = cur->next;
        free(prev);
        prev = cur;
    }
}

int add_conn(int connfd, pool *p, struct sockaddr_in* cli_addr) {
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

    new_node->connfd = connfd;
    new_node->isSSL = false;
    new_node->context = NULL;
    new_node->addr = inet_ntoa(cli_addr->sin_addr);
    new_node->prev = NULL;
    new_node->next = NULL;

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

void handleErr(int erro) {
    switch (erro) {
        case SSL_ERROR_NONE:
            logging("SSL_ERROR_NONE\n");
            break;
        case SSL_ERROR_ZERO_RETURN:
            logging("SSL_ERROR_ZERO_RETURN\n");
            break;
        case SSL_ERROR_WANT_READ:
            logging("SSL_ERROR_WANT_READ\n");
            break;
        case SSL_ERROR_WANT_CONNECT:
            logging("SSL_ERROR_WANT_CONNECT\n");
            break;
        case SSL_ERROR_WANT_X509_LOOKUP:
            logging("SSL_ERROR_WANT_X509_LOOKUP\n");
            break;
        case SSL_ERROR_SYSCALL:
            logging("SSL_ERROR_SYSCALL\n");
            break;
        case SSL_ERROR_SSL:
            logging("SSL_ERROR_SSL\n");
            break;
        default:
            logging("Unkown Error\n");
            break;
    }
}

int writecontent(conn_node* node, char* buf, size_t length) {
    if (node == NULL) {
        logging("Invalid connection node!\n");
        return -1;
    }
    return (int) write(node->connfd, buf, length);
}


static int parse_uri(char *uri, char *host, char *port, char *page)
{
    memset(host, 0, MAXLINE);
    memset(page, 0, MAXLINE);
    memset(port, 0, MAXLINE);

    int temport = 0;
    int status = 0;

    if (strstr(uri, "http://"))
    {
        if (sscanf(uri, "http://%8192[^:]:%i/%8192[^\n]", host, &temport, page) == 3)
        { status = 1;}
        else if (sscanf(uri, "http://%8192[^/]/%8192[^\n]", host, page) == 2)
        { status = 2;}
        else if (sscanf(uri, "http://%8192[^:]:%i[^\n]", host, &temport) == 2)
        { status = 3;}
        else if (sscanf(uri, "http://%8192[^/]", host) == 1)
        { status = 4;}
    }
    else
    {
        strcpy(page, uri);
    }

    if (temport != 0)
    {
        sprintf(port, "%d", temport);
    }
    else
    {
        strcpy(port, "80");
    }


    return status;
}

static void clienterror(conn_node* node, char *cause, char *errnum,
        char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    writecontent(node, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    writecontent(node, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    writecontent(node, buf, strlen(buf));
    writecontent(node, body, strlen(body));
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

    /* Walk the list, using each addrinfo to try to connect */
    for (p = addlist; p; p = p->ai_next) {
        if ((p->ai_family == AF_INET)) {
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

void conn_handle(pool *p) {
    conn_node *cur_node;
    cur_node = p->list_head;

    logging("Start Connection Handling procedure\n");

    /*Handle the http connections*/
    while (cur_node != NULL && p->nconn > 0) {

//        logging("Now in the http handling loop\n");
        if (FD_ISSET(cur_node->connfd, &p->ready_set)) {
            conn_node* tormNode = NULL;
            logging("--------------start handling http connection from at socket %d------------\n", cur_node->connfd);
            p->nconn--;

            int dest_fd, content_size = 0, object_size = 0;
            char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hdr[MAXLINE], dest_port[MAXLINE];
            char dest_host[MAXLINE], dest_page[MAXLINE], content[MAXLINE];

            /* Read request line and headers */

            if(httpreadline(cur_node->connfd, buf, MAXLINE) < 0)
            {
                printf("Cannot read content from ");
                cur_node = cur_node->next;
                continue;
            }
            sscanf(buf, "%s %s %s", method, uri, version);

            memset(hdr, 0, MAXLINE);
            /* Parse URI from GET request */

            if (parse_uri(uri, dest_host, dest_port, dest_page) < 0)
            {
                clienterror(cur_node, method, "404", "Host address wrong", "Something is wrong with the address");
                // printf("Thread %d: Thehost address wrong\n", (int)pthread_self());
            }

            if (strcasecmp(method, "GET"))
            {
                clienterror(cur_node, method, "501", "Not Implemented", "Tiny does not implement this method");
                // printf("Thread %d: TheMethod not implemented\n", (int)pthread_self());
                return;
            }

            sprintf(hdr, "%s /%s %s", method, dest_page, http_version);

            make_requesthdrs(&rio, hdr, dest_host);

            if ((dest_fd = open_clientfd_r(dest_host, dest_port)) <= 0)
            {
                return;
            }

            // printf("Thread %d: connection to %s success!\n", (int)pthread_self(), dest_host);
            write(dest_fd, hdr, strlen(hdr));

            // printf("The web content is as below: \n");
            memset(content, 0, MAXLINE);

            while (1)
            {
                int buf_size = 0;
                buf_size = httpreadline(dest_fd, content, MAXLINE);
                writecontent(cur_node, content, strlen(content));

                // printf("%s", content);
                object_size += buf_size;
                if (!strcmp(content, "\r\n"))
                {
                    break;
                }
            }

            while((content_size = read(dest_fd, content, MAXLINE)) > 0)
            {
                writecontent(cur_node, content, content_size);
                object_size += content_size;
            }



            logging("-------------------connection handling finished----------------------\n");
            continue;
        }
//        logging("At the end of http handling loop\n");
        cur_node = cur_node->next;
    }
}


