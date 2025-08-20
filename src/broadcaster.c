#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "utils.h"
#include "globals.h"
#include "broadcaster.h"

static int send_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n > 0) {
            off += (size_t) n;
            continue;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -1; // other error
        }
        // n == 0 should not happen for send(); treat as would-block
        return 0;
    }
    return 1;
}

void bcast_loop(server_t *s) {
    message_t message;

    while (1) {
        fd_set rfds = s->active_fds;
        struct timeval tv = {0, 10000}; // 10ms timeout to keep loop responsive

        int ready = select(s->max_fd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            fatal_error(s, "Select failed");
        }

        // Handle any new client connections
        if (FD_ISSET(s->sockfd, &rfds)) {
            accept_registration(s);
        }

        // Broadcast messages from parent
        ssize_t rcv_size = msgrcv(msgid, &message, sizeof(message) - sizeof(long), 1, IPC_NOWAIT);
        if (rcv_size > 0) {
            // hex_dump("Child received", message.msg_text, message.msg_len, 16);

            client_t *cli = s->head;
            while (cli) {
                int rc = send_all(cli->fd, message.msg_text, message.msg_len);
                if (rc < 0) {
                    fatal_error(s, "Failed to send message to client");
                }
                cli = cli->next;
            }
        } else if (rcv_size == -1 && errno != ENOMSG) {
            fatal_error(s, "Failed to receive message from queue");
        }
    }
}