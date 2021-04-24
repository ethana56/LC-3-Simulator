#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stddef.h>

#include "list.h"

void *safe_malloc(size_t);
void *safe_realloc(void *, size_t);

const struct list_allocator util_list_allocator = {safe_malloc, safe_realloc, free};

void *safe_malloc(size_t size) {
   void *ptr;
   ptr = malloc(size);
   if (ptr == NULL) {
      perror(NULL);
      abort();
   }
   return ptr;
}

void *safe_realloc(void *ptr, size_t size) {
   void *new_ptr;
   new_ptr = realloc(ptr, size);
   if (new_ptr == NULL) {
      perror(NULL);
      abort();
   }
   return new_ptr;
}

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

char *safe_strncpy(char *dst, const char *src, size_t len) {
   strncpy(dst, src, len);
   if (len == 0) return dst;
   dst[len - 1] = '\0';
   return dst;
}

int safe_strcpy(char *dst, const char *src, size_t dst_bufsiz, size_t src_strlen) {
   size_t i, amt_to_cpy;
   amt_to_cpy = dst_bufsiz <= src_strlen ? dst_bufsiz - 1 : src_strlen;
   for (i = 0; i < amt_to_cpy; ++i) {
      dst[i] = src[i];
   }
   dst[i] = '\0';
   return amt_to_cpy == src_strlen;
}

/*int safe_strcpy(char *dst, const char *src, size_t dst_bufsiz, size_t src_strlen) {
   int result;
   size_t amt_to_cpy;
   if (dst_bufsiz == 0) {
      return 0;
   }
   result = dst_bufsiz <= src_strlen ? 0 : 1;
   amt_to_cpy = dst_bufsiz <= src_strlen ? dst_bufsiz - 1 : src_strlen;
   dst[0] = '\0';
   strncat(dst, src, amt_to_cpy);
   return result;
}*/

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

/*int get_basename(char *base_result, const char *path, size_t base_result_len, size_t path_len) {
   char *base;
   size_t base_len;
   base = strrchr(path, '/');
   if (base == NULL) {
      return safe_strcpy(base_result, path, base_result_len, path_len);
   }
   if (base == path) {
      int res;
      ++base;
      return safe_strcpy(base_result, base, base_result_len, path_len - 1);
   }
   if (base[1] == '\0') {
      --base;
      for (base_len = 1; base != path && *(base - 1) != '/'; --base, ++base_len);
   } else {
      ++base;
      base_len = path_len - (base - path);
   }
   return safe_strcpy(base_result, base, base_result_len, base_len);
}*/

char *get_basename(const char *path, size_t path_len) {
   char *base, *base_result;
   size_t base_len;
   base = strrchr(path, '/');
   if (base == NULL) {
      base_result = safe_malloc(sizeof(char) * (path_len + 1));
      strcpy(base_result, path);
      return base_result;
   }
   if (base == path) {
      ++base;
      base_result = safe_malloc(sizeof(char) * path_len);
      strcpy(base_result, base);
      return base_result;
   }
   if (base[1] == '\0') { 
      --base;
      for (base_len = 1; base != path && *(base - 1) != '/'; --base, ++base_len);
   } else {
      ++base;
      base_len = path_len - (base - path);
   }
   base_result = safe_malloc(sizeof(char) * (base_len + 1));
   snprintf(base_result, sizeof(char) * (base_len + 1), "%s", base);
   return base_result;
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














