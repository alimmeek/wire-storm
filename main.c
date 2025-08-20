#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <netinet/in.h>

#define SOURCE_PORT 33333
#define DEST_PORT 44444
#define MAX_MSG_SIZE 1024000

/* 
 * Credit to Oduwole Dare for the original implementation of this multi-client TCP server.
 * This code is a modified version of his work, which can be found at:
 * https://medium.com/@oduwoledare/server-side-story-creating-a-multi-client-tcp-server-with-c-and-select-3692db1a8ca3)
*/

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

int msgid;
server_t *serv;

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

void delete_all(server_t *s);

void signal_handler(int sig)  { 
    printf("Caught signal %d\n", sig);
    delete_all(serv);
    msgctl(msgid, IPC_RMID, NULL);

    exit(sig);
} 

void fatal_error(server_t *s, char *msg);

int validate_message(char *msg) {
    return 1;
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

void child_loop(server_t *s) {
    message_t message;

    while (1) {
        fd_set rfds = s->active_fds;
        struct timeval tv = {0, 10000}; // 10ms timeout to keep loop responsive

        int ready = select(s->max_fd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            perror("select");
            exit(EXIT_FAILURE);
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

int main() {
    signal(SIGINT, signal_handler); 

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    key_t key;

    key = ftok("wire-storm", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);

    serv = initialise_server(pid > 0 ? SOURCE_PORT : DEST_PORT);
    if (serv) {
        create_socket(serv);
        configure_addr(serv);
        bind_and_listen(serv);
        if (pid > 0) {
            handle_connection(serv);
        } else {
            child_loop(serv);
        }
        delete_all(serv);
    }
    return 0;
}