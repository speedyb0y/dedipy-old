/*
    TODO: FIXME: TESTAR COM VÁRIAS THREADS/FORKS: SLAVE[slaveID:subProcessID]
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

static u64 rand = 0;

static inline u64 RANDOM (const u64 x) {

    rand += x;
    rand += rdtsc() & 0xFFFULL;
    rand += __builtin_ia32_rdrand64_step(&rand);

    return rand;
}

// TODO: FIXME: mais regras
#define RSIZE(x) (RANDOM(x) % ((RANDOM(x) % 3 == 0) ? 0xFFFFFULL : 0xFFFULL))

int main (int argsN, char* args[]) {

    if (argsN != 2)
        return 1;

    const uint slaveID = atoi(args[1]);

    DBGPRINTF("SLAVE[%u] - TEST 0", slaveID);

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
    { uint c = 100;
        while (c--) {

            DBGPRINTF("SLAVE[%u] - COUNTER %u\n", slaveID, c);

            void** last = NULL;
            void** new;

            while ((new = malloc(sizeof(void**) + RSIZE(c)))) {
                if (RANDOM(c) % 10 == 0)
                    new = realloc(new, RSIZE(c)) ?: new;
                // TODO: FIXME: uma porcentagem, dar free() aqui ao invés de incluir na lista
                *new = last;
                last = new;
            }

            while (last) {
                if (RANDOM(c) % 10 == 0)
                    last = realloc(last, RSIZE(c)) ?: last;
                void** old = *last;
                free(last);
                last = old;
            }
        }
    }

    DBGPRINTF("SLAVE[%u] - EXITING", slaveID);

    DBGPRINTF("SLAVE[%u] - RECEIVING SELF", slaveID);

    char received[65536];

    const uint receivedSize = SELF_GET(received, sizeof(received));

    DBGPRINTF("SLAVE[%u] - RECEIVED %u BYTES:", slaveID, receivedSize);

    write(STDOUT_FILENO, received, receivedSize);

    return 0;
}
