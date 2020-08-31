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

static uintll _rand = 0;

static inline u64 RANDOM (const u64 x) {

    _rand += x;
    _rand += rdtsc() & 0xFFFULL;
    _rand += __builtin_ia32_rdrand64_step(&_rand);

    return _rand;
}

static inline u64 RSIZE (u64 x) {

    x = RANDOM(x);

    return (x >> 2) & (
        (x & 0b1ULL) ? (
            (x & 0b10ULL) ? 0xFFFFFULL :   0xFFULL
        ) : (
            (x & 0b10ULL) ?   0xFFFULL : 0xFFFFULL
        ));
}

int main (int argsN, char* args[]) {

    if (argsN != 2)
        return 1;

    const uint slaveID = atoi(args[1]);

    (void)slaveID;

#if 0
    if (slaveID !=0)
        return 1;
#endif

#if 1
    // PRINT HOW MUCH MEMORY WE CAN ALLOCATE
    { u64 blockSize = 256*4096; // >= sizeof(void**)

        do { u64 count = 0; void** last = NULL; void** this;
            //
            while ((this = malloc(blockSize))) {
                *this = last;
                last = this;
                count += 1;
            }
            // NOW FREE THEM ALL
            while (last) {
                this = *last;
                free(last);
                last = this;
            }
            //
            if (count) {
                DBGPRINTF("SLAVE [%u] - ALLOCATED %llu BLOCKS of %llu BYTES = %llu", slaveID, (uintll)count, (uintll)blockSize, (uintll)(count * blockSize));
            }
        } while ((blockSize <<= 1));
    }
#endif

    DBGPRINTF("SLAVE [%u] - TEST 0", slaveID);

    { uint c = 100;
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
    { uint c = 1000;
        while (c--) {

            DBGPRINTF("SLAVE [%u] - COUNTER %u", slaveID, c);

            void** last = NULL;
            void** new;

            // NOTE: cuidato com o realloc(), só podemos realocar o ponteiro atual, e não os anteriores
            while ((new = malloc(sizeof(void**) + RSIZE(c)))) {
                if (RANDOM(c) % 10 == 0)
                    new = realloc(new, RSIZE(c)) ?: new;
                // TODO: FIXME: uma porcentagem, dar free() aqui ao invés de incluir na lista
                *new = last;
                last = new;
            }

            while (last) {
                if (RANDOM(c) % 10 == 0)
                    last = realloc(last, sizeof(void**) + RSIZE(c)) ?: last;
                void** old = *last;
                free(last);
                last = old;
            }
        }
    }

    // TODO: FIXME: OUTRO TESTE: aloca todos com 4GB, depois com 1GB, até 8 bytes,
    // u64 size = 1ULL << 63;
    // do {
    //      while((this = malloc(size)))
    //          ...
    //      while (last) {
    //          free()
    //      }
    // } while ((size >>= 1));

    DBGPRINTF("SLAVE [%u] - RECEIVING SELF", slaveID);

    char received[65536];

    const uint receivedSize = SELF_GET(received, sizeof(received));

    DBGPRINTF("SLAVE [%u] - RECEIVED %u BYTES:", slaveID, receivedSize);

    write(STDOUT_FILENO, received, receivedSize);

    DBGPRINTF("SLAVE [%u] - EXITING", slaveID);

    return 0;
}
