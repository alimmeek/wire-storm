#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/msg.h>

#include "broadcaster.h"
#include "globals.h"
#include "receiver.h"
#include "utils.h"

int msgid;
server_t *serv;

/* 
 * Credit to Oduwole Dare for the original implementation of this multi-client TCP server.
 * This code is a modified version of his work, which can be found at:
 * https://medium.com/@oduwoledare/server-side-story-creating-a-multi-client-tcp-server-with-c-and-select-3692db1a8ca3)
*/

void signal_handler(int sig) { 
    printf("Caught signal %d\n", sig);
    delete_all(serv);
    msgctl(msgid, IPC_RMID, NULL);

    exit(sig);
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
            bcast_loop(serv);
        }
        delete_all(serv);
    }
    return 0;
}