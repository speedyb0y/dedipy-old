/*
    TODO: FIXME: TESTAR COM VÁRIAS THREADS/FORKS: WORKER[workerID:subProcessID]

 buff                                buff + buffSize
 |___________________________________|
 |    ROOTS  | L |    CHUNKS     | R |

  TODO: FIXME: reduzir o PTR e o NEXT para um u32, múltiplo de 16
  TODO: FIXME: interceptar signal(), etc. quem captura eles somos nós. e quando possível, executamos a função deles
  TODO: FIXME: PRECISA DO MSYNC?
  TODO: FIXME: INTERCEPTAR sched_*()
  TODO: FIXME: INTERCEPTAR exec*()
  TODO: FIXME: INTERCEPTAR system()
  TODO: FIXME: INTERCEPTAR fork()
  TODO: FIXME: INTERCEPTAR clone()
  TODO: FIXME: INTERCEPTAR POSIX ALIGNED MEMORY FUNCTIONS
    int posix_memalign(void **memptr, size_t alignment, size_t size)
    void *aligned_alloc(size_t alignment, size_t size)
    void *valloc(size_t size)
    void *memalign(size_t alignment, size_t size)
    void *pvalloc(size_t size)
    void sync (void)
    int syncfs (int fd)

    cp -v ${HOME}/dedipy/{util.h,dedipy.h,python-dedipy.h,python-dedipy.c} .

    TODO: FIXME: definir dedipy_malloc como (buff->malloc)
        no main do processo a gente
        todo o codigo vai somente no main
        todos os modulos, libs etc do processo atual a partir dai poderao usar o as definicoes do python-dedipy.h

        BUFFER_MALLOC malloc_f_t malloc;

    NOTE: não vamos ter um C_DATA_MAX... pois nao vamos alocar nada próximo de 2^64 mesmo :/

    // O ALGORITIMO PODE USAR AS FLAGS PARA COISAS MAIS INTELIGENTES :/
*/

#ifndef DEDIPY_DEBUG
#define DEDIPY_DEBUG 0
#endif

#ifndef DEDIPY_VERIFY
#define DEDIPY_VERIFY 0
#endif

#ifndef DEDIPY_TEST
#define DEDIPY_TEST 0
#endif

#define _GNU_SOURCE 1

#if!(_LARGEFILE64_SOURCE && _FILE_OFFSET_BITS == 64)
#error
#endif

#define DBG_PREPEND "WORKER [%u] "
#define DBG_PREPEND_ARGS  id

#ifndef DEDIPY_TEST_1_COUNT
#define DEDIPY_TEST_1_COUNT 500
#endif

#ifndef DEDIPY_TEST_2_COUNT
#define DEDIPY_TEST_2_COUNT 150
#endif

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>

#include "util.h"

#include "dedipy.h"

#include "python-dedipy-gen.h"

#undef malloc
#define malloc ((void)0)
#undef free
#define free ((void)0)
#undef realloc
#define realloc ((void)0)

#define BUFF_ROOTS   ((chunk_s**)    ((addr_t)buff ))
#define BUFF_L       ((chunk_size_t*)((addr_t)buff + ROOTS_N*sizeof(chunk_s*) ))
#define BUFF_CHUNKS  ((chunk_s*     )((addr_t)buff + ROOTS_N*sizeof(chunk_s*) + sizeof(chunk_size_t)))
#define BUFF_R       ((chunk_size_t*)((addr_t)buff + buffSize - sizeof(chunk_size_t) ))
#define BUFF_LMT                     ((addr_t)buff + buffSize )

#define BUFF_ROOTS_SIZE (ROOTS_N*sizeof(chunk_s*))
#define BUFF_CHUNKS_SIZE (buffSize - BUFF_ROOTS_SIZE - 2*sizeof(chunk_size_t)) // É TODO O BUFFER RETIRANDO O RESTANTE

// FOR DEBUGGING
#define BOFFSET(x) ((uintll)((addr_t)(const char*)(x) - (addr_t)BUFF_ADDR))

typedef u64 chunk_size_t;

typedef struct chunk_s chunk_s;

struct chunk_s {
    chunk_size_t size;
    chunk_s** ptr; // SÓ NO FREE
    chunk_s* next; // SÓ NO FREE
};

static uint id;
static uint n;
static uint groupID;
static uint groupN;
static uint cpu;
static u64 pid;
static u64 code;
static u64 started;
static addr_t buff; // MY BUFFER
static u64 buffSize; // MY SIZE
static u64 buffTotal; // TOTAL, INCLUDING ALL PROCESSES
static int buffFD;

#define in_buff(a, s) in_mem(a, s, (void*)buff, buffSize)
#define in_chunks(a, s) in_mem(a, s, (void*)BUFF_CHUNKS, BUFF_CHUNKS_SIZE)

// TODO: FIXME: dar assert para que DATA_ALIGNMENT fique alinhado a isso no used

//  C_SIZE_MIN
// FREE: SIZE + PTR + NEXT + ... + SIZE2
// USED: SIZE + DATA...          + SIZE2
// ----- NOTE: o máximo deve ser <= último
// ----- NOTE: estes limites tem que considerar o alinhamento
// ----- NOTE: cuidado com valores grandes, pois ao serem somados com os endereços, haverão overflows
//              REGRA GERAL: (buff + 4*ROOTS_SIZES_LMT) < (1 << 63)   <--- testar no python, pois se ao verificar tiver overflow, não adiantará nada =]

// TEM DE SER NÃO 0, PARA QUE SEJA LIDO COMO NÃO NULL SE ULTRAPASSAR O OS ROOTS
// MAS TAMBÉM NÃO PODE SER EQUIVALENTE A UM PONTEIRO PARA UM CHUNK
// TEM QUE SER USED, PARA QUE NUNCA TENTEMOS ACESSÁ-LO NA HORA DE JOIN LEFT/RIGHT
// O MENOR TAMANHO POSSÍVEL; SE TENTARMOS ACESSAR ELE, VAMOS DESCOBRIR COM O assert SIZE >= MIN
#define C_FLAGS 0b00000000000000000000000000000000000000000000000111ULL
#define C_FREE  0b00000000000000000000000000000000000000000000000001ULL
#define C_USED  0b00000000000000000000000000000000000000000000000010ULL
#define C_DUMMY 0b00000000000000000000000000000000000000000000000100ULL
#define C_SIZE  0b11111111111111111111111111111111111111111111111000ULL // ALSO C_SIZE_MAX

#define C_SIZE_MIN 32ULL
#define C_SIZE_MAX C_SIZE

#define CHUNK_ALIGNMENT 8ULL
#define DATA_ALIGNMENT 8ULL

// TAMANHO DE UM CHUNK COM TAL DATA SIZE, ALINHADO
// NOTE: QUEREMOS TANTO OS DADOS QUANTO O CHUNK ALINHADOS; ALINHA AO QUE FOR MAIOR
#define c_size_used_minimal(ds) (((u64)(sizeof(chunk_size_t) + (u64)(ds) + (DATA_ALIGNMENT - 1ULL) + sizeof(chunk_size_t))) & ~(DATA_ALIGNMENT - 1ULL))

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define c_size_from_requested_data_size(ds) (c_size_used_minimal(ds) > C_SIZE_MIN ? c_size_used_minimal(ds) : C_SIZE_MIN)

#define c_size2(c, s) ((chunk_size_t*)(ADDR(c) + (s) - sizeof(chunk_size_t)))
#define c_data(c) ((void*)(ADDR(c) + sizeof(chunk_size_t)))
#define c_from_data(d) ((chunk_s*)(ADDR(d) - sizeof(chunk_size_t)))
#define c_data_size(s) ((u64)((s) - 2*sizeof(chunk_size_t))) // DATA SIZE FROM THE USED CHUNK SIZE
#define c_left_size(c) (*(chunk_size_t*)(ADDR(c) - sizeof(chunk_size_t)))
#define c_right(c, s) ((chunk_s*)(ADDR(c) + (s)))

static inline void assert_c_size (const chunk_size_t s) {
    assert(s & C_SIZE); // NÃO É 0
    assert((s & C_FLAGS) == 0); // NÃO TEM NENHUMA FLAG
    assert(((s & C_SIZE) % CHUNK_ALIGNMENT) == 0); // ESTÁ ALINHADO
    assert((s & C_SIZE) >= C_SIZE_MIN);
    assert((s & C_SIZE) <= C_SIZE_MAX);
    assert((s & C_SIZE) == s); // O TAMANHO ESTÁ DENTRO DA MASK DE TAMANHO
}

// TEM QUE TER CERTEZA DE QUE A ESTRUTURA ESTÁ TODA NA MEMÓRIA
// DEPOIS AGORA SIM LE O SIZE PARA VERIFICAR TUDO
// COISAS REPETITIVAS, MAS É PARA TESTAR AS FUNÇÕES TAMBÉM
static void assert_c_used (const chunk_s* const c) {
    /* CHUNK */
    assert((ADDR(c) % CHUNK_ALIGNMENT) == 0);
    assert(in_chunks(c, C_SIZE_MIN));
    assert(in_chunks(c, c->size & C_SIZE));
    /* SIZES */
    assert(c->size);
    assert((c->size & C_FLAGS) == C_USED); // FLAG USED ESTÁ SETADA, E SOMENTE ELA ESTÁ
    assert((c->size & ~C_FLAGS) == (c->size & C_SIZE)); // O TAMANHO NÃO EXTRAPOLA A MASK DO SIZE
    assert_c_size(c->size & C_SIZE);
    assert(c->size == *c_size2(c, c->size & C_SIZE));
    /* DATA */
    assert(c_from_data(c_data(c)) == c);
    assert((ADDR(c_data(c)) % DATA_ALIGNMENT) == 0);
}

static void assert_c_free (const chunk_s* const c) {
    /* CHUNK */
    assert(is_aligned((addr_t)c, CHUNK_ALIGNMENT));
    assert(in_chunks(c, C_SIZE_MIN));
    assert(in_chunks(c, c->size & C_SIZE));
    /* SIZES */
    assert(c->size);
    assert((c->size & C_FLAGS) == C_FREE);
    assert((c->size & ~C_FLAGS) == (c->size & C_SIZE));
    assert_c_size(c->size & C_SIZE);
    assert(c->size == *c_size2(c, c->size & C_SIZE));
    /* PTR */
    assert(c->ptr);
    assert(in_buff(c->ptr, sizeof(chunk_s*)));
    assert(*c->ptr == c);
    /* NEXT */
    if (c->next) {
        assert(is_aligned((addr_t)c->next, CHUNK_ALIGNMENT));
        assert(in_chunks(c->next, C_SIZE_MIN));
        assert(in_chunks(c->next, c->next->size));
        assert(c->next->ptr == &c->next);
    }
}

#define assert_c_used(c) ({ typeof(c) __c = (c); dbg("CHECKING USED CHUNK BX%llX", BOFFSET(__c)); assert_c_used(__c); })
#define assert_c_free(c) ({ typeof(c) __c = (c); dbg("CHECKING FREE CHUNK BX%llX", BOFFSET(__c)); assert_c_free(__c); })
#define assert_c_size(s) ({ typeof(s) __s = (s); dbg("CHECKING CHUNK SIZE %llu", (uintll)__s); assert_c_size(__s); })

// TODO: FIXME: outra verificação, completa
// C_CHUNK(c) o chunk é válido, levando em consideração qual tipo é

static inline uint root_put_idx (u64 size) {

    if (size <= (ROOTS_MAX_0 - C_SIZE_MIN)) { size -= C_SIZE_MIN              ; size = (size >> ROOTS_DIV_0) + ROOTS_GROUPS_OFFSET_0; } else
    if (size <= (ROOTS_MAX_1 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_0; size = (size >> ROOTS_DIV_1) + ROOTS_GROUPS_OFFSET_1; } else
    if (size <= (ROOTS_MAX_2 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_1; size = (size >> ROOTS_DIV_2) + ROOTS_GROUPS_OFFSET_2; } else
    if (size <= (ROOTS_MAX_3 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_2; size = (size >> ROOTS_DIV_3) + ROOTS_GROUPS_OFFSET_3; } else
    if (size <= (ROOTS_MAX_4 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_3; size = (size >> ROOTS_DIV_4) + ROOTS_GROUPS_OFFSET_4; } else
    if (size <= (ROOTS_MAX_5 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_4; size = (size >> ROOTS_DIV_5) + ROOTS_GROUPS_OFFSET_5; } else
    if (size <= (ROOTS_MAX_6 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_5; size = (size >> ROOTS_DIV_6) + ROOTS_GROUPS_OFFSET_6; } else
    if (size <= (ROOTS_MAX_7 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_6; size = (size >> ROOTS_DIV_7) + ROOTS_GROUPS_OFFSET_7; } else
        size = ROOTS_N - 1;

    return (uint)size;
}

static inline chunk_s** root_put_ptr (u64 size) {

    uint idx = root_put_idx(size);

    assert(idx < ROOTS_N);

    return BUFF_ROOTS + idx;
}

static inline uint root_get_idx (u64 size) {

    if (size <= (ROOTS_MAX_0 - C_SIZE_MIN)) { size -= C_SIZE_MIN              ; size = (size >> ROOTS_DIV_0) + (!!(size & ROOTS_GROUPS_REMAINING_0)) + ROOTS_GROUPS_OFFSET_0; } else
    if (size <= (ROOTS_MAX_1 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_0; size = (size >> ROOTS_DIV_1) + (!!(size & ROOTS_GROUPS_REMAINING_1)) + ROOTS_GROUPS_OFFSET_1; } else
    if (size <= (ROOTS_MAX_2 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_1; size = (size >> ROOTS_DIV_2) + (!!(size & ROOTS_GROUPS_REMAINING_2)) + ROOTS_GROUPS_OFFSET_2; } else
    if (size <= (ROOTS_MAX_3 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_2; size = (size >> ROOTS_DIV_3) + (!!(size & ROOTS_GROUPS_REMAINING_3)) + ROOTS_GROUPS_OFFSET_3; } else
    if (size <= (ROOTS_MAX_4 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_3; size = (size >> ROOTS_DIV_4) + (!!(size & ROOTS_GROUPS_REMAINING_4)) + ROOTS_GROUPS_OFFSET_4; } else
    if (size <= (ROOTS_MAX_5 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_4; size = (size >> ROOTS_DIV_5) + (!!(size & ROOTS_GROUPS_REMAINING_5)) + ROOTS_GROUPS_OFFSET_5; } else
    if (size <= (ROOTS_MAX_6 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_5; size = (size >> ROOTS_DIV_6) + (!!(size & ROOTS_GROUPS_REMAINING_6)) + ROOTS_GROUPS_OFFSET_6; } else
    if (size <= (ROOTS_MAX_7 - C_SIZE_MIN)) { size -= C_SIZE_MIN + ROOTS_MAX_6; size = (size >> ROOTS_DIV_7) + (!!(size & ROOTS_GROUPS_REMAINING_7)) + ROOTS_GROUPS_OFFSET_7; } else
        size = ROOTS_N - 1;

    return (uint)size;
}

static inline chunk_s** root_get_ptr (u64 size) {

    uint idx = root_get_idx(size);

    assert(idx < ROOTS_N);

    return BUFF_ROOTS + idx;
}

static inline void c_free_fill_and_register (chunk_s* const c, const u64 s) {

    *c_size2(c, s) = c->size = s | C_FREE;

    chunk_s** const ptr = root_put_ptr(s);

    assert(ptr >= BUFF_ROOTS && ptr < (BUFF_ROOTS + ROOTS_N) && ((addr_t)ptr % sizeof(chunk_s*)) == 0);

    c->ptr = ptr;
    if ((c->next = *c->ptr))
        c->next->ptr = &c->next;
    *c->ptr = c;

    assert_c_free(c);
}

// NOTE: VAI DEIXAR O PTR E O NEXT INVÁLIDOS
static inline void c_unregister (const chunk_s* const c) {

    assert_c_free(c);

    if ((*c->ptr = c->next)) {
        (*c->ptr)->ptr = c->ptr;
        assert_c_free(c->next);
    }
}

// MUST HAVE SAME ALIGNMENTS AS MALLOC! :/ @_@
static void dedipy_verify (void) {

    dbg("VERIFY-------------------------");

    // ROOTS
    assert(in_buff(BUFF_ROOTS, BUFF_ROOTS_SIZE));

    // CHUNKS
    assert(in_buff   (BUFF_CHUNKS, BUFF_CHUNKS->size & C_SIZE));
    assert(in_chunks (BUFF_CHUNKS, BUFF_CHUNKS->size & C_SIZE));

    // LEFT/RIGHT
    assert(*BUFF_L == C_DUMMY);
    assert(*BUFF_R == C_DUMMY);

    assert(in_buff(BUFF_L, sizeof(chunk_size_t)));
    assert(in_buff(BUFF_R, sizeof(chunk_size_t)));

    assert(ADDR(buff + buffSize) == ADDR(BUFF_LMT));

    assert(ADDR(BUFF_ROOTS + ROOTS_N) == ADDR(BUFF_L));

    assert(*BUFF_L == C_DUMMY);
    assert(*BUFF_R == C_DUMMY);
    assert(*BUFF_L == *BUFF_R);

    // O LEFT TEM QUE SER INTERPRETADO COMO NÃO NULL
    assert(BUFF_ROOTS[ROOTS_N]);

#if DEDIPY_VERIFY || 1
    u64 totalFree = 0;
    u64 totalUsed = 0;

    { const chunk_s* c = BUFF_CHUNKS;

        while (c != (chunk_s*)BUFF_R) {
            dbg("VERIFY CHUNK BX%llX SIZE %llu", BOFFSET(c), (uintll)(c->size & C_SIZE));
            if (c->size & C_FREE)
                { assert_c_free(c); totalFree += c->size & C_SIZE; }
            else
                { assert_c_used(c); totalUsed += c->size & C_SIZE; }
            c = c_right(c, c->size & C_SIZE);
        }
    }

    const u64 total = totalFree + totalUsed;

    dbg3("-- TOTAL %llu ------", (uintll)total);

    assert ( total == BUFF_CHUNKS_SIZE );

    // VERIFICA OS FREES
    uint idx = 0; chunk_s** ptrRoot = BUFF_ROOTS;

    do {
        chunk_s* const* ptr = ptrRoot;
        const chunk_s* chunk = *ptrRoot;

        dbg3("FREE ROOT #%d CHUNK BX%llX", idx, BOFFSET(chunk));

        while (chunk) {
            assert_c_free(chunk);
            //assert((chunk->size & C_SIZE) >= fst); // IR CALCULANDO OS RANES QUE PODEM TER EM CADA SLOT
            //assert((chunk->size & C_SIZE) <  lmt);
            assert(chunk->ptr == ptr);
            //assert ( f_ptr_root_get(f_get_size(chunk)) == ptrRoot );
            //assert ( root_put_ptr(f_get_size(chunk)) == ptrRoot ); QUAL DOS DOIS? :S e um <=/>= em umdeles
            //
            totalFree -= chunk->size & C_SIZE;
            // PRÓXIMO
            chunk = *(ptr = &chunk->next);
        }

        ptrRoot++;

    } while (++idx != ROOTS_N);

    assert(ptrRoot == (BUFF_ROOTS + ROOTS_N));

    // CONFIRMA QUE VIU TODOS OS FREES VISTOS AO ANDAR TODOS OS CHUNKS
    assert(totalFree == 0);
#endif
}

void* dedipy_malloc (const size_t size_) {
#define dedipy_malloc(b) ({ size_t __b = (b); dbg("dedipy_malloc(%llu)", (uintll)__b); void* __ret = dedipy_malloc(__b); dbg("dedipy_malloc() -> BX%llX", BOFFSET(__ret)); __ret; })

    if (size_ == 0)
        return NULL;

    u64 size = c_size_from_requested_data_size((u64)size_);

    if (size > C_SIZE_MAX)
        return NULL;

    assert_c_size(size);

    chunk_s** ptr = root_get_ptr(size);

    assert(ptr >= BUFF_ROOTS && ptr < (BUFF_ROOTS + ROOTS_N) && ((addr_t)ptr % sizeof(chunk_s*)) == 0);

    chunk_s* used;

    while ((used = *ptr) == NULL)
        ptr++;

    if (used == (chunk_s*)C_DUMMY)
        return NULL;

    assert_c_free(used);

    c_unregister(used);

    u64 usedSize = used->size & C_SIZE;

    assert_c_size(usedSize);

    const u64 freeSizeNew = usedSize - size;

    if (freeSizeNew >= C_SIZE_MIN) {
        c_free_fill_and_register(used, freeSizeNew);
        used = c_right(used, freeSizeNew);
        usedSize = size;
    }

    *c_size2(used, usedSize) = used->size = usedSize | C_USED;

    assert_c_used(used);

    return c_data(used);
}

// If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
// If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.
void* dedipy_calloc (size_t n, size_t size_) {

    const u64 size = (u64)n * (u64)size_;

    void* const data = dedipy_malloc(size);

    // INITIALIZE IT
    if (data)
        memset(data, 0, size);

    return data;
}

void dedipy_free (void* const data) {
#define dedipy_free(a) ({ void* __a =(a); dbg("dedipy_free(BX%llX)", BOFFSET(__a)); dedipy_free(__a); dbg("dedipy_free() -> void"); })

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        chunk_s* c = c_from_data(data);
dbg("FREE CHUNK BX%llX DATA BX%llX SIZE %llu", BOFFSET(c), BOFFSET(data), (uintll)(c->size & C_SIZE));
        assert_c_used(c);

        u64 size = c->size & C_SIZE;

        assert_c_size(size);

        chunk_size_t ss;

        if ((ss = c_left_size(c)) & C_FREE) {
            ss &= C_SIZE;
            size += ss;
            c = (chunk_s*)((addr_t)c - ss);
            assert_c_free(c);
            c_unregister(c);
        }

        chunk_s* const right = c_right(c, size);

        if ((ss = right->size) & C_FREE) {
            ss &= C_SIZE;
            size += ss;
            assert_c_free(right);
            c_unregister(right);
        }

        c_free_fill_and_register(c, size);

        assert_c_free(c);
    }
}

void* dedipy_realloc (void* const _data, const size_t dataSizeNew_) {
#define dedipy_realloc(a, b) ({ void* __a =(a); size_t __b = (b); dbg("dedipy_realloc(BX%llX, %llu)", BOFFSET(__a), (uintll)__b); void* __ret = dedipy_realloc(__a, __b); dbg("dedipy_realloc() -> BX%llX", BOFFSET(__ret)); __ret; })

    if (_data == NULL) // GLIBC: IF PTR IS NULL, THEN THE CALL IS EQUIVALENT TO MALLOC(SIZE), FOR ALL VALUES OF SIZE
        return dedipy_malloc(dataSizeNew_);

    if (dataSizeNew_ == 0) { // GLIBC: IF SIZE IS EQUAL TO ZERO, AND PTR IS NOT NULL, THEN THE CALL IS EQUIVALENT TO FREE(PTR)
        dedipy_free(_data);
        return NULL;
    }

    u64 sizeNew = c_size_from_requested_data_size(dataSizeNew_);

    chunk_s* const chunk = c_from_data(_data);

    assert_c_used(chunk);

    u64 size = chunk->size & C_SIZE;

    if (size >= sizeNew) { // ELE SE AUTOFORNECE
        if ((size - sizeNew) < C_SIZE_MIN)
            return _data; // MAS NÃO VALE A PENA DIMINUIR
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return _data;
    }

    chunk_s* right = c_right(chunk, size);

    u64 rightSize = right->size;

    if (rightSize & C_FREE) {
        rightSize &= C_SIZE;

        assert_c_free(right);

        u64 sizeNeeded = sizeNew - size;

        if (rightSize >= sizeNeeded) {

            c_unregister(right);

            u64 rightSizeNew = rightSize - sizeNeeded;

            if (rightSizeNew >= C_SIZE_MIN) { // PEGA ESTE PEDAÇO DELE, PELA ESQUERDA
                size += sizeNeeded;
                right = (chunk_s*)(ADDR(right) + sizeNeeded);
                c_free_fill_and_register(right, rightSizeNew);
            } else // CONSOME ELE POR INTEIRO
                size += rightSize;

            *c_size2(chunk, size) = chunk->size = size | C_USED;

            assert_c_used(chunk);

            return c_data(chunk);
        }
    }

    void* const data = dedipy_malloc(dataSizeNew_);

    if (data) {
        memcpy(data, _data, c_data_size(size));

        dedipy_free(_data);

        assert_c_used(c_from_data(data));
    }

    return data;
}

void* dedipy_reallocarray (void* ptr, size_t nmemb, size_t size) {

    (void)ptr;
    (void)nmemb;
    (void)size;

    fatal("MALLOC - REALLOCARRAY");
}

//  começa depois disso
//   usar = sizeof(Buffer) + 8 + BUFF->mallocSize + 8;
// le todos os seus de /proc/self/maps
//      if ((usar + ORIGINALSIZE) > BUFF_LMT)
//            abort();
//      pwrite(BUFFER_FD, ORIGINALADDR, ORIGINALSIZE, usar);
//   agora remapeia
//      mmap(ORIGINALADDR, ORIGINALSIZE, prot, flags, BUFFER_FD, usar);
//      usar += ORIGINALSIZE;

// TODO: FIXME: WE WILL NEED A SIGNAL HANDLER

#if DEDIPY_TEST
static inline u64 dedipy_test_random (const u64 x) {

    static volatile u64 _rand = 0;

    _rand += x;
    _rand += rdtsc() & 0xFFFULL;
#if 0
    _rand += __builtin_ia32_rdrand64_step((void*)&_rand);
#endif
    return _rand;
}

static inline u64 dedipy_test_size (u64 x) {

    x = dedipy_test_random(x);

    return (x >> 2) & (
        (x & 0b1ULL) ? (
            (x & 0b10ULL) ? 0xFFFFFULL :   0xFFULL
        ) : (
            (x & 0b10ULL) ?   0xFFFULL : 0xFFFFULL
        ));
}
#endif

void dedipy_main (void) {

    // SUPPORT CALLING FROM MULTIPLE PLACES =]
    static int initialized = 0;

    if (initialized)
        return;

    assert ( is_from_to(0, 0, 2) );
    assert ( is_from_to(0, 1, 2) );
    assert ( is_from_to(0, 2, 2) );
    assert ( is_from_to(0, 0, 0) );
    assert ( is_from_to(1, 1, 1) );

    initialized = 1;

    uintll buffFD_    = 0;
    uintll buffFlags_ = 0;
    uintll buffAddr_  = 0;
    uintll buffTotal_ = 0;
    uintll buffStart_ = 0;
    uintll buffSize_  = 0;
    uintll cpu_       = 0;
    uintll pid_       = 0;
    uintll code_      = 0;
    uintll started_   = 0;
    uintll id_        = 0;
    uintll n_         = 0;
    uintll groupID_   = 0;
    uintll groupN_    = 0;

    const char* var = getenv("DEDIPY");

    //fatal("FAILED TO LOAD ENVIROMENT PARAMS");
    if (var) {
        if (sscanf(var, "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX"  "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX",
            &cpu_, &pid_, &buffFD_, &buffFlags_, &buffAddr_, &buffTotal_, &buffStart_, &buffSize_, &code_, &started_, &id_, &n_, &groupID_, &groupN_) != 14)
            fatal("FAILED TO LOAD ENVIROMENT PARAMS");
    } else { // EMERGENCY MODE =]
        dbg("RUNNING IN FALL BACK MODE");
        cpu_ = (uintll)sched_getcpu();
        buffFD_ = 0;
        buffFlags_ = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_FIXED_NOREPLACE;
        buffAddr_ = (uintll)BUFF_ADDR;
        buffTotal_ = 256*1024*1024;
        buffStart_ = 0;
        buffSize_ = buffTotal_;
        pid_ = (uintll)getpid();
        n_ = 1;
        groupN_ = 1;
    }

    // THOSE ARE FOR EMERGENCY/DEBUGGING
    // THEY ARE IN DECIMAL, FOR MANUAL SETTING IN THE C SOURCE/SHELL
    if ((var = getenv("DEDIPY_BUFF_ADDR"  ))) buffAddr_    = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_FD"    ))) buffFD_      = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_FLAGS" ))) buffFlags_   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_TOTAL" ))) buffTotal_   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_START" ))) buffStart_   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_SIZE"  ))) buffSize_    = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_CPU"        ))) cpu_         = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_ID"         ))) id_          = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_N"          ))) n_           = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_GROUP_ID"   ))) groupID_     = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_GROUP_N"    ))) groupN_      = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_PID"        ))) pid_         = strtoull(var, NULL, 10);

    if ((void*)buffAddr_ != BUFF_ADDR)
        fatal("BUFFER ADDRESS MISMATCH");

    if ((pid_t)pid_ != getpid())
        fatal("PID MISMATCH");

    if (buffStart_ >= buffTotal_)
        fatal("BAD BUFFER START");

    if (buffSize_ == 0)
        fatal("BAD BUFFER SIZE");

    if ((int)cpu_ != sched_getcpu())
        fatal("CPU MISMATCH");

    if (n_ == 0 || n_ > 0xFF)
        fatal("BAD N");

    if (id_ >= n_)
        fatal("BAD ID/N");

    if (groupN_ == 0 || groupN_ > 0xFF)
        fatal("BAD GROUP N");

    if (groupID_ >= groupN_)
        fatal("BAD GROUP ID/N");

    //  | MAP_LOCKED | MAP_POPULATE | MAP_HUGETLB | MAP_HUGE_1GB
    if (BUFF_ADDR != mmap(BUFF_ADDR, buffTotal_, PROT_READ | PROT_WRITE, (int)buffFlags_, (int)buffFD_, 0))
        fatal("FAILED TO MAP BUFFER");

    if (buffFD_ && close((int)buffFD_))
        fatal("FAILED TO CLOSE BUFFER FD");

    dbg("INICIALIZANDO AINDA...");

    // INFO
    buffFD    = (int)buffFD_;
    buff      = (addr_t)BUFF_ADDR + buffStart_;
    buffSize  = (u64)buffSize_;
    buffTotal = (u64)buffTotal_;
    id        = (uint)id_;
    n         = (uint)n_;
    groupID   = (uint)groupID_;
    groupN    = (uint)groupN_;
    cpu       = (uint)cpu_;
    code      = (u64)code_;
    pid       = (u64)pid_;
    started   = (u64)started_;

    // var + 14*16, strlen(var + 14*16)
    char name[256];

    if (snprintf(name, sizeof(name), PROGNAME "#%u", id) < 2 || prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0))
        fatal("FAILED TO SET PROCESS NAME");

    //
    memset((void*)buff, 0, buffSize);

    // LEFT AND RIGHT
    *BUFF_L = C_DUMMY;
    *BUFF_R = C_DUMMY;

    // A CHANGE ON THOSE MAY REQUIRE A REVIEW
    assert(8 == sizeof(chunk_size_t));
    assert(8 == sizeof(u64));
    assert(8 == sizeof(off_t));
    assert(8 == sizeof(size_t));
    assert(8 == sizeof(void*));

    //assert (C_SIZE_MIN == ROOTS_SIZES_0);

    //assert (ROOTS_SIZES_0 < ROOTS_SIZES_N);

    assert((C_SIZE & (C_FREE | C_SIZE_MIN)) == C_SIZE_MIN);
    assert((C_SIZE & (C_FREE | C_SIZE_MAX)) == C_SIZE_MAX);

    assert((C_SIZE & (C_FREE | c_size_from_requested_data_size(0))) == C_SIZE_MIN);
    assert((C_SIZE & (C_FREE | c_size_from_requested_data_size(1))) == C_SIZE_MIN);
    dbg("%llu", (uintll)( (C_SIZE & (C_FREE | c_size_from_requested_data_size(sizeof(chunk_s))))));
    assert((C_SIZE & (C_FREE | c_size_from_requested_data_size(2*sizeof(chunk_size_t)))) == C_SIZE_MIN);
    assert((C_SIZE & (C_FREE | c_size_from_requested_data_size(C_SIZE_MIN - 2*sizeof(chunk_size_t)))) == C_SIZE_MIN);

    assert((C_SIZE & (C_FREE | 65536ULL)) == 65536ULL);
    assert((C_SIZE & (C_USED | 65536ULL)) == 65536ULL);

    assert(root_get_ptr(C_SIZE_MIN) == BUFF_ROOTS);
    assert(root_put_ptr(C_SIZE_MIN) == BUFF_ROOTS);

    assert(root_get_ptr(16ULL*1024*1024*1024*1024) == (BUFF_ROOTS + (ROOTS_N - 1)));
    assert(root_put_ptr(16ULL*1024*1024*1024*1024) <= (BUFF_ROOTS + (ROOTS_N - 1))); // COMO VAI ARREDONDAR PARA BAIXO, PEDIR O MÁXIMO PODE CAIR LOGO ANTES DO ÚLTIMO SLOT

    dbg("CRIANDO CHUNK 0 %llu", (uintll)BUFF_CHUNKS_SIZE);

    // THE INITIAL CHUNK
    // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O C_SIZE_MAX E ROOTS_SIZES_N SÃO MAIORES DO QUE ELE
    c_free_fill_and_register(BUFF_CHUNKS, BUFF_CHUNKS_SIZE);

    assert_c_free(BUFF_CHUNKS);

    dbg("CHUNK 0 CRIADO");

    // TODO: FIXME: tentar dar dedipy_malloc() e dedipy_realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // ROOTS_SIZES_N

    dbg("CPU %u",                  (uint)cpu);
    dbg("PID %llu",                (uintll)pid);
    dbg("ID %u",                   (uint)id);
    dbg("CODE %llu",               (uintll)code);

    dbg("C_SIZE_MIN %llu", (uintll)C_SIZE_MIN);

    dbg("BUFF ADDR 0x%016llX",      (uintll)BUFF_ADDR);
    dbg("BUFF TOTAL SIZE %llu",     (uintll)buffTotal);
    dbg("BUFF START %llu",          (uintll)buffStart_);
    dbg("BUFF 0x%016llX",           BOFFSET(buff));
    dbg("BUFF_ROOTS BX%llX",        BOFFSET(BUFF_ROOTS));
    dbg("BUFF_L BX%llX",            BOFFSET(BUFF_L));
    dbg(" BUFF_CHUNKS BX%llX",      BOFFSET(BUFF_CHUNKS));
    dbg(" BUFF_CHUNKS SIZE %llu",   (uintll)(BUFF_CHUNKS->size & C_SIZE));
    dbg(" BUFF_CHUNKS PTR BX%llX",  BOFFSET( BUFF_CHUNKS->ptr));
    dbg("*BUFF_CHUNKS PTR BX%llX",  BOFFSET(*BUFF_CHUNKS->ptr));
    dbg(" BUFF_CHUNKS NEXT BX%llX", BOFFSET( BUFF_CHUNKS->next));
    dbg("BUFF_R BX%llX",            BOFFSET(BUFF_R));
    dbg("BUFF_LMT BX%llX",          BOFFSET(BUFF_LMT));
    dbg("BUFF SIZE %llu",          (uintll)buffSize);
    dbg("NULL BX%llX",              BOFFSET(NULL));

    dbg("*BUFF_L %llu",    (uintll)*BUFF_L);
    dbg("*BUFF_R %llu",    (uintll)*BUFF_R);

    void* a = dedipy_malloc(2*1024*1024);

    if (dedipy_malloc(1))
        (void)0;

    dedipy_free(a);

    dedipy_free(dedipy_malloc(      65536));
    dedipy_free(dedipy_malloc(2*1024*1024));
    dedipy_free(dedipy_malloc(3*1024*1024));
    dedipy_free(dedipy_malloc(4*1024*1024));

    dedipy_verify();

    dbg("OKAY!");

    assert_c_size(c_size_from_requested_data_size(1));

    //assert(ROOTS_SIZES_0 <= c_size_from_requested_data_size((u64)1));

    assert(dedipy_malloc(0) == NULL);
    assert(dedipy_realloc(NULL, 0) == NULL);

    assert(dedipy_realloc(dedipy_malloc(1), 0) == NULL);
    assert(dedipy_realloc(dedipy_malloc(0), 0) == NULL);
    assert(dedipy_realloc(dedipy_malloc(65536), 0) == NULL);

    // NÃO TEM COMO DAR ASSERT LOL
    dedipy_free(NULL);

    dedipy_free(c_data(c_from_data(dedipy_realloc(NULL, 65536))));

    //
    assert(dedipy_realloc(dedipy_malloc(65536), 0) == NULL);

    // PRINT HOW MUCH MEMORY WE CAN ALLOCATE
    { u64 blockSize = 64*4096; // >= sizeof(void**)

        do { u64 count = 0; void** last = NULL; void** this;
            //
            while ((this = dedipy_malloc(blockSize))) {
                memset(this, 0, blockSize);
                *this = last;
                last = this;
                count += 1;
            }
            // NOW FREE THEM ALL
            while (last) {
                this = *last;
                dedipy_free(last);
                last = this;
            }
            //
            if (count)
                dbg("ALLOCATED %llu BLOCKS of %llu BYTES = %llu", (uintll)count, (uintll)blockSize, (uintll)(count * blockSize));
        } while ((blockSize <<= 1));
    }

    dbg("TEST 0");

    { uint c = DEDIPY_TEST_1_COUNT;
        while (c--) {

            dedipy_free(NULL);

            assert(dedipy_malloc(0) == NULL);
            assert(dedipy_realloc(NULL, 0) == NULL);

            dedipy_free(dedipy_malloc(dedipy_test_size(c + 1)));
            dedipy_free(dedipy_malloc(dedipy_test_size(c + 2)));
            dedipy_free(dedipy_malloc(dedipy_test_size(c + 3)));

            dedipy_free(dedipy_realloc(dedipy_malloc(dedipy_test_size(c + 4)), dedipy_test_size(c + 10)));
            dedipy_free(dedipy_realloc(dedipy_malloc(dedipy_test_size(c + 5)), dedipy_test_size(c + 11)));

            dedipy_free(dedipy_malloc(dedipy_test_size(c + 6)));
            dedipy_free(dedipy_malloc(dedipy_test_size(c + 7)));

            dedipy_free(dedipy_realloc(dedipy_malloc(dedipy_test_size(c + 8)), dedipy_test_size(c + 12)));
            dedipy_free(dedipy_realloc(dedipy_malloc(dedipy_test_size(c + 9)), dedipy_test_size(c + 13)));
        }
    }

    // TODO: FIXME: LEMBRAR O TAMANHO PEDIDO, E DAR UM MEMSET()
    { uint c = DEDIPY_TEST_2_COUNT;
        while (c--) {

            dbg("COUNTER %u", c);

            u64 size;

            void** last = NULL;
            void** new;

            // NOTE: cuidato com o dedipy_realloc(), só podemos realocar o ponteiro atual, e não os anteriores
            while ((new = dedipy_malloc((size = sizeof(void**) + dedipy_test_size(c))))) {
                assert(size >= sizeof(void**));
                //memset(new, size & 0xFF, size);
#if 1
                if (dedipy_test_random(c) % 10 <= 3) {
                    if ((size = sizeof(void**) + dedipy_test_size(c))) {
                        //assert(dedipy_realloc(new, size) == NULL);
                        //continue;
                        void** new2 = dedipy_realloc(new, size);
                        if (new2)
                            new = new2;
                    //memset(new, size & 0xFF, size);
                    }
                }
#endif
                assert(in_chunks(new, size));
                // TODO: FIXME: uma porcentagem, dar dedipy_free() aqui ao invés de incluir na lista
                *new = last;
                last = new;
            }

            while (last) {
                //if (dedipy_test_random(c) % 10 == 0)
                    //last = dedipy_realloc(last, sizeof(void**) + dedipy_test_size(c)) ?: last;
                void** old = *last;
                dedipy_free(last);
                last = old;
            }
        }
    }

    dedipy_verify();

    // TODO: FIXME: OUTRO TESTE: aloca todos com 4GB, depois com 1GB, até 8 bytes,
    // u64 size = 1ULL << 63;
    // do {
    //      while((this = dedipy_malloc(size)))
    //          ...
    //      while (last) {
    //          dedipy_free()
    //      }
    // } while ((size >>= 1));

#if 0
    dbg("RECEIVING SELF");

    char received[65536];

    const uint receivedSize = SELF_GET(received, sizeof(received));

    dbg("RECEIVED %u BYTES:", receivedSize);

    write(STDOUT_FILENO, received, receivedSize);
#endif

    dbg("TEST DONE");
}

// ver onde dá par diminuir acessos a ponteiros
// Só precisamos do buff, pois ele é o root. as demais coisas sós ao acessadas para inicializar e verificar.
// O restante é acessado diretamente, pelos ponteiros que o usuário possui.

// TODO: FIXME: GLIBC: RETURNS A POINTER TO THE NEWLY ALLOCATED MEMORY, WHICH IS SUITABLY ALIGNED FOR ANY BUILT-IN TYPE
