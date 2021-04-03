#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>

static struct termios saved_termios;

static void termios_atexit(void) {
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
}

void reset_terminal(void) {
   termios_atexit();
}

int init_terminal(void) {
   int err;
   struct termios buf;

   if (!isatty(STDIN_FILENO)) {
      return 0;
   }
   if (tcgetattr(STDIN_FILENO, &buf) < 0) {
      return -1;
   }
   saved_termios = buf;
   buf.c_lflag &= ~(ECHO | ICANON );

   buf.c_cc[VMIN] = 1;
   buf.c_cc[VTIME] = 0;
   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0) {
      return -1;
   }
   if (tcgetattr(STDIN_FILENO, &buf) < 0) {
      err = errno;
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
      errno = err;
      return -1;
   }
   if ((buf.c_lflag & (ECHO | ICANON)) ||
       buf.c_cc[VMIN] != 1 ||
       buf.c_cc[VTIME] != 0) {
      
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
      errno = EINVAL;
      return -1;
   }
   atexit(termios_atexit);
   return 0;
}
