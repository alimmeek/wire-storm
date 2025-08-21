#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <stdlib.h>

#include "globals.h"
#include "utils.h"


int validate_message(char *msg, size_t msg_len) {
    // for (int i = 0; i < msg_len; i++) {
    //     printf("%02x ", (unsigned char)msg[i]);
    // }
    // printf("\n");
    ctmp_t *packet = (ctmp_t *) msg;
    if (packet->magic != CTMP_MAGIC) {
        printf("server: received packet has invalid magic number: %x\n", packet->magic);
        return 0;
    }
    if (packet->padding_byte != 0 || packet->padding_4_bytes != 0) {
        printf("server: received packet has non-zero padding byte: %x\n", (packet->padding_byte == 0 ? packet->padding_4_bytes : packet->padding_byte));
        return 0;
    }
    if (ntohs(packet->length_no) != msg_len - sizeof(ctmp_t)) {
        printf("server: received packet has invalid length: %d, expected: %zu\n", packet->length_no, msg_len - sizeof(ctmp_t));
        return 0;
    }
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

void send_to_broadcast(server_t *s, int fd, char *msg, size_t msg_len) {
    size_t offset = 0;

    while (offset < msg_len) {
        size_t chunk_len = msg_len - offset;
        if (chunk_len > MAX_MSG_SIZE) chunk_len = MAX_MSG_SIZE;

        fragment_t frag;
        frag.msg_type = 1;
        frag.frag_len = chunk_len;
        frag.total_len = msg_len;
        frag.offset = offset;

        memcpy(frag.msg_text, msg + offset, chunk_len);

        if (msgsnd(msgid, &frag, sizeof(fragment_t) - sizeof(long), 0) == -1) {
            fatal_error(s, "Failed to send message to message queue");
        }

        offset += chunk_len;
    }
}

void send_message(server_t *s, client_t *cli) {
    if (cli->msg && cli->msg_len > 0) {
        if (validate_message(cli->msg, cli->msg_len) != 0) {
            send_to_broadcast(s, cli->fd, cli->msg, cli->msg_len);
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
    int read_bytes;
    client_t *cli = find_client(s, fd);
    if (!cli) {
        return;
    }

    printf("server: processing message from client %d (port %d)\n", cli->id, s->port);

    while ((read_bytes = recv(fd, buf, sizeof(buf), 0)) > 0) {

        // append received bytes to client buffer
        cli->msg = realloc(cli->msg, cli->msg_len + read_bytes + 1);
        if (!cli->msg) {
            fatal_error(s, "Failed to allocate memory for client message");
        }

        printf("server: received %d bytes from client %d\n", read_bytes, cli->id);
        memcpy(cli->msg + cli->msg_len, buf, read_bytes);
        cli->msg_len += read_bytes;
        cli->msg[cli->msg_len] = '\0';


        if (read_bytes < sizeof(buf)) {
            printf("server: received complete message from client %d\n", cli->id);
            break; // assume end of message if less than buffer size
        }
    }

    if (read_bytes < 0) {
        fatal_error(s, "Failed to read from client socket");
    } else {
        if (cli->msg_len > 0) {
            printf("server: processing complete message of %zu bytes from client %d\n", cli->msg_len, cli->id);
            send_message(s, cli);
        }

        deregister_client(s, fd, cli->id);
        return;
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