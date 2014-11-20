#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "log.h"
#include "conn.h"
#include "socket.h"
#include "parse.h"


static char* fake_ip = NULL;
static char* www_ip = NULL;
static double alpha;

#define USAGE "Usage: %s <log> <alpha> <listen-port> <fake-ip> <dns-ip> <dns-port> [<www-ip>]\n"

char* getfakeip(){
    return fake_ip;
}

char* getwwwip(){
    return www_ip;
}

double getAlpha(){
    return alpha;
}

int main(int argc, char *argv[]) {
    int http_port;
    char *log_file, *dns_ip, *dns_port;
    int http_listen_socket, http_client_sock;
    struct sockaddr_in http_addr, cli_addr;
    socklen_t conn_size;
    static pool conn_pool;

    if (argc < 7 || argc > 8) {
        printf(USAGE, argv[0]);
        return EXIT_FAILURE;
    }

    log_file = argv[1];
    alpha = atof(argv[2]);
    http_port = atoi(argv[3]);
    fake_ip = argv[4];
    dns_ip = argv[5];
    dns_port = argv[6];

    if (argc == 8) {
        www_ip = argv[7];
    }

    loginit(log_file);

    printf("-------------------Server Start------------------\n");


    if ((http_listen_socket = open_port(http_port, &http_addr)) == -1) {
        printf("Open port failed\n");
        return EXIT_FAILURE;
    }

    init_pool(http_listen_socket, &conn_pool);

    do {
//        printf("Pool start\n");
        conn_pool.ready_set = conn_pool.read_set;
        conn_pool.nconn = select(conn_pool.maxfd + 1, &conn_pool.ready_set, NULL, NULL, NULL);
        conn_size = sizeof(cli_addr);

        if (FD_ISSET(http_listen_socket, &conn_pool.ready_set)) {
            printf("Adding new http connection\n");
            if ((http_client_sock = accept(http_listen_socket, (struct sockaddr *) &cli_addr, &conn_size)) == -1) {
                printf("Error accepting http connection.\n");
                continue;
            }
            add_conn(http_client_sock, &conn_pool, &cli_addr);
        }

        conn_handle(&conn_pool);
    } while (1);

    close_socket(http_listen_socket);

    return EXIT_SUCCESS;
}
