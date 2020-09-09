
#include "python-dedipy.c"

int main (void) {

    dedipy_main();

    log("STARTED");

    dedipy_free(dedipy_malloc(0));

    while (!sigTERM) {
        log("ALIVE AND KICKING!!!");
        pause();
    }

    log("EXITING");

    return 0;
}
