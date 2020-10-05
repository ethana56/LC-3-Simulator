#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>

static struct termios saved_termios;

static void termios_atexit(void) {
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
}

int init_terminal(void) {
   int err;
   struct termios buf;
 
   if (tcgetattr(STDIN_FILENO, &buf) < 0) {
      return -1;
   }
   saved_termios = buf;
   buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
   buf.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON);
   buf.c_cflag &= ~(CSIZE | PARENB);
   buf.c_cflag |= CS8;
   buf.c_oflag &= ~(OPOST);

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
   if ((buf.c_lflag & (ECHO | ICANON | IEXTEN | ISIG)) ||
       (buf.c_iflag & (BRKINT | INPCK | ISTRIP | IXON)) ||
       (buf.c_cflag & (CSIZE | PARENB | CS8)) != CS8 ||
       (buf.c_oflag & OPOST) || buf.c_cc[VMIN] != 1 ||
       buf.c_cc[VTIME] != 0) {
      
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
      errno = EINVAL;
      return -1;
   }
   atexit(termios_atexit);
   return 0;
}
