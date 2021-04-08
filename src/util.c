#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>

size_t read_convert_16bits(uint16_t *dest, size_t amt, FILE *file) {
   size_t i;
   uint16_t result;
   for (i = 0; i < amt; ++i) {
      if (fread(&result, sizeof(uint16_t), 1, file) == 0) {
         break;
      }
      *dest = ntohs(result);
      ++dest;
   }
   return i;
}

int set_blocking(int fd) {
   int old_status, new_status;
   old_status = fcntl(fd, F_GETFL, 0);
   if (old_status < 0) {
      return -1;
   }
   new_status = fcntl(fd, F_SETFL, old_status & ~O_NONBLOCK);
   if (new_status < 0) {
      return -1;
   }
   return old_status;
}

int set_nonblock(int fd) {
   int old_status, new_status;
   old_status = fcntl(fd, F_GETFL, 0);
   if (old_status < 0) {
      return -1;
   }
   new_status = fcntl(fd, F_SETFL, old_status | O_NONBLOCK);
   if (new_status < 0) {
      return -1;
   }
   return old_status;
}

size_t safe_strcat(char *dest, char *src, size_t dest_len) {
   size_t i, amt_cpy;
   for (i = strlen(dest), amt_cpy = 0; i < dest_len && src[amt_cpy] != '\0'; ++i, ++amt_cpy) {
      dest[i] = src[amt_cpy];
   }
   if (i == dest_len) {
      dest[i - 1] = '\0';
      --amt_cpy;
   } else {
      dest[i] = '\0';
   }
   return amt_cpy;
}

char *safe_strncpy(char *dst, char *src, size_t len) {
   strncpy(dst, src, len);
   if (len == 0) return dst;
   dst[len - 1] = '\0';
   return dst;
}

long long string_to_ll_10_or_16(char *str, char **endptr) {
   int base;
   if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
      base = 16;
   } else {
      base = 10;
   }
   return strtol(str, endptr, base);
}

size_t safe_write(int fd, const void *buf, size_t amt) {
   const unsigned char *buf_ptr = buf;
   ssize_t ret;
   size_t written = 0;
   while (written != amt) {
      ret = write(fd, buf_ptr + written, amt - written);
      if (ret < 0) {
         break;
      }
      written += ret;
   }
   return written;
}

char *alloc_strcpy(const char *src) {
    char *dst;
    dst = malloc(strlen(src) + 1);
    if (dst == NULL) {
        return NULL;
    }
    strcpy(dst, src);
    return dst;
}














