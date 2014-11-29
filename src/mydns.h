#include <netdb.h>

#define PACKET_LEN 1500
#define TIMEOUT 5

typedef struct proxy {
    struct sockaddr_in myaddr;
    char *dns_ip;
    char *dns_port;
} proxy_t;

typedef struct dns {
    int sock;
    char dns_ip[64];
    unsigned int dns_port;
    struct sockaddr_in addr;
} dns_t;

/**
* dns header
*/
typedef struct dns_header {
    uint16_t id;
    uint16_t flag;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

/**
* dns request
*/
typedef struct dns_req {
    int qname_len;
    char qname[255];
    uint16_t qtype;
    uint16_t qclass;
} dns_req_t;

/**
* dns response
*/
typedef struct dns_res {
    int name_len;
    char name[255];
    uint16_t type;
    uint16_t class;
    int32_t ttl;
    uint16_t rdlength;
    struct in_addr rdata; // class A
} dns_res_t;

/**
* dns message
*/
typedef struct dns_message {
    size_t length;
    dns_header_t header;
    dns_req_t req;
    dns_res_t res;
} dns_message_t;

/**
 * Initialize your client DNS library with the IP address and port number of
 * your DNS server.
 *
 * @param  dns_ip  The IP address of the DNS server.
 * @param  dns_port  The port number of the DNS server.
 *
 * @return 0 on success, -1 otherwise
 */
int init_mydns(const char *dns_ip, unsigned int dns_port);


/**
 * Resolve a DNS name using your custom DNS server.
 *
 * Whenever your proxy needs to open a connection to a web server, it calls
 * resolve() as follows:
 *
 * struct addrinfo *result;
 * int rc = resolve("video.cs.cmu.edu", "8080", null, &result);
 * if (rc != 0) {
 *     // handle error
 * }
 * // connect to address in result
 * free(result);
 *
 *
 * @param  node  The hostname to resolve.
 * @param  service  The desired port number as a string.
 * @param  hints  Should be null. resolve() ignores this parameter.
 * @param  res  The result. resolve() should allocate a struct addrinfo, which
 * the caller is responsible for freeing.
 *
 * @return 0 on success, -1 otherwise
 */

int resolve(const char *node, const char *service, 
            const struct addrinfo *hints, struct addrinfo **res);

int initDNSRequest(dns_message_t *m, const char *name, void *encodedBuf);

int initDNSResponse(dns_message_t *response, dns_message_t *req, char rcode, const char *res, void *encodedBuf);

int decode(dns_message_t *m, void *buf, ssize_t len);

int encode(dns_message_t *m, void *encodedBuf);

