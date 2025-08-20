#ifndef WIRE_STORM_UTILS_H
#define WIRE_STORM_UTILS_H

#include "globals.h"

void free_client(client_t *cli);
void delete_all(server_t *s);
void fatal_error(server_t *s, char *msg);
client_t *add_client(server_t *s, int fd);
void register_client(server_t *s, int fd);
void accept_registration(server_t *s);
void bind_and_listen(server_t *s);
void configure_addr(server_t *s);
void create_socket(server_t *s);
server_t *initialise_server(int port);

#endif