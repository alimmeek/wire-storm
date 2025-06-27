#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>

#include "types.h"

#define SOURCE_PORT 33333
#define DESTINATION_PORT 44444


server_t *receiving_server, *sending_server;
pid_t p;


void make_socket(struct sockaddr_in *server, int *server_sock_ptr, unsigned short port_no) {
    int server_sock;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error: cannot create socket\n");
        exit(1);
    }

    server->sin_family = AF_INET;
    server->sin_addr.s_addr = inet_addr("127.0.0.1");
    server->sin_port = htons(port_no);

    if (bind(server_sock, (struct sockaddr *) server, sizeof(*server)) < 0) {
        printf("Error: failed to bind to port %d\n", port_no);
        exit(1);
    }

    *server_sock_ptr = server_sock;
}


int validate_message(char **buf, char **msg) {
    return 1;
}

char *str_join(char *buf, char *add) {
    char *newbuf;
    int  len;

    if (buf == 0)
        len = 0;
    else
        len = strlen(buf);

    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
        return (0);

    newbuf[0] = 0;
    if (buf != 0)
        strcat(newbuf, buf);

    free(buf);
    strcat(newbuf, add);
    return (newbuf);
}

void free_client(client_t *cli) {
    if (cli) {
        if (cli->msg) 
            free(cli->msg);
        if (cli->fd > 0) 
            close(cli->fd);
        free(cli);
    }
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

client_t *find_client(server_t *s, int fd) {
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
        free_client(tmp);
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

void send_message(server_t *s, client_t *cli) {
    char buf[127];
    char *msg;
    while (validate_message(&cli->msg, &msg))
    {
        if (FD_ISSET(cli->fd, &s->writefds)) {
            sprintf(buf, "client %d: ", cli->id);
            send_notification(s, cli->fd, buf);
            send_notification(s, cli->fd, msg);
        }
    }
}

void deregister_client(server_t *s, int fd, int cli_id) {
    char buf[127];
    sprintf(buf, "server: client %d just left\n", cli_id);
    send_notification(s, fd, buf);
    FD_CLR(fd, &s->active_fds);
    remove_client(s, fd);
}

void process_message(server_t *s, int fd) {
    char buf[4096];
    client_t *cli = find_client(s, fd);
    if (!cli) return;
    int read_bytes = recv(fd, buf, sizeof(buf) - 1, 0);
    if (read_bytes <= 0) {
        deregister_client(s, fd, cli->id);
    } else {
        buf[read_bytes] = '\0';
        cli->msg = str_join(cli->msg, buf);
        printf("Got message from client %d\n", cli->id);
    }
}

void register_client(server_t *s, int fd) {
    client_t *cli = add_client(s, fd);
    char buf[127];
    if (!cli) {
        printf("Client wasn't added\n");
    }
    FD_SET(cli->fd, &s->active_fds);
    if (cli->fd > s->max_fd)
        s->max_fd = cli->fd;
    sprintf(buf, "server: client %d just arrived\n", cli->id);
    send_notification(s, fd, buf);
}

void accept_registration(server_t *s) {
    struct sockaddr_in cli; 

    socklen_t len = sizeof(cli);
    int fd = accept(s->sockfd, (struct sockaddr *)&cli, &len);
    if (fd < 0) {
        printf("Unable to accept the new connection");
    }
    register_client(s, fd);
}

void monitor_fds(server_t *s) {
    if (select(s->max_fd + 1, &s->readfds, &s->writefds, NULL, NULL) < 0) {
        printf("Error");
    }
    int fd = 0;
    while (fd <= s->max_fd) {
        if (FD_ISSET(fd, &s->readfds)) {
            (fd == s->sockfd) ? accept_registration(s) : process_message(s, fd);
        }
        fd++;
    }
}

void handle_connection(server_t *s) {
    if (listen(s->sockfd, 5) < 0) {
        printf("Error: failed to listen on port %d\n", s->port);
        exit(1);
    }

    while (1) {
        s->readfds = s->active_fds;
        s->writefds = s->active_fds;
        monitor_fds(s);
    }
}

server_t *init_server(int port, int sockfd, struct sockaddr_in *server_addr) {
    server_t *s = (server_t *) malloc(sizeof(server_t));
    if (!s) {
        printf("Failed to allocate memory to the server.\n");
    }

    memset(s, 0, sizeof(server_t));
    s->sockfd = sockfd;
    FD_ZERO(&s->active_fds);
    FD_ZERO(&s->readfds);
    FD_ZERO(&s->writefds);
    s->port = port;
    s->addr = *server_addr;

    FD_SET(s->sockfd, &s->active_fds);
    s->max_fd = s->sockfd;

    return s;
}

void free_clients(client_t *head) {
    client_t *prev = head;
    client_t *curr = head;

    while (head != NULL) {
        free(prev);
        prev = curr;
        curr = curr->next;
    }
    free(prev);
}

void signal_handler(int signal) {
    if (p == 0) {
        free_clients(receiving_server->head);
        free(receiving_server);
    } else if (p > 0) {
        free_clients(sending_server->head);
        free(sending_server);
    }
    exit(0);
}

int main() {
    signal(SIGINT, signal_handler);

    struct sockaddr_in server_in, server_out;
    int server_in_sock, server_out_sock;

    make_socket(&server_in, &server_in_sock, SOURCE_PORT);
    make_socket(&server_out, &server_out_sock, DESTINATION_PORT);

    p = fork();
    if(p<0) {
      printf("Fork fail\n");
      exit(1);
    } else if (p == 0) {
        receiving_server = init_server(DESTINATION_PORT, server_out_sock, &server_out);
        printf("Waiting on Port %d\n", DESTINATION_PORT);
        handle_connection(receiving_server);
    } else {
        sending_server = init_server(SOURCE_PORT, server_in_sock, &server_in);
        printf("Waiting on Port %d\n", SOURCE_PORT);
        handle_connection(sending_server);
    }    
}