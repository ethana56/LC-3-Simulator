#include <stdio.h>
#include <stdlib.h>
#include "user_interface.h"

int main(int argc, char **argv) {
    /*char *error_string;
    if (start(error_string) < 0) {
        fputs(error_string, stderr);
        exit(1);
    }
    exit(0);*/
    start(argv[0]);
    return 0;
}
