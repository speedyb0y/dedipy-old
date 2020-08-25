
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define loop while (1)
#define elif else if

typedef unsigned int uint;

typedef uint64_t u64;

#define CHUNK_SIZE_SIZE(size)    ((size) & ~1ULL)
#define CHUNK_SIZE_IS_FREE(size) ((size) &  1ULL)

#define CHUNK_SIZE_FREE(size)  (size)
#define CHUNK_SIZE_USED(size) ((size) | 1ULL)

// LE O SIZE COM A FLAG DE FREE
#define CHUNK_SIZE(chunk)        (*(   u64*)        (chunk)      )
#define CHUNK_PTR(chunk)         (*(void***)((void*)(chunk) +  8))
#define CHUNK_NEXT(chunk)        (*( void**)((void*)(chunk) + 16))
#define CHUNK_SIZE2(chunk, size) (*(   u64*)((void*)(chunk) + (size) - 8))

#define CHUNK_LEFT(chunk)         ((void*)(chunk) - CHUNK_SIZE_SIZE(CHUNK_LEFT_SIZE(chunk)))
#define CHUNK_RIGHT_(chunk)       ((void*)(chunk) + CHUNK_SIZE_SIZE(CHUNK_SIZE(chunk))
#define CHUNK_RIGHT(chunk, size)  ((void*)(chunk) + (size))

// LE O SIZE COM A FLAG DE FREE
#define CHUNK_LEFT_SIZE(chunk)         (*(u64*)((void*)(chunk) - 8))
#define CHUNK_RIGHT_SIZE(chunk, size)  (*(u64*)((void*)(chunk) + (size)))

#define FIRST_SIZE  0x00000010ULL
#define SECOND_SIZE 0x00000013ULL
#define LAST_SIZE   0x2CCCCCCCULL
#define LMT_SIZE    0x33333333ULL

#define N_START 4

// PARA FICAR UNIFORME TEM QUE SER = (X_COUNT*X_SALT)
#define X_DIVISOR 5
#define X_SALT 1
#define X_LAST 5

#define HEADS_N 128

//
#define BUFFER ((void*)20000000ULL)

static int cpu = -1;

void free (void* chunk) {

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

    // --- BASEADO NESTE size, SELECINAR UM PREV
    uint idx;

    if (size < SECOND_SIZE) // JAMAIS É  <  FIRST_SIZE
       idx = 0;
    elif (size >= LAST_SIZE)
       idx = HEADS_N - 1;
    else {
        uint n = N_START;
        uint x = 0;

        while (size >= (((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR))) { // 1 <<n --&gt; 2^n
           x = (x + X_SALT) % X_LAST;
           n += x == 0;
        }
        // voltasGrandes*(voltasMiniN) + voltasMini - (1 pq é para usar o antes deste)
        idx = (n - N_START)*(X_LAST/X_SALT) + x/X_SALT - 1;
    }

    void** ptr = (void**)(BUFFER + idx*8);

    CHUNK_PTR (chunk) =  ptr;
    CHUNK_NEXT(chunk) = *ptr;

    if (*ptr)
       CHUNK_PTR(*ptr) = &CHUNK_NEXT(chunk);

    *ptr = chunk;
}

void* calloc (size_t n, size_t size) {

    // The  calloc()  function allocates memory for an array of nmemb elements of size bytes each and returns a pointer to the allocated memory.  The memory is set to zero.  If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to
    // free().  If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.  By contrast, an integer overflow would not be detected in the following call to malloc(), with the result that an incorrectly sized block of memory would be allocated:

      return malloc(n * size);
}

// The  realloc()  function returns a pointer to the newly allocated memory, which is suitably aligned for any built-in type, or NULL if the request failed.
// The returned pointer may be the same as ptr if the allocation was not moved (e.g., there was room to expand the allocation in-place),
//    or different from ptr if the allocation was moved to a new address.
// If size was equal to 0, either NULL or a pointer suitable to be passed to free() is returned.
// If realloc() fails, the original block is left untouched; it is not freed or moved.
void* realloc (void* chunk, size_t sizeWanted_) {

    if (sizeWanted_ == 0)
        return NULL;

    // TODO: FIXME: ESTÁ CERTO ISSO?
    if (chunk == NULL)
        return malloc(sizeWanted_);

    // LIDÁ SOMENTE COM TAMANHO TOTAL DO CHUNK; E SE PRECISAR, ALINHA
    const u64 sizeWanted = ((u64)sizeWanted_ + 16 + 7) & ~0b111ULL;

    //
    chunk -= 8;

    u64 size = CHUNK_SIZE(chunk);

    if (sizeWanted > size) {

        // SE PRECISAR, ALINHA
        u64 sizeNeeded = ((sizeWanted - size) + 7) & ~0b111ULL;

        void** const ptr  = CHUNK_PTR (chunk);
        void*  const next = CHUNK_NEXT(chunk);

        void* left  = CHUNK_LEFT (chunk);
        void* right = CHUNK_RIGHT(chunk, size);

        void** const leftPtr  = CHUNK_PTR(left);
        void** const rightPtr = CHUNK_PTR(right);

        void* const leftNext  = CHUNK_NEXT(left);
        void* const rightNext = CHUNK_NEXT(right);

        const u64 leftSize  = CHUNK_SIZE_IS_FREE(CHUNK_SIZE(left))  ? CHUNK_SIZE_SIZE(CHUNK_SIZE(left))  : 0;
        const u64 rightSize = CHUNK_SIZE_IS_FREE(CHUNK_SIZE(right)) ? CHUNK_SIZE_SIZE(CHUNK_SIZE(right)) : 0;

        if (sizeWanted <= (size + rightSize)) {
            // AUMENTA PELA DIREITA

            const u64 rightSizeNew = rightSize - sizeNeeded;

            if (rightSizeNew < 32) {
                // CONSOME ELE POR INTEIRO
                size += rightSize;
                // REMOVE ELE DE SUA LISTA
                if (rightNext)
                    CHUNK_PTR(rightNext) = rightPtr;
                *rightPtr = rightNext;
            } else { // PEGA ESTE PEDAÇO DELE
                size  += sizeNeeded;
                right += sizeNeeded;
                // REESCREVE E RELINKA ELE
                CHUNK_SIZE (right)               = CHUNK_SIZE_FREE(rightSizeNew);
                CHUNK_PTR  (right)               = rightPtr;
                CHUNK_NEXT (right)               = rightNext;
                CHUNK_SIZE2(right, rightSizeNew) = CHUNK_SIZE_FREE(rightSizeNew);
                CHUNK_PTR(rightNext) = &CHUNK_NEXT(right);
                *rightPtr = right;
            }

        } elif (sizeWanted <= (size + leftSize)) {
            // AUMENTA PELA ESQUERDA

            const u64 leftSizeNew = leftSize - sizeNeeded;

            if (leftSizeNew < 32) {
                // CONSOME ELE POR INTEIRO
                sizeNeeded += leftSize;
                // REMOVE ELE DE SUA LISTA
                if (leftNext)
                    CHUNK_PTR(leftNext) = leftPtr;
                *leftPtr = leftNext;
            } else { // PEGA ESTE PEDAÇO DELE
                // REESCREVE ELE; SOMENTE O SIZE MUDOU
                // CONTINUA NO MESMO LUGAR; NÃO PRECISA RELINKAR
                CHUNK_SIZE (left)              = CHUNK_SIZE_FREE(leftSizeNew);
                CHUNK_SIZE2(left, leftSizeNew) = CHUNK_SIZE_FREE(leftSizeNew);
            }

            // MOVE, OTIMIZADO
            memmove(chunk - sizeNeeded, chunk, size);

            chunk -= sizeNeeded;
            size  += sizeNeeded;

        } elif (sizeWanted <= (size + leftSize + rightSize)) {
            // AUMENTA POR AMBOS
            abort();
        } else
            return NULL;

        // REESCREVE ELE
        CHUNK_SIZE (chunk)       = CHUNK_SIZE_FREE(size);
        CHUNK_PTR  (chunk)       = ptr;
        CHUNK_NEXT (chunk)       = next;
        CHUNK_SIZE2(chunk, size) = CHUNK_SIZE_FREE(size);
        // RELINKA ELE
        CHUNK_PTR(next) = &CHUNK_NEXT(chunk);
        *ptr = chunk;
    }

    return chunk + 8;
}

void* reallocarray (void *ptr, size_t nmemb, size_t size) {
    (void)ptr;
    (void)nmemb;
    (void)size;
    abort();
}

void* malloc (size_t size) {

  (void)size;

  abort();

  return NULL;
}

void sync (void) {

}

int syncfs (int cpu_) {

    if (cpu != -1) {
        // AINDA NÃO INICIALIZOU
        cpu = cpu_;
        // SCHED AFFINITY CPU
        // SCHED AFFINITY FIFO
        // MMMAP ...
        if (BUFFER != BUFFER) {
            abort();
        }
    }

    return 0;
}
