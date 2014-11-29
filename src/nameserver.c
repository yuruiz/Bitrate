#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "nameserver.h"
#include "conn.h"
#include "log.h"

dns_server_t ns;

int initNode(dns_server_t *ns, char *name) {
    int pos;
    net_node_t *nd;

    pos = (ns->node_num++);
    nd = &ns->nodes[pos];

    nd->is_server = 0;
    strncpy(nd->name, name, NAME_LEN - 1);
    nd->name[NAME_LEN - 1] = '\0';

    nd->version = -1;
    //memset(nd->node_pos, 0, sizeof(nd->node_pos));
    nd->count = 0;
    return pos;
}

int getNode(dns_server_t *ns, char *name) {
    int i;
    for (i = 0; i < ns->node_num; i++) {
        if(strncmp(name, ns->nodes[i].name, NAME_LEN - 1) == 0) {
            return i;
        }
    }
    return initNode(ns, name);
}

int parse_nodes(const char *filename) {
    FILE *fp;
    int len;
    int pos;
    char name[NAME_LEN];
    if ((fp = fopen(filename, "r")) == 0) {
        fprintf(stderr, "open server file failed: %s\n", filename);
        return -1;
    }

    while(fgets(name, NAME_LEN, fp)) {
        len = strlen(name);
        name[len-1] = '\0';
        pos = initNode(&ns, name);
        ns.nodes[pos].is_server = 1;
    }
    fclose(fp);
    return 0;
}

int parse_LSAs(const char *filename) {
    FILE *fp;
    int len, seq_num, pos, neighbor;
    char line[LINE_LEN];
    char *split, *name, *p;

    fp = fopen(filename, "r");
    while( fgets(line, LINE_LEN, fp) ) {
        len = strlen(line);
        line[len-1] = '\0';
        split = strtok(line, " ");
        name = split;
        pos = getNode(&ns, name);
        split = strtok(NULL, " ");
        seq_num = atoi(split);
        split = strtok(NULL, " ");
        if (seq_num <= ns.nodes[pos].version) {
            continue;
        }
        ns.nodes[pos].version = seq_num;
        ns.nodes[pos].count = 0;
        p = strtok(split, ",");
        while(p){
            if(strlen(p))
                neighbor = getNode(&ns, p);
            ns.nodes[pos].node_pos[(ns.nodes[pos].count++)] = neighbor;
            p = strtok(NULL, ",");
        }
    }
    fclose(fp);
    return 0;
}

int main(int argc, char const* argv[]) {
    ssize_t ret;
    const char *res;
    char rcode, buf[PACKET_LEN], name[255];
    struct sockaddr_in cli_addr;
    socklen_t addrlen;
    dns_message_t req_message, res_message;
    ns.node_num = 0;
    ns.rr = (argc == 7) ? 0:-1;

    // parse parameters
    loginit(argv[ns.rr + 2]);
    inet_aton(argv[ns.rr + 3], & ns.addr.sin_addr);
    ns.addr.sin_port = htons(atoi(argv[ns.rr + 4]));
    ns.addr.sin_family = AF_INET;
    if (parse_nodes(argv[ns.rr + 5]) < 0) {
        return -1;
    }
    if (ns.rr == -1) {
        parse_LSAs(argv[ns.rr + 6]);
    }

    if ((ns.sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    if (bind(ns.sock, (struct sockaddr *) &ns.addr, sizeof(ns.addr)) < 0) {
        fprintf(stderr, "bind failed\n");
        return -1;
    }

    // main loop
    while (1) {
        addrlen = sizeof(cli_addr);
        if ((ret = recvfrom(ns.sock, buf, PACKET_LEN, 0,
                (struct sockaddr *) &cli_addr, &addrlen)) <= 0) {
            fprintf(stderr, "recvfrom failed");
            continue;
        }
        decode(&req_message, buf, ret);

    }
}
