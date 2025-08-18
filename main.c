#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client {
    int fd;
    int id;
    char *msg;
    struct s_client *next;
} t_client;

typedef struct s_server {
    int sockfd;
    int max_fd;
    int port;
    int counter;
    fd_set readfds, writefds, active_fds;
    struct sockaddr_in addr;
    t_client *head;
} t_server;

void fatalError(t_server *s);

int extract_message(char **buf, char **msg) {
    char *newbuf;
    int i;

    *msg = 0;
    if (*buf == 0)
        return (0);

    i = 0;

    while ((*buf)[i])
    {
        if ((*buf)[i] == '\n')
        {
            newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
            if (newbuf == 0)
                return (-1);
            strcpy(newbuf, *buf + i + 1);
            *msg = *buf;
            (*msg)[i + 1] = 0;
            *buf = newbuf;
            return (1);
        }
        i++;
    }
    return (0);
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

void freeClient(t_client *cli) {
    if (cli) {
        if (cli->msg) free(cli->msg);
        if (cli->fd > 0) close(cli->fd);
        free(cli);
    }
}

t_client *addClient(t_server *s, int fd) {
    t_client *cli = (t_client *)malloc(sizeof(t_client));
    if (!cli) fatalError(s);
    bzero(cli, sizeof(t_client));
    cli->fd = fd;
    cli->id = s->counter++;
    cli->msg = NULL;
    cli->next = s->head;
    s->head = cli;
    return cli;
}

t_client *findClient(t_server *s, int fd) {
    t_client *tmp = s->head;

    while (tmp && tmp->fd != fd)
        tmp = tmp->next;
    return tmp;
}

void removeClient(t_server *s, int fd) {
    t_client *tmp = s->head;
    t_client *prev = NULL;

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

void deleteAll(t_server *s) {
    t_client *tmp = s->head;

    while (tmp) {
        t_client *cache = tmp;
        tmp = tmp->next;
        freeClient(cache);
    }
    if (s->sockfd > 0) {
        close(s->sockfd);
        s->sockfd = -1;
    }
    free(s);
    s = NULL;
}

void fatalError(t_server *s) {
    deleteAll(s);
    write(2, "Fatal error\n", 12);
    exit(1);
}

void sendNotification(t_server *s, int fd, char *msg) {
    t_client *cli = s->head;
    while (cli) {
        if (FD_ISSET(cli->fd, &s->writefds) && cli->fd != fd)
            if (send(cli->fd, msg, strlen(msg), 0) < 0) fatalError(s);
        cli = cli->next;
    }
}

void sendMessage(t_server *s, t_client *cli) {
    char buf[127];
    char *msg;
    while (extract_message(&cli->msg, &msg))
    {
        if (FD_ISSET(cli->fd, &s->writefds)) {
            sprintf(buf, "client %d: ", cli->id);
            sendNotification(s, cli->fd, buf);
            sendNotification(s, cli->fd, msg);
            free(msg);
        }
    }
}

void deregisterClient(t_server *s, int fd, int cli_id) {
    char buf[127];
    sprintf(buf, "server: client %d just left\n", cli_id);
    sendNotification(s, fd, buf);
    FD_CLR(fd, &s->active_fds);
    removeClient(s, fd);
}

void processMessage(t_server *s, int fd) {
    char buf[4096];
    t_client *cli = findClient(s, fd);
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

void registerClient(t_server *s, int fd) {
    t_client *cli = addClient(s, fd);
    char buf[127];
    if (!cli) fatalError(s);
    FD_SET(cli->fd, &s->active_fds);
    if (cli->fd > s->max_fd)
        s->max_fd = cli->fd;
    sprintf(buf, "server: client %d just arrived\n", cli->id);
    sendNotification(s, fd, buf);
}

void acceptRegistration(t_server *s) {
    struct sockaddr_in  cli; 

    socklen_t len = sizeof(cli);
    int fd = accept(s->sockfd, (struct sockaddr *)&cli, &len);
    if (fd < 0) fatalError(s);
    registerClient(s, fd);
}

void monitorFDs(t_server *s) {
    if (select(s->max_fd + 1, &s->readfds, &s->writefds, NULL, NULL) < 0) fatalError(s);
    int fd = 0;
    while (fd <= s->max_fd)
    {
        if (FD_ISSET(fd, &s->readfds))
            (fd == s->sockfd) ? acceptRegistration(s) : processMessage(s, fd);
        fd++;
    }
}

void handleCon(t_server *s) {
    while (1)
    {
        s->readfds = s->active_fds;
        s->writefds = s->active_fds;
        monitorFDs(s);
    }
}

void bindAndListen(t_server *s) {
    (void)s;
 if ((bind(s->sockfd, (const struct sockaddr *)&s->addr, sizeof(s->addr)))) fatalError(s);
 if (listen(s->sockfd, SOMAXCONN)) fatalError(s);
}

void configAddr(t_server *s) {
 bzero(&s->addr, sizeof(s->addr)); 
 s->addr.sin_family = AF_INET; 
 s->addr.sin_addr.s_addr = htonl(2130706433);
 s->addr.sin_port = htons(s->port); 
}

void createSock(t_server *s) {
 s->sockfd = socket(AF_INET, SOCK_STREAM, 0); 
 if (s->sockfd < 0)  fatalError(s);

    FD_SET(s->sockfd, &s->active_fds);
    s->max_fd = s->sockfd;
}

t_server *initServer(int port) {
    t_server *s = (t_server *)malloc(sizeof(t_server));
    if (!s) fatalError(NULL);
    bzero(s, sizeof(t_server));
    FD_ZERO(&s->active_fds);
    FD_ZERO(&s->readfds);
    FD_ZERO(&s->writefds);
    s->port = port;
    return s;
}

int main(int ac, char **av) {
    if (ac != 2) {
        write(2, "Wrong number of argument\n", 26);
        exit(1);
    }
    int port = atoi(av[1]);
    if (port <= 0 || port > 65535) fatalError(NULL);
    t_server *serv = initServer(port);
    if (serv) {
        createSock(serv);
        configAddr(serv);
        bindAndListen(serv);
        handleCon(serv);
        deleteAll(serv);
    }
    return (0);
}