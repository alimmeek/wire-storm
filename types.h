#ifndef WIRE_STORM_TYPES_H
#define WIRE_STORM_TYPES_H

#include <stdint.h>

typedef struct ctmp {
    uint8_t magic;
    unsigned short length;
    char* data;
} ctmp_t;

#endif