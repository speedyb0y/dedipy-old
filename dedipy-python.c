#define _GNU_SOURCE 1

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>

#include "util.h"

#include "dedipy-lib.h"

int main (void) {

    dedipy_main();

    log("STARTED");

    free(malloc(0));

    while (!sigTERM) {
        log("ALIVE AND KICKING!!!");
        pause();
    }

    log("EXITING");

    return 0;
}
