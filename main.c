#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>

#include "types.h"

#define SOURCE_PORT 33333
#define DESTINATION_PORT 44444

void make_socket(struct sockaddr_in *server_ptr, int *server_sock_ptr, unsigned short port_no) {
    struct sockaddr_in server = *server_ptr;
    int server_sock = *server_sock_ptr;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error: cannot create socket\n");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(port_no);

    if (bind(server_sock, (struct sockaddr *) server_ptr, sizeof(server)) < 0) {
        printf("Error: failed to bind to port %d\n", port_no);
        exit(1);
    }
}

int main() {
    struct sockaddr_in server_in, server_out, remaddr;
    socklen_t addrlen = sizeof(remaddr);
    int server_in_sock, server_out_sock, conn_sock;

    make_socket(&server_in, &server_in_sock, SOURCE_PORT);
    make_socket(&server_out, &server_out_sock, DESTINATION_PORT);

    printf("Waiting on Port %d\n", SOURCE_PORT);
    
}