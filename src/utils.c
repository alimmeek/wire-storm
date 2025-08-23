#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#include "globals.h"


/**
 * @brief Frees a client structure and its associated resources.
 * @param cli Pointer to the client structure to be freed.
 */
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

/**
 * @brief Deletes all clients and frees the server structure.
 * @param s Pointer to the server structure.
 */
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

/**
 * @brief Handles fatal errors (e.g., memory allocation fail) by printing an error message, cleaning up resources,
 * and exiting the program.
 * @param s Pointer to the server structure.
 * @param msg Error message to be printed.
 */
void fatal_error(server_t *s, char *msg) {
    printf("Fatal error: %s\n", msg);
    delete_all(s);
    msgctl(msgid, IPC_RMID, NULL);
    exit(1);
}

/**
 * @brief Adds a new client to the server's client list.
 * @param s Pointer to the server structure.
 * @param fd File descriptor of the new client.
 * @return Pointer to the newly added client structure.
 */
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

/**
 * @brief Registers a new client by adding it to the server's client list and updating the file descriptor sets.
 * @param s Pointer to the server structure.
 * @param fd File descriptor of the new client.
 */
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

/**
 * @brief Accepts a new client connection and registers the client.
 * @param s Pointer to the server structure.
 */
void accept_registration(server_t *s) {
    struct sockaddr_in cli; 

    socklen_t len = sizeof(cli);
    int fd = accept(s->sockfd, (struct sockaddr *)&cli, &len);
    if (fd < 0) {
        fatal_error(s, "Failed to accept client connection");
    }
    register_client(s, fd);
}

/**
 * @brief Binds the server socket to the specified address and port, and starts listening for incoming connections.
 * @param s Pointer to the server structure.
 */
void bind_and_listen(server_t *s) {
    if ((bind(s->sockfd, (const struct sockaddr *)&s->addr, sizeof(s->addr))) != 0) {
        fatal_error(s, "Failed to bind socket");
    }


    printf("server: listening on port %d\n", s->port);
    if (listen(s->sockfd, SOMAXCONN) != 0) {
        fatal_error(s, "Failed to listen on socket");
    }
}

/**
 * @brief Configures the server address structure with the specified port and loopback address.
 * @param s Pointer to the server structure.
 */
void configure_addr(server_t *s) {
    bzero(&s->addr, sizeof(s->addr)); 
    s->addr.sin_family = AF_INET; 
    s->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    s->addr.sin_port = htons(s->port); 
}

/**
 * @brief Creates a TCP socket, sets socket options, and adds it to the server's active file descriptor set.
 * @param s Pointer to the server structure.
 */
void create_socket(server_t *s) {
    s->sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (s->sockfd < 0)  {
        fatal_error(s, "Failed to create socket");
    }

    FD_SET(s->sockfd, &s->active_fds);
    s->max_fd = s->sockfd;

    int optval = 1;

    if (setsockopt(s->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        fatal_error(s, "Failed to set socket options");
    }
}

/**
 * @brief Initializes the server structure, including file descriptor sets and port number.
 * @param port Port number for the server to listen on.
 * @return Pointer to the initialized server structure.
 */
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