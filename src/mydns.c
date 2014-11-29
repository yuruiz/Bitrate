#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "mydns.h"

dns_t dns;
proxy_t proxy;

int init_mydns(const char *dns_ip, unsigned int dns_port) {
    memset(&dns, 0, sizeof(dns_t));
    strcpy(dns.dns_ip, dns_ip);
    dns.dns_port = dns_port;
    if ((dns.sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }
    if (bind(dns.sock, (struct sockaddr *) &proxy.myaddr, sizeof(proxy.myaddr)) < 0 ) {
        fprintf(stderr, "bind failed\n");
        return -1;
    }
    memset(&dns.addr, 0, sizeof(dns.addr));
    dns.addr.sin_family = AF_INET;
    dns.addr.sin_port = htons(dns_port);
    if (inet_aton(dns_ip, &dns.addr.sin_addr) < 0) {
        fprintf(stderr, "inet_aton failed\n");
        return -1;
    }
    return 0;
}

int resolve(const char *node, const char *service,
        const struct addrinfo *hints, struct addrinfo **res) {
    char buf[PACKET_LEN];
    dns_message_t req_message, res_message;
    size_t ret;
    fd_set fdset;
    struct timeval tv;
    struct addrinfo *new_res;
    struct sockaddr_in * sa_in;

    if ((ret = initDNSRequest(&req_message, node, buf)) < 0) {
        return -1;
    }

    if ((ret = sendto(dns.sock, buf, ret, 0,
            (struct sockaddr *)&dns.addr,
            sizeof(dns.addr))) < 0) {
        fprintf(stderr, "sendto failed\n");
        return -1;
    }

    // add dns fd and wait for response
    FD_ZERO(&fdset);
    FD_SET(dns.sock, &fdset);
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if ((ret = select(dns.sock+1, &fdset, NULL, NULL, &tv)) <= 0) {
        fprintf(stderr, "select failed or timeout\n");
        return -1;
    }
    if(FD_ISSET(dns.sock, &fdset)) {
        if ((ret = recvfrom(dns.sock, buf, PACKET_LEN, 0, NULL, NULL)) <= 0) {
            fprintf(stderr, "recvfrom failed\n");
            return -1;
        }
        if (decode(&res_message, buf, ret) != 0) {
            fprintf(stderr, "decode failed\n");
            return -1;
        }
//        if (req_message.header.id != res_message.header.id) {
//            fprintf(stderr, "response id not match\n");
//            return -1;
//        }
        new_res = (struct addrinfo *)calloc(1, sizeof(struct addrinfo)
                + sizeof(struct sockaddr_in));

        //fill address info, sockaddr and canonname.
        new_res->ai_flags = 0;
        new_res->ai_family = AF_INET;
        new_res->ai_socktype = SOCK_STREAM;
        new_res->ai_protocol = 0;
        new_res->ai_addrlen = sizeof(struct sockaddr_in);
        new_res->ai_addr = (struct sockaddr *) (new_res + 1);
        sa_in = (struct sockaddr_in *)new_res->ai_addr;
        sa_in->sin_family = AF_INET;
        sa_in->sin_port = htons(atoi(service));
        memcpy(&sa_in->sin_addr, &res_message.res.rdata, sizeof(res_message.res.rdata));
        new_res->ai_canonname = (char *)calloc(1, strlen(node) + 1);
        memcpy(new_res->ai_canonname, node, strlen(node));
        new_res->ai_next = NULL;
        (*res) = new_res;
        return 0;
    }
    return -1;
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