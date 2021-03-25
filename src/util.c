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

int set_nonblock(int fd) {
   int status;
   status = fcntl(fd, F_GETFL, 0);
   if (status == -1) {
      return -1;
   }
   status = fcntl(fd, F_SETFL, status | O_NONBLOCK);
   if (status == -1) {
      return -1;
   }
   return 0;
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














