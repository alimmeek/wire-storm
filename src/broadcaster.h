#ifndef WIRE_STORM_BROADCASTER_H
#define WIRE_STORM_BROADCASTER_H

#include "globals.h"

static int send_all(int fd, const char *buf, size_t len);

void bcast_loop(server_t *s);

#endif