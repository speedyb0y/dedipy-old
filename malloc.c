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
#include <errno.h>

#include "util.h"

#include "malloc.h"

// A CPU 0 DEVE SER DEIXADA PARA O KERNEL, INTERRUPTS, E ADMIN
static uint processID = 0;
static uint processesN = 0;
static u64 processPID = 0;
static u64 processCode = 0;

static void initializer (void) {
    if (processPID == 0) {
        // AINDA NÃO INICIALIZOU

        WRITESTR("INITIALIZING");

        //
        processPID = getpid();

        //
        BufferProcessInfo processInfo;

        if (read(SELF_RD_FD, &processInfo, sizeof(processInfo)) != sizeof(processInfo))
            abort();

        //
        if (processPID != processInfo.pid)
            abort();

        processID = processInfo.id;
        processesN = processInfo.n;
        processCode = processInfo.code;

        // AGORA SIM MAPEIA
        if (mmap(BUFFER, processInfo.size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, BUFFER_FD, processInfo.start) != BUFFER)
            abort();

        //  BUFFER                       BUFFER + processSize
        // |____________________________|
        // | HEADS | 0 |    CHUNK   | 0 |

        // HEADS
        memset(BUFFER_HEADS, 0, BUFFER_HEADS_SIZE);

        // LEFT AND RIGHT
        *(u64*)BUFFER_L                   = 0;
        *(u64*)BUFFER_R(processInfo.size) = 0;

        *(void**)BUFFER_HEADS_LAST = BUFFER_CHUNKS;

        const u64 size = processInfo.size - BUFFER_HEADS_SIZE - 8 - 8;

        CHUNK_SIZE (BUFFER_CHUNKS)       = CHUNK_SIZE_FREE(size);
        CHUNK_PTR  (BUFFER_CHUNKS)       = BUFFER_HEADS_LAST;
        CHUNK_NEXT (BUFFER_CHUNKS)       = NULL;
        CHUNK_SIZE2(BUFFER_CHUNKS, size) = CHUNK_SIZE_FREE(size);

        WRITESTR("INITIALIZING - DONE");
    }
}

void free (void* chunk) {

    WRITESTR("FREE");

    if (chunk == NULL)
        return;

    // VAI PARA O COMEÇO DO CHUNK
    chunk -= 8;

    // NÃO PRECISA REMOVER A FLAG DE FREE POIS NÃO É FREE
    u64 size = CHUNK_SIZE(chunk);

    // JOIN WITH THE LEFT CHUNK
    u64 leftSize = CHUNK_LEFT_SIZE(chunk);

    if (CHUNK_SIZE_IS_FREE(leftSize)) {
       leftSize = CHUNK_SIZE_SIZE(leftSize);
       size += leftSize;
       chunk -= leftSize;
        if (CHUNK_NEXT(chunk))
            CHUNK_PTR(CHUNK_NEXT(chunk)) = CHUNK_PTR(chunk);
       *CHUNK_PTR(chunk) = CHUNK_NEXT(chunk);
    }

    // JOIN WITH THE RIGHT CHUNK
    void* const right = CHUNK_RIGHT(chunk, size);

    u64 rightSize = CHUNK_SIZE(right);

    if (CHUNK_SIZE_IS_FREE(rightSize)) {
        size += CHUNK_SIZE_SIZE(rightSize);
        if (CHUNK_NEXT(right)) // SE TEM UM FILHO,
            CHUNK_PTR(CHUNK_NEXT(right)) = CHUNK_PTR(right); // O PAI DO MEU FILHO É O MEU PAI
       *CHUNK_PTR(right) = CHUNK_NEXT(right); // E O FILHO DO MEU PAI É O MEU FILHO
    }

    // WRITE THE SIZE
    CHUNK_SIZE(chunk) = CHUNK_SIZE2(chunk, size) = CHUNK_SIZE_FREE(size);

    // BASEADO NESTE SIZE, SELECINAR UM PTR
    void** ptr;

    if (size < SECOND_SIZE) // JAMAIS É  <  FIRST_SIZE
       ptr = BUFFER;
    elif (size >= LAST_SIZE)
       ptr = BUFFER + (HEADS_N - 1)*8;
    else {
        uint n = N_START;
        uint x = 0;

        while (size >= (((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR))) { // 1 <<n --&gt; 2^n
           x = (x + X_SALT) % X_LAST;
           n += x == 0;
        }
        // voltasGrandes*(voltasMiniN) + voltasMini - (1 pq é para usar o antes deste)
        ptr = BUFFER + ((n - N_START)*(X_LAST/X_SALT) + x/X_SALT - 1)*8;
    }

    CHUNK_PTR (chunk) =  ptr;
    CHUNK_NEXT(chunk) = *ptr;

    if (*ptr)
       CHUNK_PTR(*ptr) = &CHUNK_NEXT(chunk);

    *ptr = chunk;
}

static void* malloc_ (u64 size) {

    initializer();

    //
    size = ((size < 16 ? 16 : size) + 16 + 7) & ~0b111ULL;

    // PODE ATÉ TER, MAS NÃO É GARANTIDO
    // TERIA QUE IR PARA O ÚLTIMO, E ALI PROCURAR UM QUE SE ENCAIXE
    // MAS PREFIRO MANTER OTIMIZADO DO QUE SUPORTAR ISSO
    if (size > LAST_SIZE) // >= LMT_SIZE
        return NULL;

    // BASEADO NESTE SIZE, SELECINAR UM PTR
    void** ptr;

    if (size < SECOND_SIZE) // JAMAIS É  <  FIRST_SIZE
       ptr = BUFFER;
    else {
        uint n = N_START;
        uint x = 0;

        while (size >= (((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR))) { // 1 <<n --&gt; 2^n
           x = (x + X_SALT) % X_LAST;
           n += x == 0;
        }
        // voltasGrandes*(voltasMiniN) + voltasMini - (1 pq é para usar o antes deste)
        ptr = BUFFER + ((n - N_START)*(X_LAST/X_SALT) + x/X_SALT - 1)*8;
    }

    // A PARTIR DESTE PTR É GARANTIDO QUE TODOS OS CHUNKS TENHAM ESTE TAMANHO
    void* free;

    // ENCONTRA A PRIMEIRA LISTA LIVRE
    while ((free = *ptr) == NULL)
        ptr++;

    // TAMANHO DESTE CHUNK FREE
    u64 freeSize = CHUNK_SIZE_SIZE(CHUNK_SIZE(free));

    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = freeSize - size;

    if (freeSizeNew >= 32) { // CONSOME UMA PARTE; SÓ PRECISA REESCREVER O TAMANHO
        CHUNK_SIZE (free)               = CHUNK_SIZE_FREE(freeSizeNew);
        CHUNK_SIZE2(free, freeSizeNew)  = CHUNK_SIZE_FREE(freeSizeNew);
        //
        free += freeSizeNew;
        freeSize = size;
    } else { // CONSOME ELE
        // REMOVE ELE DE SUA LISTA
        if (CHUNK_NEXT(free))
            CHUNK_PTR(CHUNK_NEXT(free)) = CHUNK_PTR(free);
        *CHUNK_PTR(free) = CHUNK_NEXT(free);
    }

    CHUNK_SIZE (free)           = CHUNK_SIZE_USED(freeSize);
    CHUNK_SIZE2(free, freeSize) = CHUNK_SIZE_USED(freeSize);

    return free + 8;
}

void* malloc (size_t size) {

    WRITESTR("MALLOC");

    return malloc_((u64)size);
}

// If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
// If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.
void* calloc (size_t n, size_t size_) {

    WRITESTR("CALLOC");

    const u64 size = (u64)n * (u64)size_;

    void* const data = malloc_(size);

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
void* realloc (void* chunk, size_t sizeWanted_) {

    WRITESTR("REALLOC");

    if (sizeWanted_ == 0)
        return NULL;

    // TODO: FIXME: ESTÁ CERTO ISSO?
    if (chunk == NULL)
        return malloc(sizeWanted_);

    // LIDÁ SOMENTE COM TAMANHO TOTAL DO CHUNK
    // QUANDO VIRAR UM FREE, VAI PRECISAR DE 2*8 BYTES, ENTÃO ESTE É O MÍNIMO
    // ALINHADO A 8 BYTES
    const u64 sizeWanted = ((u64)(sizeWanted_ < 16 ? 16 : sizeWanted_) + 16 + 7) & ~0b111ULL;

    // FOI NOS PASSADO O DATA; VAI PARA O CHUNK
    chunk -= 8;

    // JA SABEMOS QUE ESTE CHUNK ESTÁ EM USO, ENTÃO NÃO PRECISA REMOVER A FLAG DE FREE
    u64 size = CHUNK_SIZE(chunk);

    // SE JÁ TEM, NÃO PRECISA FAZER NADA
    if (sizeWanted <= size) {
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return chunk + 8;
    }

    // SE PRECISAR, ALINHA
    u64 sizeNeeded = ((sizeWanted - size) + 7) & ~0b111ULL;

    void* right = CHUNK_RIGHT(chunk, size);

    // LE O TAMANHO SE FOR LIVRE, OU 0 SE ESTIVER EM USO
    const u64 rightSize = CHUNK_SIZE_IS_FREE(CHUNK_SIZE(right)) ? CHUNK_SIZE_SIZE(CHUNK_SIZE(right)) : 0;

    if (sizeWanted > (size + rightSize)) {
        // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
        void* const data = malloc(sizeWanted - 16);
        // CONSEGUIU?
        if (data) {
            // COPIA ELE
            memcpy(data, chunk + 8, size - 16);
            // SE LIVRA DELE
            free(chunk);
        } //
        return data;
    }

    // AUMENTA PELA DIREITA
    const u64 rightSizeNew = rightSize - sizeNeeded;

    if (rightSizeNew < 32) {
        // CONSOME ELE POR INTEIRO
        size += rightSize;
        // REMOVE ELE DE SUA LISTA
        if (CHUNK_NEXT(right))
            CHUNK_PTR(CHUNK_NEXT(right)) = CHUNK_PTR(right);
        *CHUNK_PTR(right) = CHUNK_NEXT(right);
    } else { // PEGA ESTE PEDAÇO DELE
        size  += sizeNeeded;
        right += sizeNeeded;
        // REESCREVE E RELINKA ELE
        CHUNK_SIZE (right)               = CHUNK_SIZE_FREE(rightSizeNew);
        CHUNK_PTR  (right)               = CHUNK_PTR (right);
        CHUNK_NEXT (right)               = CHUNK_NEXT(right);
        CHUNK_SIZE2(right, rightSizeNew) = CHUNK_SIZE_FREE(rightSizeNew);
        CHUNK_PTR(CHUNK_NEXT(right))    = &CHUNK_NEXT(right);
       *CHUNK_PTR(right) = right;
    }

    // REESCREVE ELE
    CHUNK_SIZE (chunk)       = CHUNK_SIZE_USED(size);
    CHUNK_SIZE2(chunk, size) = CHUNK_SIZE_USED(size);

    return chunk + 8;
}

void* reallocarray (void *ptr, size_t nmemb, size_t size) {

    WRITESTR("REALLOCARRAY");

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
