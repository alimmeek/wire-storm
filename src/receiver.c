#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <stdlib.h>

#include "globals.h"
#include "utils.h"


int validate_message(char *msg) {
    return 1;
}

client_t *find_client(server_t *s, int fd) {
    client_t *tmp = s->head;

    while (tmp && tmp->fd != fd) {
        tmp = tmp->next;
    }
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
        if (prev) {
            prev->next = tmp->next;
        } else {
            s->head = tmp->next;
        }

        free_client(tmp);
    }
}

void send_notification(server_t *s, int fd, char *msg, size_t msg_len) {
    message_t message;
    message.msg_type = 1;

    if (msg_len > MAX_MSG_SIZE) {
        fprintf(stderr, "Message too large for queue\n");
        return;
    }

    message.msg_len = msg_len;
    memcpy(message.msg_text, msg, msg_len);

    if (msgsnd(msgid, &message, sizeof(size_t) + msg_len, 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
}

void send_message(server_t *s, client_t *cli) {
    if (cli->msg && cli->msg_len > 0) {
        if (validate_message(cli->msg)) {
            send_notification(s, cli->fd, cli->msg, cli->msg_len);
        }
        free(cli->msg);
        cli->msg = NULL;
        cli->msg_len = 0;
    }
}

void deregister_client(server_t *s, int fd, int cli_id) {
    printf("server: client %d just left on %d\n", cli_id, s->port);
    FD_CLR(fd, &s->active_fds);
    remove_client(s, fd);
}

void process_message(server_t *s, int fd) {
    char buf[4096];
    client_t *cli = find_client(s, fd);
    if (!cli) {
        return;
    }

    printf("server: processing message from client %d (port %d)\n", cli->id, s->port);

    int read_bytes = recv(fd, buf, sizeof(buf), 0);
    if (read_bytes <= 0) {
        deregister_client(s, fd, cli->id);
    } else {
        printf("server: received %d bytes from client %d (port %d)\n", read_bytes, cli->id, s->port);

        // grow the message buffer
        cli->msg = realloc(cli->msg, cli->msg_len + read_bytes);
        if (!cli->msg) {
            fatal_error(s, "Failed to allocate memory for client message");
        }

        memcpy(cli->msg + cli->msg_len, buf, read_bytes);
        cli->msg_len += read_bytes;

        // hex_dump("Received message", cli->msg, cli->msg_len, 16);

        send_message(s, cli);
    }
}

void monitor_FDs(server_t *s) {
    if (select(s->max_fd + 1, &s->readfds, &s->writefds, NULL, NULL) < 0) {
        fatal_error(s, "select failed");
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
    while (1) {
        s->readfds = s->active_fds;
        s->writefds = s->active_fds;
        monitor_FDs(s);
    }
}