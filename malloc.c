/*

  TODO: FIXME: reduzir o PTR e o NEXT para um u32, múltiplo de 16
  TODO: FIXME: interceptar signal(), etc. quem captura eles somos nós. e quando possível, executamos a função deles
  TODO: FIXME: PRECISA DO MSYNC?
  TODO: FIXME: INTERCEPTAR ABORT()
  TODO: FIXME: INTERCEPTAR EXIT()
  TODO: FIXME: INTERCEPTAR _EXIT()

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
    if (!(sscanf(getenv("SLAVEPARAMS"), // TODO: FIXME: PEGAR SOMENTE O START E O SIZE; O RESTANTE É INICIALIZADO NO MASTER
        "%02X"    "%02X"   "%02X"         "%02X"        "%016llX"      "%016llX"   "%02X"    "%016llX"     "%016llX",
        &slaveID, &slaveN, &slaveGroupID, &slaveGroupN, &slaveStarted, &slaveCode, &slaveCPU, &slaveStart, &slaveSize) == 9 && slaveStart && slaveSize &&
        mmap(BUFFER_INFO, slaveSize, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_LOCKED | MAP_POPULATE, BUFFER_FD, slaveStart) == BUFFER_INFO &&
        slaveCPU == sched_getcpu() &&
        snprintf(name, sizeof(name), PROGNAME "#%u", (uint)BUFFER_INFO->id) &&
        prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0) == 0
        )) abort();

    // TO PROTECT FROM ACCIDENTS
    if (close(BUFFER_FD))
        abort();

    // TODO: FIXME: VERIFICAR O PID TB

    //  BUFFER_INFO                       BUFFER_INFO + processSize
    // |___________________________________|
    // | INFO | HEADS | 0 |    CHUNK   | 0 |

    // INFO AND HEADS
    memset(BUFFER_INFO, 0, sizeof(Buffer));

    // INFO
    BUFFER_INFO->id       = slaveID;
    BUFFER_INFO->n        = slaveN;
    BUFFER_INFO->groupID  = slaveGroupID;
    BUFFER_INFO->groupN   = slaveGroupN;
    BUFFER_INFO->cpu      = slaveCPU;
    BUFFER_INFO->reserved = 0;
    BUFFER_INFO->code     = slaveCode;
    BUFFER_INFO->pid      = slavePID;
    BUFFER_INFO->started  = slaveStarted;
    BUFFER_INFO->start    = slaveStart;
    BUFFER_INFO->size     = slaveSize;

    // LEFT AND RIGHT
    BUFFER_L            = BUFFER_LR_VALUE;
    BUFFER_R(slaveSize) = BUFFER_LR_VALUE;

    // THE INITIAL CHUNK
    // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O CHUNK_SIZE_MAX E ROOTS_SIZES_LST SÃO MAIORES DO QUE ELE
    CHUNK_FREE_FILL_AND_REGISTER(BUFFER_CHUNK0, BUFFER_CHUNK0_SIZE(slaveSize));

    // TODO: FIXME: tentar dar malloc() e realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // CHUNK_SIZE_MAX
    // ROOTS_SIZES_LST
    // ROOTS_SIZES_LMT

    DBGPRINTF("ROOTS_N %llu", (uintll)ROOTS_N);
    DBGPRINTF("ROOTS_SIZES_FST %llu", (uintll)ROOTS_SIZES_FST);
    DBGPRINTF("ROOTS_SIZES_LST %llu", (uintll)ROOTS_SIZES_LST);
    DBGPRINTF("ROOTS_SIZES_LMT %llu", (uintll)ROOTS_SIZES_LMT);

    DBGPRINTF("CHUNK_SIZE_MIN %llu", (uintll)CHUNK_SIZE_MIN);
    DBGPRINTF("CHUNK_SIZE_MAX %llu", (uintll)CHUNK_SIZE_MAX);

    ASSERT (CHUNK_SIZE_MIN == ROOTS_SIZES_FST);
    ASSERT (CHUNK_SIZE_MAX <= ROOTS_SIZES_LST);
    ASSERT (CHUNK_SIZE_MAX <  ROOTS_SIZES_LMT);

    ASSERT (ROOTS_SIZES_FST < ROOTS_SIZES_LST);
    ASSERT (ROOTS_SIZES_LST < ROOTS_SIZES_LMT);

    ASSERT (CHUNK_PTR_ROOT_GET(CHUNK_SIZE_MIN) == (BUFFER_ROOTS));
    ASSERT (CHUNK_PTR_ROOT_GET(CHUNK_SIZE_MAX) == (BUFFER_ROOTS + (ROOTS_N - 1)));

    ASSERT (CHUNK_PTR_ROOT_PUT(CHUNK_SIZE_MIN) == (BUFFER_ROOTS));
    ASSERT (CHUNK_PTR_ROOT_PUT(CHUNK_SIZE_MAX) <= (BUFFER_ROOTS + (ROOTS_N - 1))); // COMO VAI ARREDONDAR PARA BAIXO, PEDIR O MÁXIMO PODE CAIR LOGO ANTES DO ÚLTIMO SLOT

    ASSERT (BUFFER_ROOTS_LMT == (BUFFER_ROOTS + ROOTS_N));

    DBGPRINTF("BUFFER_INFO BX%llX",      BOFFSET(BUFFER_INFO));
    DBGPRINTF("BUFFER_INFO->id %llu",    (uintll)BUFFER_INFO->id);
    DBGPRINTF("BUFFER_INFO->pid %llu",   (uintll)BUFFER_INFO->pid);
    DBGPRINTF("BUFFER_INFO->cpu %llu",   (uintll)BUFFER_INFO->cpu);
    DBGPRINTF("BUFFER_INFO->start %llu", (uintll)BUFFER_INFO->start);
    DBGPRINTF("BUFFER_INFO->size %llu",  (uintll)BUFFER_INFO->size);
    DBGPRINTF("BUFFER_INFO->code %llu",  (uintll)BUFFER_INFO->code);

    DBGPRINTF("BUFFER_ROOTS BX%llX",     BOFFSET(BUFFER_ROOTS));
    DBGPRINTF("BUFFER_ROOTS_LMT BX%llX", BOFFSET(BUFFER_ROOTS_LMT));

    DBGPRINTF(" BUFFER_L %llu",    (uintll)BUFFER_L);
    DBGPRINTF("&BUFFER_L BX%llX", BOFFSET(&BUFFER_L));

    DBGPRINTF(" BUFFER_CHUNK0 BX%llX", BOFFSET(BUFFER_CHUNK0));
    DBGPRINTF(" BUFFER_CHUNK0 SIZE %llu",   (uintll)(CHUNK_FREE_LD_SIZE(BUFFER_CHUNK0)));
    DBGPRINTF(" BUFFER_CHUNK0 PTR BX%llX",  BOFFSET( CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)));
    DBGPRINTF("*BUFFER_CHUNK0 PTR BX%llX",  BOFFSET(*CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)));
    DBGPRINTF(" BUFFER_CHUNK0 NEXT BX%llX", BOFFSET( CHUNK_FREE_LD_NEXT(BUFFER_CHUNK0)));

    DBGPRINTF(" BUFFER_R %llu",    (uintll)BUFFER_R(BUFFER_INFO->size));
    DBGPRINTF("&BUFFER_R BX%llX", BOFFSET(&BUFFER_R(BUFFER_INFO->size)));

    DBGPRINTF("BUFFER_LMT BX%llX", BOFFSET(BUFFER_LMT));

    DBGPRINTF("---");

    ASSERT_ADDR_IN_BUFFER(BUFFER_CHUNK0);
    ASSERT_ADDR_IN_CHUNKS(BUFFER_CHUNK0);

    ASSERT ( sizeof(u64) == 8 );
    ASSERT ( sizeof(void*) == 8 );

    CHUNKS_VERIFY();

    //
    if (dup2(SLAVE_GET_FD(BUFFER_INFO->id), SELF_GET_FD) != SELF_GET_FD ||
        dup2(SLAVE_PUT_FD(BUFFER_INFO->id), SELF_PUT_FD) != SELF_PUT_FD)
        abort();

    // TODO: FIXME: o malloc também tem que ser inicializado no master
}

void free (void* const data) {

    DBGPRINTF("=== FREE(BX%llX) ========================================================================", BOFFSET(data));

    CHUNKS_VERIFY();

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        void* chunk = CHUNK_USED_FROM_DATA(data);

        ASSERT(CHUNK_UNKN_IS_FREE(chunk) == 0);

        u64 size = CHUNK_USED_LD_SIZE(chunk);

        // JOIN WITH THE LEFT CHUNK
        void* const left = CHUNK_LEFT(chunk);

        if (CHUNK_UNKN_IS_FREE(left)) {
            size += CHUNK_FREE_LD_SIZE(left);
            chunk = left;
            CHUNK_FREE_REMOVE(chunk);
        }

        // JOIN WITH THE RIGHT CHUNK
        void* const right = CHUNK_RIGHT(chunk, size);

        if (CHUNK_UNKN_IS_FREE(right)) {
            size += CHUNK_FREE_LD_SIZE(right);
            CHUNK_FREE_REMOVE(right);
        }

        //
        CHUNK_FREE_FILL_AND_REGISTER(chunk, size);
    }
}

void* malloc (size_t size_) {

    if (__builtin_expect_with_probability(initialized, 1, 0.99) == 0)
        initialize();

    DBGPRINTF("=== MALLOC(%llu) ========================================================================", (uintll)size_);

    CHUNKS_VERIFY();

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 size = CHUNK_SIZE_FROM_DATA_SIZE(size_);

    //
    if (size > CHUNK_SIZE_MAX)
        return NULL;

    // PEGA UM LIVRE A SER USADO
    void* used;

    // ENCONTRA A PRIMEIRA LISTA LIVRE
    void** ptr = CHUNK_PTR_ROOT_GET(size);

    // LOGO APÓS O HEADS, HÁ O LEFT CHUNK, COM UM SIZE FAKE 1, PORTANTO ELE É NÃO-NULL, E VAI PARAR SE NAO TIVER MAIS CHUNKS LIVRES
    while ((used = *ptr) == NULL)
        ptr++;

    // SE
    if (ptr == BUFFER_ROOTS_LMT) {
        ASSERT ( used == (void*)BUFFER_LR_VALUE ); // TERÁ LIDO DESTA FORMA
        return NULL;
    }

    ASSERT(BUFFER_ROOTS <= ptr && ptr < BUFFER_ROOTS_LMT);
    ASSERT(CHUNK_UNKN_IS_FREE(used));
    //ASSERT(CHUNK_FREE(used)->next == NULL || CHUNK_FREE((CHUNK_FREE(used)->next))->ptr == &CHUNK_FREE(used)->next);
    ASSERT(CHUNK_SIZE_MIN <= CHUNK_FREE_LD_SIZE(used) && CHUNK_FREE_LD_SIZE(used) <= CHUNK_SIZE_MAX && (CHUNK_FREE_LD_SIZE(used) % 8) == 0);

    //ASSERT_ADDR_IN_CHUNKS(CHUNK_FREE(used)->next);

    // TAMANHO DESTE CHUNK FREE
    u64 usedSize = CHUNK_FREE_LD_SIZE(used);

    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = usedSize - size;

    // REMOVE ELE DE SUA LISTA, MESMO QUE SÓ PEGUEMOS UM PEDAÇO, VAI TER REALOCÁ-LO NA TREE
    CHUNK_FREE_REMOVE(used);

    // SE DER, CONSOME SÓ UMA PARTE, NO FINAL DELE
    if (CHUNK_SIZE_MIN <= freeSizeNew) {
        CHUNK_FREE_FILL_AND_REGISTER(used, freeSizeNew);
        used += freeSizeNew;
        usedSize = size;
    }

    CLEAR_CHUNK(used, usedSize);

    CHUNK_USED_ST_SIZE (used, usedSize);
    CHUNK_USED_ST_SIZE2(used, usedSize);

    return CHUNK_USED_DATA(used);
}

// If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
// If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.
void* calloc (size_t n, size_t size_) {

    DBGPRINTF2("MALLOC - CALLOC");

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

    DBGPRINTF("=== REALLOC(BX%llX, %llu) ========================================================================", BOFFSET(data_), (uintll)dataSizeNew_);

    CHUNKS_VERIFY();

    if (data_ == NULL)
        return malloc(dataSizeNew_);

    // FOI NOS PASSADO O DATA; VAI PARA O CHUNK
    void* const chunk = CHUNK_USED_FROM_DATA(data_);

    ASSERT(!CHUNK_UNKN_IS_FREE(chunk));

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 sizeNew = CHUNK_SIZE_FROM_DATA_SIZE(dataSizeNew_);

    //
    u64 size = CHUNK_USED_LD_SIZE(chunk);

    DBGPRINTF("WILL REALLOC CHUNK BX%llX SIZE %llu -> %llu", BOFFSET(chunk), (uintll)size, (uintll)sizeNew);

    if (size >= sizeNew) {
        // ELE SE AUTOFORNECE
        if ((size - sizeNew) < 64)
            // MAS NÃO VALE A PENA DIMINUIR
            return data_;
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return data_;
    }

    void* right = CHUNK_RIGHT(chunk, size);

    // SÓ SE FOR FREE E SUFICIENTE
    if (CHUNK_UNKN_IS_FREE(right)) {

        DBGPRINTF("RIGHT CHUNK BX%llX IS FREE", BOFFSET(right));

        const u64 rightSize = CHUNK_FREE_LD_SIZE(right);
        // O QUANTO VAMOS TENTAR RETIRAR DA DIREITA
        const u64 sizeNeeded = sizeNew - size;

        if (rightSize >= sizeNeeded) {
            // DÁ SIM; VAMOS ALOCAR ARRANCANDO DA DIREITA

            // O TAMANHO NOVO DA DIREITA
            const u64 rightSizeNew = rightSize - sizeNeeded;
            DBGPRINTF("UA UAU UAU");

            // REMOVE ELE DE SUA LISTA
            CHUNK_FREE_REMOVE(right);

            if (rightSizeNew >= CHUNK_SIZE_MIN) {
                // PEGA ESTE PEDAÇO DELE, PELA ESQUERDA
                size += sizeNeeded; // size -> sizeNew
                // MOVE O COMEÇO PARA A DIREITA
                right += sizeNeeded;
                // REESCREVE
                CHUNK_FREE_FILL_AND_REGISTER(right, rightSizeNew);
                DBGPRINTF("RIGHT CONSUMED PARTIALLY; RIGHT CHUNK BX%llX SIZE %llu PTR BX%llX NEXT BX%llX", BOFFSET(right), (uintll)CHUNK_FREE_LD_SIZE(right), BOFFSET(CHUNK_FREE_LD_PTR(right)), BOFFSET(CHUNK_FREE_LD_NEXT(right)));
            } else { // CONSOME ELE POR INTEIRO
                size += rightSize; // size -> o que ja era + o free chunk
                DBGPRINTF("RIGHT CONSUMED ENTIRELY");
            }

            // ESCREVE ELE
            CHUNK_USED_ST_SIZE (chunk, size);
            CHUNK_USED_ST_SIZE2(chunk, size);

            DBGPRINTF("RETURNING CHUNK BX%llX SIZE %llu DATA BX%llX", BOFFSET(chunk), (uintll)CHUNK_USED_LD_SIZE(chunk), BOFFSET(CHUNK_USED_DATA(chunk)));

            return CHUNK_USED_DATA(chunk);
        }
    }

    DBGPRINTF("RIGHT CHUNK BX%llX NOT USED; ALLOCATING A NEW ONE", BOFFSET(right));

    // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
    void* const data = malloc(dataSizeNew_);

    if (data) {
        // CONSEGUIU
        // COPIA DO CHUNK ORIGINAL
        DBGPRINTF("COPYING DATA SIZE %llu BX%llX TO BX%llX", (uintll)CHUNK_USED_DATA_SIZE(size), BOFFSET(data_), BOFFSET(data));
        memcpy(data, data_, CHUNK_USED_DATA_SIZE(size));
        // LIBERA O CHUNK ORIGINAL
        free(CHUNK_USED_DATA(chunk));

        DBGPRINTF("RETURNING CHUNK BX%llX SIZE %llu DATA BX%llX", BOFFSET(CHUNK_USED_FROM_DATA(data)), (uintll)CHUNK_USED_LD_SIZE(CHUNK_USED_FROM_DATA(data)), BOFFSET(CHUNK_USED_DATA(CHUNK_USED_FROM_DATA(data))));

        ASSERT(CHUNK_USED_DATA(CHUNK_USED_FROM_DATA(data)) == data);
    }

    DBGPRINTF("RETURNING DATA BX%llX", BOFFSET(data));

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

//  começa depois disso
//   usar = sizeof(Buffer) + 8 + BUFF->mallocSize + 8;
// le todos os seus de /proc/self/maps
//      if ((usar + ORIGINALSIZE) > BUFFER_LMT)
//            abort();
//      pwrite(BUFFER_FD, ORIGINALADDR, ORIGINALSIZE, usar);
//   agora remapeia
//      mmap(ORIGINALADDR, ORIGINALSIZE, prot, flags, BUFFER_FD, usar);
//      usar += ORIGINALSIZE;
