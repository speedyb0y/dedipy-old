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

int main (int argsN, char* args[]) {

    if (argsN != 2)
        return 1;

    const uint slaveN = atoi(args[1]);

    DBGPRINTF("SLAVE[%u] - TEST 0", slaveN);

    { uint c = 10;

        while (c--) {

            free(NULL);

            free(malloc(RSIZE(c + 1)));
            free(malloc(RSIZE(c + 2)));
            free(malloc(RSIZE(c + 3)));

            free(realloc(malloc(RSIZE(c + 4)), RSIZE(c + 10)));
            free(realloc(malloc(RSIZE(c + 5)), RSIZE(c + 11)));

            free(malloc(RSIZE(c + 6)));
            free(malloc(RSIZE(c + 7)));

            free(realloc(malloc(RSIZE(c + 8)), RSIZE(c + 12)));
            free(realloc(malloc(RSIZE(c + 9)), RSIZE(c + 13)));
        }
    }

    // TODO: FIXME: LEMBRAR O TAMANHO PEDIDO, E DAR UM MEMSET()
    { uint counter = 100;
        while (counter--) {

            DBGPRINTF("SLAVE[%u] - COUNTER %u\n", slaveN, counter);

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

    DBGPRINTF("SLAVE[%u] - EXITING", slaveN);

    DBGPRINTF("SLAVE[%u] - RECEIVING SELF", slaveN);

    char received[65536];

    const uint receivedSize = SELF_GET(received, sizeof(received));

    DBGPRINTF("SLAVE[%u] - RECEIVED %u BYTES:", slaveN, receivedSize);

    write(STDOUT_FILENO, received, receivedSize);

    return 0;
}

// TODO: FIXME: TESTAR COM VÁRIAS THREADS/FORKS
