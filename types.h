#ifndef WIRE_STORM_TYPES_H
#define WIRE_STORM_TYPES_H

#include <stdint.h>

typedef struct ctmp {
    uint8_t magic;
    unsigned short length;
    char* data;
} ctmp_t;


typedef struct client {
    int fd;
    int id;
    char *msg;
    struct client *next;
} client_t;

typedef struct server {
    int sockfd;
    int max_fd;
    int port;
    int counter;
    fd_set readfds, writefds, active_fds;
    struct sockaddr_in addr;
    client_t *head;
} server_t;

#endif