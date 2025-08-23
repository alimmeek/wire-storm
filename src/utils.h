#ifndef WIRE_STORM_UTILS_H
#define WIRE_STORM_UTILS_H

#include "globals.h"

client_t *add_client(server_t *s, int fd);

server_t *initialise_server(int port);

void accept_registration(server_t *s);
void bind_and_listen(server_t *s);
void configure_addr(server_t *s);
void create_socket(server_t *s);
void delete_all(server_t *s);
void fatal_error(server_t *s, char *msg);
void free_client(client_t *cli);
void register_client(server_t *s, int fd);

#endif