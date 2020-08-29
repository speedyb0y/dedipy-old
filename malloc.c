/*

  TODO: FIXME: reduzir o PTR e o NEXT para um u32, múltiplo de 16

*/

#define _GNU_SOURCE 1
#define _LARGEFILE64_SOURCE 64
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>

#include "util.h"

#include "ms.h"

static int initialized = 0;

// TODO: FIXME: WE WILL NEED A SIGNAL HANDLER
static void initialize (void) {

    initialized = 1;

    DBGPRINT("MALLOC - INITIALIZING");

    char name[256];

    const u64 slavePID = getpid();

    uint   slaveID = ~0U;
    uint   slaveN = ~0U;
    uint   slaveGroupID = ~0U;
    uint   slaveGroupN = ~0U;
    uint   slaveCPU = ~0U;
    uintll slaveCode = 0;
    uintll slaveStarted = 0;
    uintll slaveStart = 0;
    uintll slaveSize = 0;

    // AGORA SIM MAPEIA
    if (!(sscanf(getenv("SLAVEPARAMS"),
        "%02X"    "%02X"   "%02X"         "%02X"        "%016llX"      "%016llX"   "%02X"    "%016llX"     "%016llX",
        &slaveID, &slaveN, &slaveGroupID, &slaveGroupN, &slaveStarted, &slaveCode, &slaveCPU, &slaveStart, &slaveSize) == 9 && slaveStart && slaveSize &&
        mmap(BUFFER, slaveSize, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_LOCKED | MAP_POPULATE, BUFFER_FD, slaveStart) == BUFFER &&
        slaveCPU == sched_getcpu() &&
        snprintf(name, sizeof(name), PROGNAME "#%u", (uint)BUFFER->id) &&
        prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0) == 0
        )) abort();

    // TO PROTECT FROM ACCIDENTS
    if (close(BUFFER_FD))
        abort();

    //  BUFFER                       BUFFER + processSize
    // |___________________________________|
    // | INFO | HEADS | 0 |    CHUNK   | 0 |

    // INFO AND HEADS
    memset(BUFFER, 0, sizeof(Buffer));

    // INFO
    BUFFER->id       = slaveID;
    BUFFER->n        = slaveN;
    BUFFER->groupID  = slaveGroupID;
    BUFFER->groupN   = slaveGroupN;
    BUFFER->cpu      = slaveCPU;
    BUFFER->reserved = 0;
    BUFFER->code     = slaveCode;
    BUFFER->pid      = slavePID;
    BUFFER->started  = slaveStarted;
    BUFFER->start    = slaveStart;
    BUFFER->size     = slaveSize;

    // LEFT AND RIGHT
    BUFFER_L            = BUFFER_LR_VALUE;
    BUFFER_R(slaveSize) = BUFFER_LR_VALUE;

    // THE INITIAL CHUNK
    const u64 size = BUFFER_CHUNK0_SIZE(slaveSize);

    void* const chunk = BUFFER_CHUNK0;

    CHUNK_FREE_ST_SIZE (chunk, size);
    CHUNK_FREE_ST_PTR  (chunk, CHOOSE_HEAD_PTR(size));
   *CHUNK_FREE_LD_PTR  (chunk) = chunk;
    CHUNK_FREE_ST_NEXT (chunk, NULL);
    CHUNK_FREE_ST_SIZE2(chunk, size);

    DBGPRINTF("BUFFER_HEADS_N %llu", (uintll)HEADS_N);

    DBGPRINTF("BUFFER %llu", ADDR(BUFFER));
    DBGPRINTF("BUFFER->id %llu", (uintll)BUFFER->id);
    DBGPRINTF("BUFFER->pid %llu", (uintll)BUFFER->pid);
    DBGPRINTF("BUFFER->cpu %llu", (uintll)BUFFER->cpu);
    DBGPRINTF("BUFFER->start %llu", (uintll)BUFFER->start);
    DBGPRINTF("BUFFER->size %llu", (uintll)BUFFER->size);
    DBGPRINTF("BUFFER->code %llu", (uintll)BUFFER->code);
    DBGPRINTF("BUFFER->heads %llu BUFFER+%llu", ADDR(BUFFER->heads), BOFFSET(BUFFER->heads));
    DBGPRINTF("BUFFER_HEADS_LMT %llu BUFFER+%llu", ADDR(BUFFER_HEADS_LMT), BOFFSET(BUFFER_HEADS_LMT));

    DBGPRINTF(" BUFFER_L %llu", (uintll)BUFFER_L);
    DBGPRINTF("&BUFFER_L %llu BUFFER+%llu", ADDR(&BUFFER_L), BOFFSET(&BUFFER_L));

    DBGPRINTF(" BUFFER_CHUNK0 %llu BUFFER+%llu", ADDR(BUFFER_CHUNK0), BOFFSET(BUFFER_CHUNK0));
    DBGPRINTF(" BUFFER_CHUNK0->size %llu", (uintll)(CHUNK_FREE_LD_SIZE(BUFFER_CHUNK0)));
    DBGPRINTF(" BUFFER_CHUNK0->ptr %llu BUFFER+%llu",  ADDR( CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)), BOFFSET( CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)));
    DBGPRINTF("*BUFFER_CHUNK0->ptr %llu BUFFER+%llu",  ADDR(*CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)), BOFFSET(*CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)));
    DBGPRINTF(" BUFFER_CHUNK0->next %llu BUFFER+%llu", ADDR( CHUNK_FREE_LD_NEXT(BUFFER_CHUNK0)), BOFFSET( CHUNK_FREE_LD_NEXT(BUFFER_CHUNK0)));

    DBGPRINTF(" BUFFER_R %llu", (uintll)BUFFER_R(BUFFER->size));
    DBGPRINTF("&BUFFER_R %llu &BUFFER+%llu", ADDR(&BUFFER_R(BUFFER->size)), BOFFSET(&BUFFER_R(BUFFER->size)));

    DBGPRINTF("BUFFER_LMT %llu BUFFER+%llu", ADDR(BUFFER_LMT), BOFFSET(BUFFER_LMT));

    DBGPRINTF(" chunk %llu BUFFER+%llu", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF(" chunk->size %llu", (uintll)CHUNK_FREE_LD_SIZE(chunk));
    DBGPRINTF(" chunk->ptr %llu BUFFER+%llu",  ADDR( CHUNK_FREE_LD_PTR (chunk)), BOFFSET( CHUNK_FREE_LD_PTR (chunk)));
    DBGPRINTF("*chunk->ptr %llu BUFFER+%llu",  ADDR(*CHUNK_FREE_LD_PTR (chunk)), BOFFSET(*CHUNK_FREE_LD_PTR (chunk)));
    DBGPRINTF(" chunk->next %llu BUFFER+%llu", ADDR( CHUNK_FREE_LD_NEXT(chunk)), BOFFSET( CHUNK_FREE_LD_NEXT(chunk)));

    DBGPRINTF("---");

    ASSERT_ADDR_IN_BUFFER(chunk);
    ASSERT_ADDR_IN_CHUNKS(chunk);

    ASSERT ( BUFFER_L               == BUFFER_LR_VALUE );
    ASSERT ( BUFFER_R(BUFFER->size) == BUFFER_LR_VALUE );
    ASSERT( ((void*)BUFFER + BUFFER->size) == BUFFER_LMT );

    ASSERT ( sizeof(u64) == 8 );
    ASSERT ( sizeof(void*) == 8 );
    ASSERT ( (void*)&BUFFER->heads[HEADS_N] == ((void*)BUFFER + sizeof(Buffer)) );

    //
    if (dup2(SLAVE_GET_FD(BUFFER->id), SELF_GET_FD) != SELF_GET_FD ||
        dup2(SLAVE_PUT_FD(BUFFER->id), SELF_PUT_FD) != SELF_PUT_FD)
        abort();
}

void free (void* const data) {

    DBGPRINTF("");
    DBGPRINTF("FREE(%llu BUFFER+%llu)", ADDR(data), BOFFSET(data));

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        void* chunk = CHUNK_USED_FROM_DATA(data);

        DBGPRINTF("WILL FREE CHUNK %llu BUFFER+%llu | SIZE %llu | IS FREE %d", ADDR(chunk), BOFFSET(chunk), (uintll)CHUNK_UNKN_LD_SIZE(chunk), !!CHUNK_UNKN_IS_FREE(chunk));

        ASSERT(CHUNK_UNKN_IS_FREE(chunk) == 0);

        u64 size = CHUNK_UNKN_LD_SIZE(chunk);

        // JOIN WITH THE LEFT CHUNK
        void* const left = CHUNK_LEFT(chunk);

        DBGPRINTF("LEFT CHUNK %llu BUFFER+%llu", ADDR(left), BOFFSET(left));

        if (CHUNK_UNKN_IS_FREE_AND_NOT_LR(left)) {
            DBGPRINTF("LEFT CHUNK %llu BUFFER+%llu | SIZE %llu | IS FREE %d", ADDR(left), BOFFSET(left), (uintll)CHUNK_UNKN_LD_SIZE(left), !!CHUNK_UNKN_IS_FREE(left));
            DBGPRINTF("LEFT JOINING");
            size += CHUNK_FREE_LD_SIZE(left);
            if (CHUNK_FREE_LD_NEXT(left))
                CHUNK_FREE_ST_PTR(CHUNK_FREE_LD_NEXT(left), CHUNK_FREE_LD_PTR(left));
           *CHUNK_FREE_LD_PTR(left) = CHUNK_FREE_LD_NEXT(left);
            chunk = left;
        }

        // JOIN WITH THE RIGHT CHUNK
        void* const right = CHUNK_RIGHT(chunk, size);

        DBGPRINTF("RIGHT CHUNK %llu BUFFER+%llu", ADDR(right), BOFFSET(right));

        if (CHUNK_UNKN_IS_FREE_AND_NOT_LR(right)) {
            DBGPRINTF("RIGHT CHUNK %llu BUFFER+%llu | SIZE %llu | IS FREE %d", ADDR(right), BOFFSET(right), (uintll)CHUNK_UNKN_LD_SIZE(right), !!CHUNK_UNKN_IS_FREE(right));
            DBGPRINTF("RIGHT JOINING");
            size += CHUNK_FREE_LD_SIZE(right);
            if (CHUNK_FREE_LD_NEXT(right))
                CHUNK_FREE_ST_PTR(CHUNK_FREE_LD_NEXT(right), CHUNK_FREE_LD_PTR(right));
           *CHUNK_FREE_LD_PTR(right) = CHUNK_FREE_LD_NEXT(right);
        }

        DBGPRINTF("JOINED; CHUNK %llu BUFFER+%llu SIZE %llu", ADDR(chunk), BOFFSET(chunk), (uintll)size);

        // WRITE THE SIZE
        CHUNK_FREE_ST_SIZE (chunk, size); // INSERE ELE NA LISTA DE FREES
        CHUNK_FREE_ST_SIZE2(chunk, size); void** const ptr = CHOOSE_HEAD_PTR(size);
        CHUNK_FREE_ST_PTR  (chunk,  ptr);
        CHUNK_FREE_ST_NEXT (chunk, *ptr);

        if (*ptr)
           CHUNK_FREE_ST_PTR(*ptr, CHUNK_FREE_LD_NEXT_(chunk));

        *ptr = chunk;
    }
}

void* malloc (size_t size_) {

    if (__builtin_expect_with_probability(initialized, 1, 0.99) == 0)
        initialize();

    DBGPRINTF("");
    DBGPRINTF("MALLOC(%llu)", (uintll)size_);

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 size = CHUNK_SIZE_FROM_DATA_SIZE(size_);

    DBGPRINTF("MALLOC - CHUNK SIZE REQUESTED %llu", (uintll)size);

    //
    if (size > CHUNK_SIZE_MAX)
        return NULL;

    // PEGA UM LIVRE A SER USADO
    void* used;

    // ENCONTRA A PRIMEIRA LISTA LIVRE
    void** ptr = CHOOSE_HEAD_PTR(size);

    DBGPRINTF("MALLOC - ptr BUFFER->heads[#%llu] %llu BUFFER+%llu", (uintll)(ptr - BUFFER->heads), ADDR(ptr), BOFFSET(ptr));

    // LOGO APÓS O HEADS, HÁ O LEFT CHUNK, COM UM SIZE FAKE 1, PORTANTO ELE É NÃO-NULL, E VAI PARAR SE NAO TIVER MAIS CHUNKS LIVRES
    while ((used = *ptr) == NULL)
        ptr++;

    DBGPRINTF("MALLOC - ptr BUFFER->heads[#%llu] %llu BUFFER+%llu", (uintll)(ptr - BUFFER->heads), ADDR(ptr), BOFFSET(ptr));
    DBGPRINTF("MALLOC - *ptr %llu BUFFER+%llu", ADDR(*ptr), BOFFSET(*ptr));

    // SE
    if (ptr == BUFFER_HEADS_LMT) {
        DBGPRINTF("MALLOC - NO AVAILABLE CHUNK WITH THIS SIZE");
        ASSERT ( used == (void*)BUFFER_LR_VALUE ); // TERÁ LIDO DESTA FORMA
        return NULL;
    }

    DBGPRINTF("USANDO O FREE CHUNK %llu BUFFER+%llu | SIZE %llu | IS FREE %d", ADDR(used), BOFFSET(used), (uintll)CHUNK_UNKN_LD_SIZE(used), !!CHUNK_UNKN_IS_FREE(used));

    DBGPRINTF("CHUNK_FREE_LD_SIZE(used) = %llu", (uintll)CHUNK_FREE_LD_SIZE(used));
    DBGPRINTF("CHUNK_FREE_LD_SIZE2(used, ) = %llu", (uintll)CHUNK_FREE_LD_SIZE2(used, CHUNK_FREE_LD_SIZE(used)));

    ASSERT(BUFFER->heads <= ptr && ptr < BUFFER_HEADS_LMT); DBGPRINTF(":/");
    ASSERT_ADDR_IN_BUFFER (ptr);
    ASSERT(CHUNK_UNKN_IS_FREE(used)); DBGPRINTF(":/");
    ASSERT( CHUNK_FREE_LD_SIZE(used) == CHUNK_FREE_LD_SIZE2(used, CHUNK_FREE_LD_SIZE(used))); DBGPRINTF(":/");
    ASSERT(*CHUNK_FREE_LD_PTR (used) == used);
    //ASSERT(CHUNK_FREE(used)->next == NULL || CHUNK_FREE((CHUNK_FREE(used)->next))->ptr == &CHUNK_FREE(used)->next);
    ASSERT(CHUNK_SIZE_MIN <= CHUNK_FREE_LD_SIZE(used) && CHUNK_FREE_LD_SIZE(used) <= CHUNK_SIZE_MAX && (CHUNK_FREE_LD_SIZE(used) % 8) == 0);
    ASSERT_ADDR_IN_CHUNKS(used);
    ASSERT_ADDR_IN_BUFFER(CHUNK_FREE_LD_PTR(used));
    //ASSERT_ADDR_IN_CHUNKS(CHUNK_FREE(used)->next);

    // TAMANHO DESTE CHUNK FREE
    u64 usedSize = CHUNK_FREE_LD_SIZE(used);

    DBGPRINTF("ESTE CHUNK LIVRE TEM TAMANHO %llu", (uintll)usedSize);

    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = usedSize - size;

    DBGPRINTF("SE TIRAR %llu ELE FICARIA COM %llu", (uintll)size, (uintll)freeSizeNew);

    if (CHUNK_SIZE_MIN <= freeSizeNew) {
        // CONSOME UMA PARTE, NO FINAL DELE
        CHUNK_FREE_ST_SIZE (used, freeSizeNew);
        CHUNK_FREE_ST_SIZE2(used, freeSizeNew);
        DBGPRINTF("CONSUMIU O FINAL; DEIXOU O FREE COMO CHUNK %llu BUFFER+%llu | SIZE %llu / %llu | IS FREE %d", ADDR(used), BOFFSET(used), (uintll)CHUNK_FREE_LD_SIZE(used), (uintll)CHUNK_FREE_LD_SIZE2(used, freeSizeNew), !!CHUNK_UNKN_IS_FREE(used));
        used = CHUNK_RIGHT(used, freeSizeNew);
        usedSize = size;
    } else { // CONSOME ELE: REMOVE ELE DE SUA LISTA
        if (CHUNK_FREE_LD_NEXT(used))
            CHUNK_FREE_ST_PTR(CHUNK_FREE_LD_NEXT(used), CHUNK_FREE_LD_PTR(used));
        *CHUNK_FREE_LD_PTR(used) = CHUNK_FREE_LD_NEXT(used);
        DBGPRINTF("CONSUMIU O FREE INTEIRO; USANDO ELE");
    }

    CHUNK_USED_ST_SIZE (used, usedSize);
    CHUNK_USED_ST_SIZE2(used, usedSize);

    DBGPRINTF("ALOCOU CHUNK %llu BUFFER+%llu | SIZE %llu IS FREE %d", ADDR(used), BOFFSET(used), (uintll)CHUNK_USED_LD_SIZE(used), !!CHUNK_UNKN_IS_FREE(used));

    return CHUNK_USED_DATA(used);
}

// If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
// If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.
void* calloc (size_t n, size_t size_) {

    DBGPRINT("MALLOC - CALLOC");

    const u64 size = (u64)n * (u64)size_;

    void* const data = malloc(size);

    // INITIALIZE IT
    if (data)
        memset(data, 0, size);

    return data;
}

// The  realloc()  function returns a pointer to the newly allocated memory, which is suitably aligned for any built-in type, or NULL if the request failed.
// The returned pointer may be the same as ptr if the allocation was not moved (e.g., there was room to expand the allocation in-place),
//    or different from ptr if the allocation was moved to a new address.
// If size was equal to 0, either NULL or a pointer suitable to be passed to free() is returned.
// If realloc() fails, the original block is left untouched; it is not freed or moved.
void* realloc (void* const data_, const size_t dataSizeNew_) {

    DBGPRINT("MALLOC - REALLOC");

    if (data_ == NULL)
        return malloc(dataSizeNew_);

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 sizeNew = CHUNK_SIZE_FROM_DATA_SIZE(dataSizeNew_);

    // FOI NOS PASSADO O DATA; VAI PARA O CHUNK
    void* const chunk = CHUNK_USED_FROM_DATA(data_);

    //
    u64 size = CHUNK_USED_LD_SIZE(chunk);

    if (size >= sizeNew) {
        // ELE SE AUTOFORNECE
        if ((size - sizeNew) < 265)
            // MAS NÃO VALE A PENA DIMINUIR
            return data_;
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return data_;
    }

    void* right = CHUNK_RIGHT(chunk, size);

    // SÓ SE FOR FREE E SUFICIENTE
    if (CHUNK_UNKN_IS_FREE_AND_NOT_LR(right)) {

        const u64 rightSize = CHUNK_FREE_LD_SIZE(right);
        // O QUANTO VAMOS TENTAR RETIRAR DA DIREITA
        const u64 sizeNeeded = sizeNew - size;

        if (rightSize >= sizeNeeded) {
            // DÁ SIM; VAMOS ALOCAR ARRANCANDO DA DIREITA

            // O TAMANHO NOVO DA DIREITA
            const u64 rightSizeNew = rightSize - sizeNeeded;

            if (rightSizeNew >= CHUNK_SIZE_MIN) {
                // PEGA ESTE PEDAÇO DELE, PELA ESQUERDA
                size += sizeNeeded;
                // LEMBRA QUAIS ERAM
                void** const ptr  = CHUNK_FREE_LD_PTR (right);
                void*  const next = CHUNK_FREE_LD_NEXT(right);
                // MOVE O COMEÇO PARA A DIREITA
                right += sizeNeeded;
                // REESCREVE
                CHUNK_FREE_ST_SIZE (right, rightSizeNew);
                CHUNK_FREE_ST_PTR  (right, ptr);
                CHUNK_FREE_ST_NEXT (right, next);
                CHUNK_FREE_ST_SIZE2(right, rightSizeNew);
                // RELINKA
                if (CHUNK_FREE_LD_NEXT(right))
                    CHUNK_FREE_ST_PTR(CHUNK_FREE_LD_NEXT(right), CHUNK_FREE_LD_NEXT_(right));
                *CHUNK_FREE_LD_PTR(right) = right;
            } else { // CONSOME ELE POR INTEIRO
                size += rightSize;
                // REMOVE ELE DE SUA LISTA
                if (CHUNK_FREE_LD_NEXT(right))
                    CHUNK_FREE_ST_PTR(CHUNK_FREE_LD_NEXT(right), CHUNK_FREE_LD_PTR(right));
                *CHUNK_FREE_LD_PTR(right) = CHUNK_FREE_LD_NEXT(right);
            }

            // REESCREVE ELE
            CHUNK_USED_ST_SIZE (chunk, size);
            CHUNK_USED_ST_SIZE2(chunk, size);

            return CHUNK_USED_DATA(chunk);
        }
    }

    // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
    void* const data = malloc(dataSizeNew_);

    if (data) {
        // CONSEGUIU
        // COPIA DO CHUNK ORIGINAL
        memcpy(data, data_, CHUNK_USED_DATA_SIZE(size));
        // LIBERA O CHUNK ORIGINAL
        free(chunk);
    }

    return data;
}

void* reallocarray (void *ptr, size_t nmemb, size_t size) {

    DBGPRINT("MALLOC - REALLOCARRAY");

    (void)ptr;
    (void)nmemb;
    (void)size;
    abort();
}

void sync (void) {

}

int syncfs (int fd) {

    (void)fd;

    return 0;
}


// POR QUE PRECISA DO MSYNC???

//  começa depois disso
//   usar = sizeof(Buffer) + 8 + BUFF->mallocSize + 8;
// le todos os seus de /proc/self/maps
//      if ((usar + ORIGINALSIZE) > BUFFER_LMT)
//            abort();
//      pwrite(BUFFER_FD, ORIGINALADDR, ORIGINALSIZE, usar);
//   agora remapeia
//      mmap(ORIGINALADDR, ORIGINALSIZE, prot, flags, BUFFER_FD, usar);
//      usar += ORIGINALSIZE;

// TODO: FIXME: TODA VEZ QUE FOR VER O LEFT()/RIGHT(), TOMAR CUIDADO POIS PODE SER O LR
