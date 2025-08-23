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
#define CTMP_MAGIC 0xCC

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

typedef struct ctmp_packet {
    unsigned char magic;
    unsigned char padding_byte;
    unsigned short length_no;
    unsigned int padding_4_bytes;
    char data[];
} ctmp_t;

typedef struct {
    long msg_type;
    size_t frag_len;
    size_t total_len;
    size_t offset;
    char msg_text[MAX_MSG_SIZE];
} fragment_t;

extern int msgid;
extern server_t *serv;

#endif