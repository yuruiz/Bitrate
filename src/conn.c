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
//        printf("Start processing response header\n");
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

//        printf("Processing response header finished\n");
    }else {


//        printf("Start processing response payload\n");

        size_t left = resStatus->contentlen - resStatus->rec_len;

        /*read the response content*/
        if ((n = read(cur_node->serverfd, resStatus->content + resStatus->rec_len, left)) <= 0) {
            free(resStatus->content);
            printf("Read Response payload error %d\n", n);
            return -1;
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

//        printf("Processing response payload finished\n");
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

int initDNSRequest(dns_message_t *m, const char *name, void *encodedBuf) {
    char *tok, *str;
    char *saveptr = NULL;
    char len, buf[255];
    dns_req_t *qptr;

    // header
    memset(&m->header, 0, sizeof(dns_header_t));
    m->header.id = random() % (1 << 16); // choose id randomly
    m->header.qdcount = 1;

    // dns request
    qptr = &m->req;
    memset(qptr, 0, sizeof(dns_req_t));
    qptr->qtype = 1;
    qptr->qclass = 1;
    if (strlen(name) > 255) {
        fprintf(stderr, "error name length\n");
        return -1;
    }
    strncpy(buf, name, strlen(name));
    str = buf;

    // parse name, replace '.' with length, thread safety
    while ((tok = strtok_r(str, ".", &saveptr)) != NULL) {
        len = (char)strlen(tok);
        qptr->qname[qptr->qname_len] = len;
        strncpy(qptr->qname + qptr->qname_len+1, tok, len);
        qptr->qname_len += (len+1);
        str = NULL;
    }
    qptr->qname_len++;
    m->length = sizeof(dns_header_t) + qptr->qname_len + sizeof(qptr->qtype) + sizeof(qptr->qclass);

    // encode
    encode(m, encodedBuf);
    return m->length;
}

int initDNSResponse(dns_message_t *response, dns_message_t *req, char rcode, const char *res, void *encodedBuf) {
    dns_res_t *aptr;
    dns_req_t *qptr;
    size_t ap_len;
    size_t qp_len;

    // header
    memset(&response->header, 0, sizeof(dns_header_t));
    response->header.id = req->header.id;
    response->header.flag |= 1 << 15; // QA
    response->header.flag |= 1 << 10; // AA
    response->header.flag |= rcode;
    response->header.qdcount = 1;
    response->header.ancount = 1;

    // build response
    aptr = &response->res;
    qptr = &req->req;
    aptr->name_len = qptr->qname_len;
    memcpy(aptr->name, qptr->qname, aptr->name_len);
    aptr->type = 1;
    aptr->class = 1;
    aptr->rdlength = sizeof(aptr->rdata);
    inet_aton(res, &aptr->rdata);
    memcpy(&response->req, qptr, sizeof(*qptr));
    ap_len = aptr->name_len + sizeof(aptr->type) + sizeof(aptr->class) + sizeof(aptr->ttl) + sizeof(aptr->rdlength) + aptr->rdlength;
    qp_len = qptr->qname_len + sizeof(qptr->qtype) + sizeof(qptr->qclass);
    response->length = sizeof(dns_header_t) + ap_len + qp_len;

    // encode
    encode(response, encodedBuf);
    return response->length;
}

/**
* encode from host machine order to network order
*/
int encode(dns_message_t *m, void *encodedBuf) {
    void *offset;
    m->header.id = htons(m->header.id);
    m->header.flag = htons(m->header.flag);
    m->header.qdcount = htons(m->header.qdcount);
    m->header.ancount = htons(m->header.ancount);
    offset = memcpy(encodedBuf, &m->header, sizeof(m->header)) + sizeof(m->header);
    if (m->header.qdcount > 0) {
        m->req.qtype = htons(m->req.qtype);
        m->req.qclass = htons(m->req.qclass);
        offset = memcpy(offset, m->req.qname, m->req.qname_len)
                + m->req.qname_len;
        offset = memcpy(offset, &m->req.qtype, sizeof(m->req.qtype))
                + sizeof(m->req.qtype);
        offset = memcpy(offset, &m->req.qclass, sizeof(m->req.qclass))
                + sizeof(m->req.qclass);
    }
    if (m->header.ancount > 0) {
        m->res.type = htons(m->res.type);
        m->res.class = htons(m->res.class);
        m->res.ttl = htonl(m->res.ttl);
        m->res.rdlength = ntohs(m->res.rdlength);
        offset = memcpy(offset, m->res.name, m->res.name_len)
                + m->res.name_len;
        offset = memcpy(offset, &m->res.type, sizeof(m->res.type))
                + sizeof(m->res.type);
        offset = memcpy(offset, &m->res.class, sizeof(m->res.class))
                + sizeof(m->res.class);
        offset = memcpy(offset, &m->res.ttl, sizeof(m->res.ttl))
                + sizeof(m->res.ttl);
        offset = memcpy(offset, &m->res.rdlength, sizeof(m->res.rdlength))
                + sizeof(m->res.rdlength);
        offset = memcpy(offset, &m->res.rdata, sizeof(m->res.rdata))
                + sizeof(m->res.rdata);
    }
    return 0;
}

/**
* decode from network byte order to host machine byte order
*/
int decode(dns_message_t *m, void *buf, ssize_t len) {
    void *offset;
    char *i;
    char *j;
    memset(m, 0, sizeof(*m));
    m->length = len;
    memcpy(&m->header, buf, sizeof(m->header));
    m->header.id = ntohs(m->header.id);
    m->header.flag = ntohs(m->header.flag);
    m->header.qdcount = ntohs(m->header.qdcount);
    m->header.ancount = ntohs(m->header.ancount);
    if (m->header.qdcount > 1 || m->header.ancount > 1) {
        fprintf(stderr, "query number error\n");
        return -1;
    }
    offset = buf + sizeof(m->header);
    if (m->header.qdcount > 0) {
        m->req.qname_len = 1;
        for (i = offset; (*i) != 0; i++) {
            m->req.qname_len++;
        }
        memcpy(&m->req.qname, offset, m->req.qname_len);
        offset += m->req.qname_len;
        memcpy(&m->req.qtype, offset, sizeof(m->req.qtype));
        m->req.qtype = ntohs(m->req.qtype);
        offset += sizeof(m->req.qtype);
        memcpy(&m->req.qclass, offset, sizeof(m->req.qclass));
        m->req.qclass = ntohs(m->req.qclass);
        offset += sizeof(m->req.qclass);
    }
    if (m->header.ancount > 0) {
        m->res.name_len = 1;
        for (j = offset; (*j) != 0; j++) {
            m->res.name_len++;
        }
        memcpy(&m->res.name, offset, m->res.name_len);
        offset += m->res.name_len;
        memcpy(&m->res.type, offset, sizeof(m->res.type));
        m->res.type = ntohs(m->res.type);
        offset += sizeof(m->res.type);
        memcpy(&m->res.class, offset, sizeof(m->res.class));
        m->res.class = ntohs(m->res.class);
        offset += sizeof(m->res.class);
        memcpy(&m->res.ttl, offset, sizeof(m->res.ttl));
        m->res.ttl = ntohs(m->res.ttl);
        offset += sizeof(m->res.ttl);
        memcpy(&m->res.rdlength, offset, sizeof(m->res.rdlength));
        m->res.rdlength = ntohl(m->res.rdlength);
        offset += sizeof(m->res.rdlength);
        memcpy(&m->res.rdata, offset, sizeof(m->res.rdata));
        offset += sizeof(m->res.rdata);
    }
    return 0;
}
