#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

size_t read_convert_16bits(uint16_t *, size_t, FILE *);
int set_nonblock(int fd);

#endif
