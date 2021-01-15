#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "device_event_loop.h"

static void callback(int fd, void *data) {
    char buf[BUFSIZ];
    printf("Callback\n");
    read(fd, buf, sizeof(buf));
}

int main(void) {
    Dvel *dvel;
    dvel = dvel_new();
    if (dvel == NULL) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    if (dvel_add_listener_read(dvel, STDIN_FILENO, callback, NULL) < 0) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    sleep(20);
}