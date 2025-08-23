#ifndef WIRE_STORM_RECEIVER_H
#define WIRE_STORM_RECEIVER_H

#include "globals.h"


client_t *find_client(server_t *s, int fd);

int validate_message(char *msg, size_t msg_len);

void deregister_client(server_t *s, int fd, int cli_id);
void handle_connection(server_t *s);
void monitor_FDs(server_t *s);
void process_message(server_t *s, int fd);
void send_message(server_t *s, client_t *cli);
void send_to_broadcast(server_t *s, int fd, char *msg, size_t msg_len);
void remove_client(server_t *s, int fd);

#endif