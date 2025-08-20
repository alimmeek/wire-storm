#ifndef WIRE_STORM_GLOBALS_H
#define WIRE_STORM_GLOBALS_H

#include <unistd.h>
#include <sys/select.h>
#include <netinet/in.h>

/*
 * This file defines macros and makes global variables available across files
*/

#define SOURCE_PORT 33333
#define DEST_PORT 44444
#define MAX_MSG_SIZE 4096

typedef struct client {
    int fd;
    int id;
    char *msg;
    size_t msg_len;
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

typedef struct msg_buffer {
    long msg_type;
    size_t msg_len;
    char msg_text[MAX_MSG_SIZE];
} message_t;

extern int msgid;
extern server_t *serv;

#endif