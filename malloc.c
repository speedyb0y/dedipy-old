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
#include <errno.h>

#include "util.h"

#include "ms.h"

static int initialize = 0;

// TODO: FIXME: WE WILL NEED A SIGNAL HANDLER
static void initializer (void) {

    if (initialize) {
        initialize = 0;

        DBGPRINT("MALLOC - INITIALIZING");

        u64 start = 0, size = 0;

        if (!(sscanf(getenv("SLAVEPARAMS"), "%016llX" "%016llX", (uintll*)&start, (uintll*)&size) == 2 && start && size))
            abort();

        // AGORA SIM MAPEIA
        if (mmap(BUFFER, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_LOCKED | MAP_POPULATE, BUFFER_FD, start) != BUFFER)
            abort();

        if (BUFFER_INFO_PID != getpid())
            abort();

        { //
            char name[256];
            snprintf(name, sizeof(name), PROGNAME "#%u", (uint)BUFFER_INFO_ID);
            //
            if (prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0))
                abort();
        }

        DBGPRINT("MALLOC - INITIALIZING - DONE");
    }
}

// BASEADO NESTE SIZE, SELECINAR UM PTR
// A PARTIR DESTE PTR É GARANTIDO QUE TODOS OS CHUNKS TENHAM ESTE TAMANHO
// JAMAIS É size < FIRST_SIZE
// JAMAIS É size > LAST_SIZE
static inline Chunk** head (u64 size) {

    uint n = N_START;
    uint x = 0;

    while (size >= (((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR))) { // 1 <<n --&gt; 2^n
       x = (x + X_SALT) % X_LAST;
       n += x == 0;
    }

    // voltasGrandes*(voltasMiniN) + voltasMini - (1 pq é para usar o antes deste)
    return &BUFFER_HEADS[(n - N_START)*(X_LAST/X_SALT) + x/X_SALT - 1];
}

void free (void* const data) {

    DBGPRINT("MALLOC - FREE");

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        Chunk* chunk = CHUNK_FROM_CHUNK_USED_DATA(data);

        u64 size = CHUNK_UNKN_SIZE(chunk);

        // JOIN WITH THE LEFT CHUNK
        Chunk* const left = CHUNK_LEFT(chunk);

        if (CHUNK_IS_FREE(left)) {
            size += CHUNK_FREE_SIZE(left);
            if (CHUNK_FREE(left)->next)
                CHUNK_FREE(CHUNK_FREE(left)->next)->ptr = CHUNK_FREE(left)->ptr;
           *CHUNK_FREE(left)->ptr = CHUNK_FREE(left)->next;
            chunk = left;
        }

        // JOIN WITH THE RIGHT CHUNK
        Chunk* const right = CHUNK_RIGHT(chunk, size);

        if (CHUNK_IS_FREE(right)) {
            size += CHUNK_FREE_SIZE(right);
            if (CHUNK_FREE(right)->next)
                CHUNK_FREE(CHUNK_FREE(right)->next)->ptr = CHUNK_FREE(right)->ptr;
           *CHUNK_FREE(right)->ptr = CHUNK_FREE(right)->next;
        }

        // WRITE THE SIZE
        CHUNK_FREE(chunk      )->size = CHUNK_SIZE_FREE(size);
        CHUNK_TAIL(chunk, size)->size = CHUNK_SIZE_FREE(size);

        // INSERE ELE NA LISTA DE FREES
        Chunk** const ptr = head(size);

        CHUNK_FREE(chunk)->ptr = ptr;
        CHUNK_FREE(chunk)->next = *ptr;

        if (*ptr)
           CHUNK_FREE(*ptr)->ptr = &CHUNK_FREE(chunk)->next;

        *ptr = chunk;
    }
}

void* malloc (size_t size_) {

    DBGPRINT("MALLOC - MALLOC");

    initializer();

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 size = size_ > CHUNK_SIZE_MIN ? (((u64)size_ + 7ULL) & ~0b111ULL) : CHUNK_SIZE_MIN;

    // PODE ATÉ TER, MAS NÃO É GARANTIDO
    // TERIA QUE IR PARA O ÚLTIMO, E ALI PROCURAR UM QUE SE ENCAIXE
    // MAS PREFIRO MANTER OTIMIZADO DO QUE SUPORTAR ISSO
    if (size > CHUNK_SIZE_MAX) // >= LMT_SIZE
        return NULL;

    // PEGA UM LIVRE A SER USADO
    Chunk* used;

    // ENCONTRA A PRIMEIRA LISTA LIVRE
    Chunk** ptr = head(size);

    while ((used = *ptr) == NULL)
        ptr++;

    // TAMANHO DESTE CHUNK FREE
    u64 usedSize = CHUNK_FREE_SIZE(used);

    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = usedSize - size;

    if (CHUNK_SIZE_MAX <= freeSizeNew) {
        // CONSOME UMA PARTE, NO FINAL DELE
        CHUNK_FREE(used             )->size = CHUNK_SIZE_FREE(freeSizeNew);
        CHUNK_TAIL(used, freeSizeNew)->size = CHUNK_SIZE_FREE(freeSizeNew);
        //
        used = CHUNK((void*)used + freeSizeNew);
        usedSize = size;
    } else { // CONSOME ELE: REMOVE ELE DE SUA LISTA
        if (CHUNK_FREE(used)->next)
            CHUNK_FREE(CHUNK_FREE(used)->next)->ptr = CHUNK_FREE(used)->ptr;
        *CHUNK_FREE(used)->ptr = CHUNK_FREE(used)->next;
    }

    CHUNK_USED(used          )->size = CHUNK_SIZE_USED(usedSize);
    CHUNK_TAIL(used, usedSize)->size = CHUNK_SIZE_USED(usedSize);

    return CHUNK_USED(used)->data;
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
    Chunk* const chunk = CHUNK_FROM_CHUNK_USED_DATA(data_);

    //
    u64 size = CHUNK_USED_SIZE(chunk);

    if (size >= sizeNew) {
        // ELE SE AUTOFORNECE
        if ((size - sizeNew) < 265)
            // MAS NÃO VALE A PENA DIMINUIR
            return data_;
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return data_;
    }

    Chunk* right = CHUNK_RIGHT(chunk, size);

    // SÓ SE FOR FREE E SUFICIENTE
    if (CHUNK_IS_FREE(right)) {

        const u64 rightSize = CHUNK_FREE_SIZE(right);
        // O QUANTO VAMOS TENTAR RETIRAR DA DIREITA
        const u64 sizeNeeded = sizeNew - size;

        if (rightSize >= sizeNeeded) {
            // DÁ SIM; VAMOS ALOCAR ARRANCANDO DA DIREITA

            // O TAMANHO NOVO DA DIREITA
            const u64 rightSizeNew = rightSize - sizeNeeded;

            if (rightSizeNew >= CHUNK_SIZE_MAX) {
                // PEGA ESTE PEDAÇO DELE, PELA ESQUERDA
                size += sizeNeeded;
                // LEMBRA QUAIS ERAM
                Chunk** const ptr = CHUNK_FREE(right)->ptr;
                Chunk* const next = CHUNK_FREE(right)->next;
                // MOVE O COMEÇO PARA A DIREITA
                right = CHUNK((void*)right + sizeNeeded);
                // REESCREVE
                CHUNK_FREE(right)->size = CHUNK_SIZE_FREE(rightSizeNew);
                CHUNK_FREE(right)->ptr = ptr;
                CHUNK_FREE(right)->next = next;
                CHUNK_TAIL(right, rightSizeNew)->size = CHUNK_SIZE_FREE(rightSizeNew);
                // RELINKA
                if (CHUNK_FREE(right)->next)
                    CHUNK_FREE(CHUNK_FREE(right)->next)->ptr = &CHUNK_FREE(right)->next;
                *CHUNK_FREE(right)->ptr = right;
            } else { // CONSOME ELE POR INTEIRO
                size += rightSize;
                // REMOVE ELE DE SUA LISTA
                if (CHUNK_FREE(right)->next)
                    CHUNK_FREE(CHUNK_FREE(right)->next)->ptr = CHUNK_FREE(right)->ptr;
                *CHUNK_FREE(right)->ptr = CHUNK_FREE(right)->next;
            }

            // REESCREVE ELE
            CHUNK_USED(chunk      )->size = CHUNK_SIZE_USED(size);
            CHUNK_TAIL(chunk, size)->size = CHUNK_SIZE_USED(size);

            return CHUNK_USED(chunk)->data;
        }
    }

    // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
    void* const data = malloc(dataSizeNew_);

    if (data) {
        // CONSEGUIU
        // COPIA DO CHUNK ORIGINAL
        memcpy(data, data_, size - sizeof(ChunkUsed) - sizeof(ChunkTail));
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
