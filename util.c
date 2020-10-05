#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>

size_t read_convert_16bits(uint16_t *dest, size_t amt, FILE *file) {
   size_t i;
   uint16_t result;
   for (i = 0; i < amt; ++i) {
      if (fread(&result, sizeof(uint16_t), 1, file) == 0) {
         break;
      }
      *dest = htons(result);
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
