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

static inline u64 RANDOM(const u64 x) {

    uintll rand = rdtsc() + x;

    return rand + __builtin_ia32_rdrand64_step(&rand);
}

#define RSIZE(x) (RANDOM(x) % ((RANDOM(x) % 3 == 0) ? 0xFFFFFULL : 0xFFFULL))

int main (void) {

    DBGPRINT("SLAVE - MAIN");

    int x = 10;

    while (x--) {

        free(malloc(RSIZE(0)));
        free(malloc(RSIZE(0)));
        free(malloc(RSIZE(0)));

        free(realloc(malloc(RSIZE(0)), RSIZE(0)));
        free(realloc(malloc(RSIZE(0)), RSIZE(0)));

        free(malloc(RSIZE(0)));
        free(malloc(RSIZE(0)));

        free(realloc(malloc(RSIZE(0)), RSIZE(0)));
        free(realloc(malloc(RSIZE(0)), RSIZE(0)));
    }

    // TODO: FIXME: LEMBRAR O TAMANHO PEDIDO, E DAR UM MEMSET()
    { uint counter = 50000;
        while (counter--) {

            printf("SLAVE - COUNTER %u\n", counter);

            void** last = NULL;
            void** new;

            while ((new = malloc(sizeof(void**) + (RANDOM(counter) % ((counter % 3 == 0) ? 0xFFFFFULL : 0xFFFULL))))) {

                if ((rdtsc() + counter) % 10 == 0) {
                    void** const new_ = realloc(new, rdtsc() % 65536);
                    if (new_)
                        new = new_;
                }

                // TODO: FIXME: uma porcentagem, dar free() aqui ao invés de incluir na lista

                *new = last;
                last = new;
            }

            while (last) {

                if ((rdtsc() + counter) % 15 == 0) {
                    void** const last_ = realloc(last, rdtsc() % 65536);
                    if (last_)
                        last = last_;
                }

                void** old = *last;

                free(last);

                last = old;
            }
        }
    }

    DBGPRINT("SLAVE - EXITING");

    DBGPRINT("SLAVE - RECEIVING SELF");

    char received[65536];

    const uint receivedSize = SELF_GET(received, sizeof(received));

    DBGPRINTF("SLAVE - RECEIVED %u BYTES:", receivedSize);

    write(STDOUT_FILENO, received, receivedSize);

    return 0;
}

// TODO: FIXME: TESTAR COM VÁRIAS THREADS/FORKS
