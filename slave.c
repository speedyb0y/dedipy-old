/*

*/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"

int main (void) {

    WRITESTR("MAIN");

    //
    void* buff0 = malloc(65536);
    void* buff1 = malloc(65536);
    void* buff2 = malloc(65536);

    memset(buff0, 0x00, 65536);
    memset(buff1, 0x11, 65536);
    memset(buff2, 0x22, 65536);

    free(buff0);
    free(buff1);
    free(buff2);

    WRITESTR("EXITING");

    return 0;
}
