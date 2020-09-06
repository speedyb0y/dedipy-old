/*

*/

#ifndef DEDIPY_DEBUG
#define DEDIPY_DEBUG 0
#endif

#ifndef DEDIPY_TEST
#define DEDIPY_TEST 0
#endif

#ifndef DEDIPY_TEST_1_COUNT
#define DEDIPY_TEST_1_COUNT 500
#endif

#ifndef DEDIPY_TEST_2_COUNT
#define DEDIPY_TEST_2_COUNT 150
#endif

#define _GNU_SOURCE 1

#if!(_LARGEFILE64_SOURCE && _FILE_OFFSET_BITS == 64)
#error
#endif

#define DBG_PREPEND "WORKER [%u] "
#define DBG_PREPEND_ARGS  BUFF_INFO->id

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include "util.h"

#include "python-dedipy.h"

#include "dedipy.h"

#include "python-dedipy-gen.h"

#define BOFFSET(x) ((x) ?  ((uintll)((void*)(x) - (void*)BUFF)) : 0ULL)

typedef struct BufferInfo BufferInfo;

struct BufferInfo {
    u16 cpu;
    u16 id;
    u16 n; // quantos processos tem
    u16 groupID;
    u16 groupN;
    u16 reserved;
    u32 reserved2;
    u64 code;
    u64 pid;
    u64 started;
    u64 start;
    u64 size;
    u64 total; // TOTAL MEMORY MAPPED
};

#define BUFF_INFO          ((BufferInfo*)(BUFF))
#define BUFF_ROOTS           ((chunk_s**)(BUFF + sizeof(BufferInfo)))
#define BUFF_ROOTS_LST       ((chunk_s**)(BUFF + sizeof(BufferInfo) + sizeof(chunk_s*)*(ROOTS_N - 1)))
#define BUFF_ROOTS_LMT       ((chunk_s**)(BUFF + sizeof(BufferInfo) + sizeof(chunk_s*)*(ROOTS_N    )))
#define BUFF_L           ((chunk_size_t*)(BUFF + sizeof(BufferInfo) + sizeof(chunk_s*)*(ROOTS_N    )))
#define BUFF_CHUNKS                      (BUFF + sizeof(BufferInfo) + sizeof(chunk_s*)*(ROOTS_N    ) + sizeof(u64))
#define BUFF_R           ((chunk_size_t*)(BUFF + BUFF_INFO->size - sizeof(u64)))
#define BUFF_LMT                         (BUFF + BUFF_INFO->size)

#define BUFF_CHUNKS_SIZE ((BUFF_INFO->size) - sizeof(BufferInfo) - ROOTS_N*sizeof(chunk_s*) - sizeof(u64) - sizeof(u64))

#define CHUNK_ALIGNMENT 8ULL
#define DATA_ALIGNMENT 8ULL

#define C_SIZE_MIN 32ULL
#define C_SIZE_MAX C_SIZE

#define _CHUNK_USED_SIZE(dataSize) (sizeof(chunk_size_t) + ((((u64)(dataSize) + 7ULL) & ~0b111ULL)) + sizeof(chunk_size_t))

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define c_size_from_data_size(dataSize) (_CHUNK_USED_SIZE(dataSize) > C_SIZE_MIN ? _CHUNK_USED_SIZE(dataSize) : C_SIZE_MIN)

#define C_SIZE  0b111111111111111111111111111111111111111111111111000ULL
#define C_FLAGS 0b000000000000000000000000000000000000000000000000111ULL
#define C_FREE  0b000000000000000000000000000000000000000000000000001ULL
#define C_USED  0b000000000000000000000000000000000000000000000000010ULL
#define C_DUMMY 0b000000000000000000000000000000000000000000000000100ULL

typedef u64 chunk_size_t;

typedef struct chunk_s chunk_s;

struct chunk_s {
    chunk_size_t size;
    chunk_s** ptr;
    chunk_s* next;
};

#define c_size_decode(s)      ((s) & C_SIZE)
#define c_size_is_free(s)     ((s) & C_FREE)
#define c_size_is_used(s)     ((s) & C_USED)

#define c_size_encode_free(s) ((s) | C_FREE)
#define c_size_encode_used(s) ((s) | C_USED)

#define c_data(c) ((void*)(c) + sizeof(chunk_size_t))
#define c_data_size(s) ((u64)(s) - 2*sizeof(chunk_size_t)) // DADO UM CHUNK USED DE TAL TAMANHO, CALCULA O TAMANHO DOS DADOS
#define c_next_(c) ((void*)&((((chunk_s*)(c))->next)))
#define c_size2(c, s) ((chunk_size_t*)((void*)(c) + (u64)(s) - sizeof(chunk_size_t)))

#define c_from_data(d) ((void*)(d) - sizeof(chunk_size_t))

#define c_is_free(c) c_size_is_free(((chunk_s*)(c))->size)
#define c_is_used(c) c_size_is_used(((chunk_s*)(c))->size)

#define c_get_size(c) c_size_decode(((chunk_s*)(c))->size)
#define c_get_ptr(c) ((void**)(((chunk_s*)(c))->ptr))
#define c_get_next(c) ((void*)(((chunk_s*)(c))->next))
#define c_get_size2(c, s) c_size_decode(*c_size2(c, s))

#define c_set_size_free(c, s) (((chunk_s*)(c))->size = c_size_encode_free(s))
#define c_set_size_used(c, s) (((chunk_s*)(c))->size = c_size_encode_used(s))
#define c_set_ptr(c, p)  (((chunk_s*)(c))->ptr = (chunk_s**)(p))
#define c_set_next(c, n) (((chunk_s*)(c))->next = (chunk_s*)(n))

#define c_left(c) ({ void* const __c = (c); (__c - c_left_get_size(__c)); })
#define c_left_get_size(c) c_size_decode(*(chunk_size_t*)((void*)(c) - sizeof(chunk_size_t)))

#define c_right(c, s) ((void*)(c) + (s))

#define ASSERT_ADDR_IN_BUFFER(a) assert( BUFF <= (void*)(a) && (void*)(a) < BUFF_LMT )
#define ASSERT_ADDR_IN_CHUNKS(a) assert( (void*)BUFF_CHUNKS <= (void*)(a) && (void*)(a) < (void*)BUFF_R )

static void* BUFF;

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

    c->ptr = ptr;
    c->next = *ptr;

    if (*ptr)
        (*ptr)->ptr = &c->next;

    *ptr = c;
}

static inline void c_free_unregister (chunk_s* const c) {

    if (c->next)
        c->next->ptr = c->ptr;

    *c->ptr = c->next;
}

void dedipy_free (void* const d) {
    if (d) {
        chunk_s* c = c_from_data(d);

        u64 s = c_get_size(c);

        chunk_s* const left = c_left(c);

        if (left->size & C_FREE) {
            s += left->size & C_SIZE;
            c_free_unregister((c = left));
        }

        chunk_s* const right = c_right(c, s);

        if (right->size & C_FREE) {
            s += right->size & C_SIZE;
            c_free_unregister(right);
        }

        c_free_fill_and_register(c, s);
    }
}

void* dedipy_malloc (size_t size_) {

    u64 size = c_size_from_data_size(size_);

    if (size > C_SIZE_MAX)
        return NULL;

    chunk_s** ptr = root_get_ptr(size);

    chunk_s* used;

    while ((used = *ptr) == NULL)
        ptr++;

    if (used == (chunk_s*)C_DUMMY)
        return NULL;

    u64 usedSize = used->size & C_SIZE;

    const u64 freeSizeNew = usedSize - size;

    c_free_unregister(used);

    if (freeSizeNew >= C_SIZE_MIN) {
        c_free_fill_and_register(used, freeSizeNew);
        used = (void*)used + freeSizeNew;
        usedSize = size;
    }

    *c_size2(used, usedSize) = used->size = usedSize | C_USED;

    return c_data(used);
}

void* dedipy_calloc (size_t n, size_t size_) {

    const u64 size = (u64)n * (u64)size_;

    void* const data = dedipy_malloc(size);

    if (data)
        memset(data, 0, size);

    return data;
}

void* dedipy_realloc (void* const d_, const size_t dsNew_) {

    if (d_ == NULL)
        return dedipy_malloc(dsNew_);

    u64 sNew = c_size_from_data_size(dsNew_);

    if (sNew > C_SIZE_MAX)
        return NULL;

    chunk_s* const c = c_from_data(d_);

    u64 s = c->size & C_SIZE;

    if (s >= sNew) {
        if ((s - sNew) < 64)
            // MAS NÃO VALE A PENA DIMINUIR
            return d_;
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return d_;
    }

    chunk_s* r = c_right(c, s);

    if (r->size & C_FREE) {

        const u64 rs = r->size & C_SIZE;

        const u64 rsCut = sNew - s;

        if (rs >= rsCut) {

            const u64 rsNew = rs - rsCut;

            c_free_unregister(r);

            if (rsNew >= C_SIZE_MIN) {
                s += rsCut;
                r = (void*)r + rsCut;
                c_free_fill_and_register(r, rsNew);
            } else
                s += rs;

            *c_size2(c, s) = c->size = s | C_USED;

            return c_data(c);
        }
    }

    void* const d = dedipy_malloc(dsNew_);

    if (d) {
        memcpy(d, d_, c_data_size(s));

        dedipy_free(c_data(c));
    }

    return d;
}

void* dedipy_reallocarray (void *ptr, size_t nmemb, size_t size) {

    (void)ptr;
    (void)nmemb;
    (void)size;

    fatal("MALLOC - REALLOCARRAY");
}

#if DEDIPY_TEST
static inline u64 dedipy_test_random (const u64 x) {

    static volatile u64 _rand = 0;

    _rand += x;
    _rand += rdtsc() & 0xFFFULL;
#if 0
    _rand += __builtin_ia32_rdrand64_step((uintll*)&_rand);
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

// MUST HAVE SAME ALIGNMENTS AS MALLOC! :/ @_@
static void dedipy_test_verify (void) {

    // LEFT/RIGHT
    assert ( *BUFF_L == C_DUMMY );
    assert ( *BUFF_R == C_DUMMY );

    assert( (BUFF + BUFF_INFO->size) == BUFF_LMT );

    u64 totalFree = 0;
    u64 totalUsed = 0;

    u64 countFree = 0;
    u64 countUsed = 0;

    const chunk_s* c = BUFF_CHUNKS;

    while (c != (chunk_s*)BUFF_R) {

        ASSERT_ADDR_IN_CHUNKS(c);

        assert(((uintptr_t)c % CHUNK_ALIGNMENT) == 0);

        //assert(in_chunks(c, C_SIZE_MIN));

        const u64 s = c->size & C_SIZE;

        //assert(in_chunks(c, s));

        assert(s & C_SIZE); // NÃO É 0
        assert((s & C_FLAGS) == 0); // NÃO TEM NENHUMA FLAG
        assert((s & C_SIZE) >= C_SIZE_MIN);
        assert((s & C_SIZE) <= C_SIZE_MAX);
        assert((s & C_SIZE) == s); // O TAMANHO ESTÁ DENTRO DA MASK DE TAMANHO
        assert(((s & C_SIZE) % CHUNK_ALIGNMENT) == 0); // ESTÁ ALINHADO

        assert((c->size & ~C_FLAGS) == (c->size & C_SIZE)); // O TAMANHO NÃO EXTRAPOLA A MASK DO SIZE
        assert(c->size == *c_size2(c, s));

        if (c->size & C_FREE) {
            assert((c->size & C_FLAGS) == C_FREE);
            assert(c->ptr);
            //assert(in_buff(c->ptr, sizeof(chunk_s*)));
            assert(*c->ptr == c);
            ASSERT_ADDR_IN_BUFFER(c->ptr);
            assert(*(c->ptr) == c);
            if (c->next) {
                ASSERT_ADDR_IN_CHUNKS(c->next);
                assert(c->next->size & C_FREE);
                assert(c->next->ptr == &c->next);
                assert(((uintptr_t)c->next % CHUNK_ALIGNMENT) == 0);
                //assert(in_chunks(c->next, C_SIZE_MIN));
                //assert(in_chunks(c->next, c->next->size));
                assert(c->next->ptr == &c->next);
            }
            totalFree += s;
            countFree++;
        } else {
            assert((c->size & C_FLAGS) == C_USED); // FLAG USED ESTÁ SETADA, E SOMENTE ELA ESTÁ
            assert(c_from_data(c_data(c)) == c);
            assert(((uintptr_t)c_data(c) % DATA_ALIGNMENT) == 0);
            totalUsed += s;
            countUsed++;
        }

        c = c_right(c, s);
    }

    const u64 total = totalFree + totalUsed;

    //dbg("-- TOTAL %llu FREE COUNT %llu SIZE %llu USED COUNT %llu SIZE %llu ------", (uintll)total, (uintll)countFree, (uintll)totalFree, (uintll)countUsed, (uintll)totalUsed);

    assert(total == BUFF_CHUNKS_SIZE);

    // VERIFICA OS FREES
    int idx = 0; chunk_s** ptrRoot = BUFF_ROOTS;

    u64 lastSlotMaximum = 0;

    do {
        chunk_s* const* ptr = ptrRoot;
        const chunk_s* c = *ptrRoot;

        u64 thisSlotMaximum = lastSlotMaximum;

        while (c) { const u64 s = c->size & C_SIZE;
            ASSERT_ADDR_IN_CHUNKS(c);
            ASSERT_ADDR_IN_CHUNKS((void*)c + s - 1);
            assert(s >= C_SIZE_MIN);
            assert(s <  C_SIZE_MAX);
            assert(c->size & C_FREE);
            assert(s >= lastSlotMaximum);
            if (s > thisSlotMaximum)
                thisSlotMaximum = s;
            // assert(c_put_idx(s) == slot);
            assert(c->size == *c_size2(c, s));
            assert(c->ptr == ptr);

            totalFree -= s;
            countFree--;

            ptr = &c->next;
            c = c->next;
        }

        ptrRoot++;

        lastSlotMaximum = thisSlotMaximum;

    } while (++idx != ROOTS_N);

    assert (ptrRoot == (chunk_s**)BUFF_ROOTS_LMT);

    assert (totalFree == 0);
    assert (countFree == 0);
}
#endif

void dedipy_main (void) {

    // SUPPORT CALLING FROM MULTIPLE PLACES
    static int initialized = 0;

    if (initialized)
        return;

    initialized = 1;

    uintll buffFD    = 0;
    uintll buffFlags = 0;
    uintll buffAddr  = 0;
    uintll buffTotal = 0;
    uintll buffStart = 0;
    uintll buffSize  = 0;
    uintll cpu       = 0;
    uintll pid       = 0;
    uintll code      = 0;
    uintll started   = 0;
    uintll id        = 0;
    uintll n         = 0;
    uintll groupID   = 0;
    uintll groupN    = 0;

    const char* var = getenv("DEDIPY");

    if (var) {
        if (sscanf(var, "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX"  "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX",
            &cpu, &pid, &buffFD, &buffFlags, &buffAddr, &buffTotal, &buffStart, &buffSize, &code, &started, &id, &n, &groupID, &groupN) != 14)
            fatal("FAILED TO LOAD ENVIROMENT PARAMS");
    } else { // EMERGENCY MODE =]
        cpu = (uintll)sched_getcpu();
        buffFD = 0;
        buffFlags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_FIXED_NOREPLACE;
        buffAddr = (uintll)BUFF_ADDR;
        buffTotal = 256*1024*1024;
        buffStart = 0;
        buffSize = buffTotal;
        pid = (uintll)getpid();
        n = 1;
        groupN = 1;
    }

    // THOSE ARE FOR EMERGENCY/DEBUGGING
    // THEY ARE IN DECIMAL, FOR MANUAL SETTING IN THE C SOURCE/SHELL
    if ((var = getenv("DEDIPY_BUFF_ADDR"  ))) buffAddr    = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_FD"    ))) buffFD      = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_FLAGS" ))) buffFlags   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_TOTAL" ))) buffTotal   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_START" ))) buffStart   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_SIZE"  ))) buffSize    = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_CPU"        ))) cpu         = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_ID"         ))) id          = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_N"          ))) n           = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_GROUP_ID"   ))) groupID     = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_GROUP_N"    ))) groupN      = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_PID"        ))) pid         = strtoull(var, NULL, 10);

    if ((void*)buffAddr != BUFF_ADDR)
        fatal("BUFFER ADDRESS MISMATCH");

    if (buffSize == 0)
        fatal("BAD BUFFER SIZE");

    if ((buffStart + buffSize) > buffTotal)
        fatal("BAD BUFFER START");

    if ((int)cpu != sched_getcpu())
        fatal("CPU MISMATCH");

    if ((pid_t)pid != getpid())
        fatal("PID MISMATCH");

    if (n == 0 || n > 0xFFFF)
        fatal("BAD N");

    if (id >= n)
        fatal("BAD ID/N");

    if (groupN == 0 || groupN > 0xFFFF)
        fatal("BAD GROUP N");

    if (groupID >= groupN)
        fatal("BAD GROUP ID/N");

    //  | MAP_LOCKED | MAP_POPULATE | MAP_HUGETLB | MAP_HUGE_1GB
    if (mmap(BUFF_ADDR, buffTotal, PROT_READ | PROT_WRITE, (int)buffFlags, (int)buffFD, 0) != BUFF_ADDR)
        fatal("FAILED TO MAP BUFFER");

    if (close((int)buffFD))
        fatal("FAILED TO CLOSE BUFFER FD");

    BUFF = (void*)BUFF_ADDR + buffStart;

    memset(BUFF, 0, buffSize);

    BUFF_INFO->cpu      = (u16)cpu;
    BUFF_INFO->id       = (u16)id;
    BUFF_INFO->n        = (u16)n;
    BUFF_INFO->groupID  = (u16)groupID;
    BUFF_INFO->groupN   = (u16)groupN;
    BUFF_INFO->code     = (u64)code;
    BUFF_INFO->pid      = (u64)pid;
    BUFF_INFO->started  = (u64)started;
    BUFF_INFO->start    = (u64)buffStart;
    BUFF_INFO->size     = (u64)buffSize;
    BUFF_INFO->total    = (u64)buffTotal;

    char name[256];
DBGPRINT("TAMO AQUI MANO");
    if (snprintf(name, sizeof(name), PROGNAME "#%u", (uint)BUFF_INFO->id) < 2 || prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0))
        fatal("FAILED TO SET PROCESS NAME");

    *BUFF_L = C_DUMMY;
    *BUFF_R = C_DUMMY;

    // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O C_SIZE_MAX E ROOTS_SIZES_LST SÃO MAIORES DO QUE ELE
    c_free_fill_and_register(BUFF_CHUNKS, BUFF_CHUNKS_SIZE);

    // TODO: FIXME: tentar dar malloc() e realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // C_SIZE_MAX
    // ROOTS_SIZES_LST
    // ROOTS_SIZES_LMT

    //assert(C_SIZE_MIN == ROOTS_SIZES_FST);
    //assert(C_SIZE_MAX <  ROOTS_SIZES_LMT);

    //assert(ROOTS_SIZES_FST < ROOTS_SIZES_LST);
    //assert(ROOTS_SIZES_LST < ROOTS_SIZES_LMT);

    assert(root_get_ptr(C_SIZE_MIN) == (BUFF_ROOTS));
    assert(root_get_ptr(C_SIZE_MAX) == (BUFF_ROOTS + (ROOTS_N - 1)));

    assert(root_put_ptr(C_SIZE_MIN) == (BUFF_ROOTS));
    assert(root_put_ptr(C_SIZE_MAX) <= (BUFF_ROOTS + (ROOTS_N - 1))); // COMO VAI ARREDONDAR PARA BAIXO, PEDIR O MÁXIMO PODE CAIR LOGO ANTES DO ÚLTIMO SLOT

    assert(BUFF_ROOTS_LMT == (BUFF_ROOTS + ROOTS_N));

    dbg("BUFF_INFO BX%llX",      BOFFSET(BUFF_INFO));
    dbg("BUFF_INFO->id %llu",    (uintll)BUFF_INFO->id);
    dbg("BUFF_INFO->pid %llu",   (uintll)BUFF_INFO->pid);
    dbg("BUFF_INFO->cpu %llu",   (uintll)BUFF_INFO->cpu);
    dbg("BUFF_INFO->start %llu", (uintll)BUFF_INFO->start);
    dbg("BUFF_INFO->size %llu",  (uintll)BUFF_INFO->size);
    dbg("BUFF_INFO->code %llu",  (uintll)BUFF_INFO->code);

    ASSERT_ADDR_IN_BUFFER(BUFF_CHUNKS);
    ASSERT_ADDR_IN_CHUNKS(BUFF_CHUNKS);

    // A CHANGE ON THOSE MAY REQUIRE A REVIEW
    assert(8 == sizeof(chunk_size_t));
    assert(8 == sizeof(u64));
    assert(8 == sizeof(off_t));
    assert(8 == sizeof(size_t));
    assert(8 == sizeof(void*));

#if DEDIPY_TEST
    dbg("TEST 0");

    { uint c = 200;
        while (c--) {

            dedipy_free(NULL);

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
    { uint c = 500;
        while (c--) {

            dbg("COUNTER %u", c);

            dedipy_test_verify();

            void** last = NULL;

            void** new; u64 size;

            while ((new = dedipy_malloc((size = sizeof(void**) + dedipy_test_size(c))))) {

                memset(new, 0xFF, size);

                if (dedipy_test_random(c) % 8 == 0) { void** new2; u64 size2;
                    if ((new2 = dedipy_realloc(new, (size2 = sizeof(void**) + dedipy_test_size(c)))))
                        memset((new = new2), 0xFF, (size = size2));
                }

                if (dedipy_test_random(c) % 8 == 0) {
                    dedipy_free(new);
                    continue;
                }

                // TODO: FIXME: uma porcentagem, dar dedipy_free() aqui ao invés de incluir na lista
                *new = last;
                last = new;
            }

            dedipy_test_verify();

            while (last) {
                if (dedipy_test_random(c) % 8 == 0)
                    last = dedipy_realloc(last, sizeof(void**) + dedipy_test_size(c)) ?: last;
                void** old = *last;
                dedipy_free(last);
                last = old;
            }
        }
    }

    dedipy_test_verify();

    // TODO: FIXME: ALOCAR SIMULTANEAMENTE O MAXIMO POSSIVEL
    //      -> quando este blockSize der malloc() null, ir reduzindo ele aqté que nem mesmo size 1 possa ser alocado
    // si depois, partir para um novo block size
    { u64 blockSize = 64*1024; // >= sizeof(void**)

        do { u64 count = 0; void** last = NULL; void** this;

            while ((this = dedipy_malloc(blockSize))) {
                *this = last;
                last = this;
                count += 1;
            }

            while (last) {
                this = *last;
                dedipy_free(last);
                last = this;
            }

            if (count)
                dbg("ALLOCATED %llu BLOCKS of %llu BYTES = %llu", (uintll)count, (uintll)blockSize, (uintll)(count * blockSize));

            dedipy_test_verify();

        } while ((blockSize <<= 1));
    }

    dedipy_test_verify();

    dbg("TEST DONE");
#endif
}
