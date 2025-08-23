#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "utils.h"
#include "globals.h"
#include "broadcaster.h"

/**
 * @brief Send all data in buf to client file descriptor, handling partial sends and interruptions.
 * @param fd The file descriptor to send data to.
 * @param buf The buffer containing data to send.
 * @param len The length of data to send.
 * @return 1 on success, 0 if would block, -1 on error.
 */
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


/**
 * @brief Main broadcasting loop: accepts new clients, receives message fragments from the queue,
 * assembles complete messages, and broadcasts them to all connected clients.
 * @param s Pointer to the server structure.
 */
void bcast_loop(server_t *s) {
    fragment_t frag;
    char *assembly_buf = NULL;
    size_t assembly_size = 0;
    size_t received_so_far = 0;

    while (1) {
        fd_set rfds = s->active_fds;
        struct timeval tv = {0, 10000}; // 10ms timeout

        int ready = select(s->max_fd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            fatal_error(s, "select failed");
        }

        // Handle new client connections
        if (FD_ISSET(s->sockfd, &rfds)) {
            accept_registration(s);
        }

        // Handle fragments from queue
        ssize_t rcv_size = msgrcv(msgid, &frag, sizeof(fragment_t) - sizeof(long), 1, IPC_NOWAIT);
        if (rcv_size > 0) {
            // Allocate assembly buffer if this is the first fragment
            if (assembly_buf == NULL) {
                assembly_buf = malloc(frag.total_len);
                if (!assembly_buf) {
                    fatal_error(s, "Failed to allocate memory for message assembly buffer");
                }
                assembly_size = frag.total_len;
                received_so_far = 0;
            }

            memcpy(assembly_buf + frag.offset, frag.msg_text, frag.frag_len);
            received_so_far += frag.frag_len;

            // When complete, broadcast
            if (received_so_far >= assembly_size) {
                client_t *cli = s->head;
                while (cli) {
                    int rc = send_all(cli->fd, assembly_buf, assembly_size);
                    if (rc < 0) {
                        fatal_error(s, "Failed to send message to client");
                    }
                    cli = cli->next;
                }

                free(assembly_buf);
                assembly_buf = NULL;
                assembly_size = 0;
                received_so_far = 0;
            }
        } else if (rcv_size == -1 && errno != ENOMSG) {
            fatal_error(s, "Failed to receive message from queue");
        }
    }
}