#include "socket.h"


int close_socket(int socket) {
    if (close(socket)) {
        logging("Failed closing socket.\n");
        return 1;
    }

    return 0;
}

int open_port(int port, struct sockaddr_in * addr){
    int ret_socket;

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;

    if ((ret_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        logging("Failed creating socket.\n");
        return -1;
    }
    logging("Listen socket %d created successfully!\n", ret_socket);

    if (bind(ret_socket, (struct sockaddr *) addr, sizeof(struct sockaddr))) {
        close_socket(ret_socket);
        logging("Failed binding socket.\n");
        return -1;
    }
    logging("listen socket %d binded successfully\n", ret_socket);

    if (listen(ret_socket, 5)) {
        close_socket(ret_socket);
        logging("Error listening on socket %d.\n", ret_socket);
        return EXIT_FAILURE;
    }
    logging("Start listening at port %d.\n", ret_socket);

    return ret_socket;
}