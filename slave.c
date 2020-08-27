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

#include "ms.h"

int main (void) {

    DBGPRINT("SLAVE - MAIN");

    //
    void* const buff0 = malloc(65536);
    void* const buff1 = malloc(65536);
    void* const buff2 = malloc(65536);
    void* const buff3 = malloc(16);
    void* const buff4 = malloc(8);

    memset(buff0, 0x00, 65536);
    memset(buff1, 0x11, 65536);
    memset(buff2, 0x22, 65536);
    memset(buff3, 0x33, 16);
    memset(buff4, 0x44, 8);

    free(buff0);
    free(buff1);
    free(buff2);
    free(buff3);
    free(buff4);

    DBGPRINT("SLAVE - EXITING");

    DBGPRINT("SLAVE - RECEIVING SELF");

    char received[65536];

    const int receivedSize = read(SELF_GET_FD, received, sizeof(received));

    if (receivedSize != - 1)
        write(STDOUT_FILENO, received, receivedSize);

    return 0;
}
