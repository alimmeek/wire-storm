#ifndef WIRE_STORM_RECEIVER_H
#define WIRE_STORM_RECEIVER_H

#include "globals.h"

int validate_message(char *msg);
client_t *find_client(server_t *s, int fd);
void remove_client(server_t *s, int fd);
void send_to_broadcast(server_t *s, int fd, char *msg, size_t msg_len);
void send_message(server_t *s, client_t *cli);
void deregister_client(server_t *s, int fd, int cli_id);
void process_message(server_t *s, int fd);
void monitor_FDs(server_t *s);
void handle_connection(server_t *s);

#endif