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

    *server_sock_ptr = server_sock;
    *server_ptr = server;
}

client_t *add_client(server_t *s, int fd) {
    client_t *cli = (client_t *) malloc(sizeof(client_t));

    if (!cli) {
        printf("Error: cannot allocate memory for client\n");
        exit(1);
    }

    memset(cli, 0, sizeof(client_t));
    cli->fd = fd;
    cli->id = s->counter++;
    cli->msg = NULL;
    cli->next = s->head;
    s->head = cli;
    
    return cli;
}

client_t *findClient(server_t *s, int fd) {
    client_t *tmp = s->head;

    while (tmp && tmp->fd != fd)
        tmp = tmp->next;
    return tmp;
}

void remove_client(server_t *s, int fd) {
    client_t *tmp = s->head;
    client_t *prev = NULL;

    while (tmp && tmp->fd != fd) {
        prev = tmp;
        tmp = tmp->next;
    }
    if (tmp) {
        if (prev)
            prev->next = tmp->next;
        else
            s->head = tmp->next;
        freeClient(tmp);
    }
}

void send_notification(server_t *s, int fd, char *msg) {
    client_t *cli = s->head;
    while (cli) {
        if (FD_ISSET(cli->fd, &s->writefds) && cli->fd != fd)
            if (send(cli->fd, msg, strlen(msg), 0) < 0) {
                printf("Error: failed to send message to client %d\n", cli->id);
                close(cli->fd);
                FD_CLR(cli->fd, &s->active_fds);
            }
        cli = cli->next;
    }
}

void sendMessage(server_t *s, client_t *cli) {
    char buf[127];
    char *msg;
    while (extract_message(&cli->msg, &msg))
    {
        if (FD_ISSET(cli->fd, &s->writefds)) {
            sprintf(buf, "client %d: ", cli->id);
            send_notification(s, cli->fd, buf);
            send_notification(s, cli->fd, msg);
            free(msg);
        }
    }
}

void deregisterClient(server_t *s, int fd, int cli_id) {
    char buf[127];
    sprintf(buf, "server: client %d just left\n", cli_id);
    send_notification(s, fd, buf);
    FD_CLR(fd, &s->active_fds);
    remove_client(s, fd);
}

void processMessage(server_t *s, int fd) {
    char buf[4096];
    client_t *cli = findClient(s, fd);
    if (!cli) return;
    int read_bytes = recv(fd, buf, sizeof(buf) - 1, 0);
    if (read_bytes <= 0) {
        deregisterClient(s, fd, cli->id);
    } else {
        buf[read_bytes] = '\0';
        cli->msg = str_join(cli->msg, buf);
        sendMessage(s, cli);
    }
}

void registerClient(server_t *s, int fd) {
    client_t *cli = add_client(s, fd);
    char buf[127];
    if (!cli) fatalError(s);
    FD_SET(cli->fd, &s->active_fds);
    if (cli->fd > s->max_fd)
        s->max_fd = cli->fd;
    sprintf(buf, "server: client %d just arrived\n", cli->id);
    send_notification(s, fd, buf);
}

void acceptRegistration(server_t *s) {
    struct sockaddr_in cli; 

    socklen_t len = sizeof(cli);
    int fd = accept(s->sockfd, (struct sockaddr *)&cli, &len);
    if (fd < 0) fatalError(s);
    registerClient(s, fd);
}

void monitor_fds(server_t *s) {
    if (select(s->max_fd + 1, &s->readfds, &s->writefds, NULL, NULL) < 0) fatalError(s);
    int fd = 0;
    while (fd <= s->max_fd) {
        if (FD_ISSET(fd, &s->readfds))
            (fd == s->sockfd) ? acceptRegistration(s) : processMessage(s, fd);
        fd++;
    }
}

void handle_connection(server_t *s) {
    while (1) {
        s->readfds = s->active_fds;
        s->writefds = s->active_fds;
        monitor_fds(s);
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