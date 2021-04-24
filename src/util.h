#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

extern struct list_allocator util_list_allocator;

void *safe_malloc(size_t);
void *safe_realloc(void *, size_t);
size_t read_convert_16bits(uint16_t *, size_t, FILE *);
int set_blocking(int fd);
int set_nonblock(int fd);
size_t safe_strcat(char *dest, char *src, size_t dest_len);
char *safe_strncpy(char *dst, char *src, size_t len);
int safe_strcpy(char *, const char *, size_t, size_t);
long long string_to_ll_10_or_16(char *, char **);
size_t safe_write(int fd, const void *buf, size_t amt);
char *get_basename(const char *, size_t);
char *alloc_strcpy(const char *dst);
#endif
