#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

size_t read_convert_16bits(uint16_t *, size_t, FILE *);
int set_nonblock(int fd);
size_t safe_strcat(char *dest, char *src, size_t dest_len);
char *safe_strncpy(char *dst, char *src, size_t len);
size_t safe_write(int fd, const void *buf, size_t amt);
char *alloc_strcpy(const char *dst);
#endif
