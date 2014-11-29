#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "mydns.h"
#include "proxy.h"
#include "conn.h"

dns_t dns;
extern proxy_t proxy;

int init_mydns(const char *dns_ip, unsigned int dns_port) {
    memset(&dns, 0, sizeof(dns_t));
    strcpy(dns.dns_ip, dns_ip);
    dns.dns_port = dns_port;
    if ((dns.sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)) < 0) {
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
        if (req_message.header.id != res_message.header.id) {
            fprintf(stderr, "response id not match\n");
            return -1;
        }
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