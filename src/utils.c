#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#include "globals.h"

/*
 * Remove after debugging
 * Valkmit/Paxdiablo: https://stackoverflow.com/questions/7775991/how-to-get-hexdump-of-a-structure-data
 */
void hex_dump(const char * desc, const void * addr, const int len, int perLine) {
    // Silently ignore silly per-line values.

    if (perLine < 4 || perLine > 64) perLine = 16;

    int i;
    unsigned char buff[perLine+1];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL) printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of perLine means new or first line (with line offset).

        if ((i % perLine) == 0) {
            // Only print previous-line ASCII buffer for lines beyond first.

            if (i != 0) printf ("  %s\n", buff);

            // Output the offset of current line.

            // printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.

        printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % perLine] = '.';
        else
            buff[i % perLine] = pc[i];
        buff[(i % perLine) + 1] = '\0';
    }

    // Pad out last line if not exactly perLine characters.

    while ((i % perLine) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}

void free_client(client_t *cli) {
    if (cli) {
        if (cli->msg) {
            free(cli->msg);
        }
        if (cli->fd > 0) {
            close(cli->fd);
        }
        free(cli);
    }
}

void delete_all(server_t *s) {
    client_t *tmp = s->head;

    while (tmp) {
        client_t *cache = tmp;
        tmp = tmp->next;
        free_client(cache);
    }

    if (s->sockfd > 0) {
        shutdown(s->sockfd, SHUT_RDWR);
        close(s->sockfd);
        s->sockfd = -1;
    }
    free(s);
    s = NULL;
}

void fatal_error(server_t *s, char *msg) {
    printf("Fatal error: %s\n", msg);
    delete_all(s);
    msgctl(msgid, IPC_RMID, NULL);
    exit(1);
}

client_t *add_client(server_t *s, int fd) {
    client_t *cli = (client_t *) malloc(sizeof(client_t));
    if (!cli) {
        fatal_error(s, "Failed to allocate memory for client");
    }

    bzero(cli, sizeof(client_t));
    cli->fd = fd;
    cli->id = s->counter++;
    cli->msg = NULL;
    cli->next = s->head;
    s->head = cli;
    return cli;
}

void register_client(server_t *s, int fd) {
    client_t *cli = add_client(s, fd);
    char buf[127];
    if (!cli) {
        fatal_error(s, "Failed to register client");
    }
    FD_SET(cli->fd, &s->active_fds);
    if (cli->fd > s->max_fd) {
        s->max_fd = cli->fd;
    }
    printf("server: client %d just arrived on port %d\n", cli->id, s->port);
}

void accept_registration(server_t *s) {
    struct sockaddr_in cli; 

    socklen_t len = sizeof(cli);
    int fd = accept(s->sockfd, (struct sockaddr *)&cli, &len);
    if (fd < 0) {
        fatal_error(s, "Failed to accept client connection");
    }
    register_client(s, fd);
}

void bind_and_listen(server_t *s) {
    if ((bind(s->sockfd, (const struct sockaddr *)&s->addr, sizeof(s->addr))) != 0) {
        fatal_error(s, "Failed to bind socket");
    }
    printf("server: listening on port %d\n", s->port);
    if (listen(s->sockfd, SOMAXCONN) != 0) {
        fatal_error(s, "Failed to listen on socket");
    }
}

void configure_addr(server_t *s) {
    bzero(&s->addr, sizeof(s->addr)); 
    s->addr.sin_family = AF_INET; 
    s->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    s->addr.sin_port = htons(s->port); 
}

void create_socket(server_t *s) {
    s->sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (s->sockfd < 0)  {
        fatal_error(s, "Failed to create socket");
    }

    FD_SET(s->sockfd, &s->active_fds);
    s->max_fd = s->sockfd;
}

server_t *initialise_server(int port) {
    server_t *s = (server_t *)malloc(sizeof(server_t));
    if (!s) {
        fatal_error(NULL, "Failed to allocate memory for server structure");
    }
    bzero(s, sizeof(server_t));
    FD_ZERO(&s->active_fds);
    FD_ZERO(&s->readfds);
    FD_ZERO(&s->writefds);
    s->port = port;
    return s;
}