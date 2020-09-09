/*

*/

#define _GNU_SOURCE 1

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>

#include "util.h"

#include "dedipy-lib.h"

#define C_SIZE_MIN 32ULL
#define C_SIZE_MAX C_SIZE

#define _CHUNK_USED_SIZE(dataSize) (sizeof(chunk_size_t) + ((((u64)(dataSize) + 7ULL) & ~0b111ULL)) + sizeof(chunk_size_t))

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define c_size_from_data_size(dataSize) (_CHUNK_USED_SIZE(dataSize) > C_SIZE_MIN ? _CHUNK_USED_SIZE(dataSize) : C_SIZE_MIN)

#define C_SIZE  0b111111111111111111111111111111111111111111111111000ULL // NOTE: JÁ ALINHADO, PORTANTO UM VALOR SIZE FICTÍCIO PARA SER USADO COMO C_SIZE_MAX
#define C_FREE  0b000000000000000000000000000000000000000000000000001ULL

struct chunk_s {
    chunk_size_t size;
    chunk_s** ptr;
    chunk_s* next;
};

#define c_data(c) ((char*)(c) + sizeof(chunk_size_t))
#define c_data_size(s) ((u64)(s) - 2*sizeof(chunk_size_t)) // DADO UM CHUNK USED DE TAL TAMANHO, CALCULA O TAMANHO DOS DADOS
#define c_size2(c, s) ((chunk_size_t*)((char*)(c) + (u64)(s) - sizeof(chunk_size_t)))

#define c_from_data(d) ((chunk_s*)((char*)(d) - sizeof(chunk_size_t)))

#define c_left(c)    ((chunk_s*)((char*)(c) - ((*(chunk_size_t*)((char*)(c) - sizeof(chunk_size_t))) & C_SIZE)))
#define c_right(c, s) ((chunk_s*)((char*)(c) + s))

#define assert_addr_in_buffer(a, s) dedipy_assert( (char*)BUFFER         <= (char*)(a) && ((char*)(a) + (s)) <= ((char*)BUFFER + DEDIPY_BUFFER_SIZE) )
#define assert_addr_in_chunks(a, s) dedipy_assert( (char*)BUFFER->chunks <= (char*)(a) && ((char*)(a) + (s)) <=  (char*)&BUFFER->r )

//
volatile sig_atomic_t sigTERM;
volatile sig_atomic_t sigUSR1;
volatile sig_atomic_t sigUSR2;
volatile sig_atomic_t sigALRM;
volatile sig_atomic_t sigCHLD;

static void signal_handler (const int signal, const siginfo_t* const restrict signalInfo, const void* const signalData) {

    (void)signalInfo; (void)signalData;

    switch (signal) {
        case SIGTERM: sigTERM = 1; break;
        case SIGINT:  sigTERM = 1; break;
        case SIGUSR1: sigUSR1 = 1; break;
        case SIGUSR2: sigUSR2 = 1; break;
        case SIGALRM: sigALRM = 1; break;
        case SIGCHLD: sigCHLD = 1; break;
    }
}

/*
    COMMON BETWEEN THE DAEMON AND THE INTERPRETER
*/

#if!(_LARGEFILE64_SOURCE && _GNU_SOURCE == 1 && _FILE_OFFSET_BITS == 64)
#error
#endif

#ifndef DEDIPY_TEST_1_COUNT
#define DEDIPY_TEST_1_COUNT 500
#endif

#ifndef DEDIPY_TEST_2_COUNT
#define DEDIPY_TEST_2_COUNT 150
#endif

#ifndef PROGNAME
#define PROGNAME "dedipy"
#endif

#define DEDIPY_VERSION 0ULL

#define DEDIPY_CHECK ((u64)( \
       sizeof(buffer_s) \
    + (sizeof(chunk_s)  << 32) \
    + (sizeof(worker_s) << 33) \
    + (sizeof(group_s)  << 34) \
    + (sizeof(io_msg_s) << 35) \
    ))

#define DEDIPY_BUFFER_FD 0

typedef enum io_msg_code_e io_msg_code_e;

typedef struct io_msg_s io_msg_s;

#define IO_QUEUES_N 4

//volatile atomic_t ioOwners[IO_QUEUES_N];
//io_msg_s* ioQueues[IO_QUEUES_N];

#define MSG_CONNECT_FLAG_SSL
#define MSG_CONNECT_FLAG_SSL_DONT_CHECK_CERT
#define MSG_CONNECT_FLAG_NO_RESOLVE

enum io_msg_code_e {
    MSG_RESOLVE,
    MSG_RESOLVE_RES,
    MSG_CONNECT,
    MSG_CONNECT_RES,
    MSG_RECV,
    MSG_RECV_RES,
    MSG_SEND,
    MSG_SEND_RES,
    MSG_CLOSE,
    MSG_CLOSE_RES,
};

struct io_msg_s {
    io_msg_s* next;
    u64 code:6;
    u64 fd:22;
    u64 flags:36;
    u64 timeout;
    union {
        struct connect { u32 port; u16 hostnameLen; u16 sslHostnameLen; char* hostname; char* sslHostname; } connect;
        struct connectRes { uint fd; int error; } connectRes;
        struct send { uint size; uint sizeMin; char* buff; } send;
        struct recv { uint size; uint sizeMin; char* buff; } recv;
        struct sendRes { int error; uint sent; } sendRes;
        struct recvRes { int error; uint received; } recvRes;
    };
};

uint myID = DEDIPY_ID_NONE;

static inline void c_free_fill_and_register (chunk_s* const c, u64 s) {

    assert_addr_in_chunks(c, s);

    *c_size2(c, s) = c->size = s | C_FREE;

    s =
        (s <= ROOTS_MAX_0) ? (s >> ROOTS_DIV_0) + ROOTS_GROUPS_OFFSET_0 :
        (s <= ROOTS_MAX_1) ? (s >> ROOTS_DIV_1) + ROOTS_GROUPS_OFFSET_1 :
        (s <= ROOTS_MAX_2) ? (s >> ROOTS_DIV_2) + ROOTS_GROUPS_OFFSET_2 :
        (s <= ROOTS_MAX_3) ? (s >> ROOTS_DIV_3) + ROOTS_GROUPS_OFFSET_3 :
        (s <= ROOTS_MAX_4) ? (s >> ROOTS_DIV_4) + ROOTS_GROUPS_OFFSET_4 :
        (s <= ROOTS_MAX_5) ? (s >> ROOTS_DIV_5) + ROOTS_GROUPS_OFFSET_5 :
        (s <= ROOTS_MAX_6) ? (s >> ROOTS_DIV_6) + ROOTS_GROUPS_OFFSET_6 :
        (s <= ROOTS_MAX_7) ? (s >> ROOTS_DIV_7) + ROOTS_GROUPS_OFFSET_7 :
            ROOTS_N - 1
        ;

    if ((c->next = *(c->ptr = &BUFFER->roots[s])))
        (*c->ptr)->ptr = &c->next;
    else { // SABEMOS QUE AGORA TEM PELO MENOS UM NESTES GRUPOS
        BUFFER->tops1[s >>  6] |= 1ULL << ( s       & 0b111111U);
        BUFFER->tops0[s >> 12] |= 1ULL << ((s >> 6) & 0b111111U);
    }

    *c->ptr = c;
}

static inline void c_free_unregister (chunk_s* const c) {

    assert_addr_in_chunks(c, C_SIZE_MIN);

    if (c->next) {
        c->next->ptr = c->ptr;
        *c->ptr = c->next;
    } else { *c->ptr = NULL;
        if (c->ptr < &BUFFER->roots[ROOTS_N]) {
            const uint idx = (uint)(c->ptr - BUFFER->roots);
            if(!(BUFFER->tops1[idx >>  6] ^= 1ULL << ( idx      & 0b111111U)))
                 BUFFER->tops0[idx >> 12] ^= 1ULL << ((idx >>6) & 0b111111U);
        }
    }
}

void dedipy_free_ (void* const d) {

    if (d) {
        chunk_s* c = c_from_data(d);

        assert_addr_in_chunks(c, C_SIZE_MIN);

        u64 s = c->size & C_SIZE;

        assert_addr_in_chunks(c, s);

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

void free (void* const d) {
    dedipy_alloc_acquire();
    dedipy_free_(d);
    dedipy_alloc_release();
}

void* dedipy_malloc_ (const size_t size_) {

    u64 size = size_;

    if (size == 0)
        return NULL;

    if ((size = c_size_from_data_size(size)) >= C_SIZE_MAX)
        return NULL;

    // O INDEX, A PARTIR DO QUAL, TODOS OS CHUNKS GARANTEM ESTE SIZE

    u64 idx =
        (size <= ROOTS_MAX_0) ? (size >> ROOTS_DIV_0) + (!!(size & ROOTS_GROUPS_REMAINING_0)) + ROOTS_GROUPS_OFFSET_0 :
        (size <= ROOTS_MAX_1) ? (size >> ROOTS_DIV_1) + (!!(size & ROOTS_GROUPS_REMAINING_1)) + ROOTS_GROUPS_OFFSET_1 :
        (size <= ROOTS_MAX_2) ? (size >> ROOTS_DIV_2) + (!!(size & ROOTS_GROUPS_REMAINING_2)) + ROOTS_GROUPS_OFFSET_2 :
        (size <= ROOTS_MAX_3) ? (size >> ROOTS_DIV_3) + (!!(size & ROOTS_GROUPS_REMAINING_3)) + ROOTS_GROUPS_OFFSET_3 :
        (size <= ROOTS_MAX_4) ? (size >> ROOTS_DIV_4) + (!!(size & ROOTS_GROUPS_REMAINING_4)) + ROOTS_GROUPS_OFFSET_4 :
        (size <= ROOTS_MAX_5) ? (size >> ROOTS_DIV_5) + (!!(size & ROOTS_GROUPS_REMAINING_5)) + ROOTS_GROUPS_OFFSET_5 :
        (size <= ROOTS_MAX_6) ? (size >> ROOTS_DIV_6) + (!!(size & ROOTS_GROUPS_REMAINING_6)) + ROOTS_GROUPS_OFFSET_6 :
        (size <= ROOTS_MAX_7) ? (size >> ROOTS_DIV_7) + (!!(size & ROOTS_GROUPS_REMAINING_7)) + ROOTS_GROUPS_OFFSET_7 :
            ROOTS_N - 1
        ;

    chunk_s* used; u64 tid; u64 found;

    if ((found = BUFFER->tops1[idx >> 6] & (0xFFFFFFFFFFFFFFFFULL << (idx & 0b111111ULL)))) {
        found = ctz_u64(found) | (idx & ~(u64)0b111111ULL); // TODO: FIXME: WHY IS THIS GIVING ME A WARN?
        goto FOUND;
    }

    tid = idx >> 12;

    if ((found = BUFFER->tops0[tid] & (0xFFFFFFFFFFFFFFFFULL << (((idx >> 6) + 1) & 0b111111ULL)))) {
        tid  = (tid << 6) | ctz_u64(found);
        found = (tid << 6) | ctz_u64(BUFFER->tops1[tid]);
        if (found >= idx)
            goto FOUND;
    }

    idx >>= 12;

    do {
        if (++idx == TOPS0_N)
            return NULL;
    } while (!(found = BUFFER->tops0[idx]));

    idx   = (idx << 6) | ctz_u64(found);
    found = (idx << 6) | ctz_u64(BUFFER->tops1[idx]);

FOUND:

    dedipy_assert(found < ROOTS_N);

    used = BUFFER->roots[found];

    dedipy_assert(used);

    u64 usedSize = used->size & C_SIZE;

    dedipy_assert(usedSize >= size_);

    const u64 freeSizeNew = usedSize - size;

    c_free_unregister(used);

    if (freeSizeNew >= C_SIZE_MIN) {
        c_free_fill_and_register(used, freeSizeNew);
        used = (chunk_s*)((char*)used + freeSizeNew);
        usedSize = size;
    }

    *c_size2(used, usedSize) = used->size = usedSize;

    return c_data(used);
}

void* malloc (const size_t size_) {
    dedipy_alloc_acquire();
    void* const ret = dedipy_malloc_(size_);
    dedipy_alloc_release();
    return ret;
}

void* dedipy_calloc_ (const size_t n, const size_t size_) {

    const u64 size = (u64)n * (u64)size_;

    void* const data = malloc(size);

    if (data)
        memset(data, 0, size);

    return data;
}

void* calloc (const size_t n, const size_t size_) {
    dedipy_alloc_acquire();
    void* const ret = dedipy_calloc_(n, size_);
    dedipy_alloc_release();
    return ret;
}

void* dedipy_realloc_ (void* const d_, const size_t dsNew_) {

    if (d_ == NULL) // GLIBC: IF PTR IS NULL, THEN THE CALL IS EQUIVALENT TO MALLOC(SIZE), FOR ALL VALUES OF SIZE
        return dedipy_malloc_(dsNew_);

    if (dsNew_ == 0) { // GLIBC: IF SIZE IS EQUAL TO ZERO, AND PTR IS NOT NULL, THEN THE CALL IS EQUIVALENT TO FREE(PTR)
        dedipy_free_(d_);
        return NULL;
    }

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
                r = (chunk_s*)((char*)r + rsCut);
                c_free_fill_and_register(r, rsNew);
            } else
                s += rs;

            *c_size2(c, s) = c->size = s;

            return c_data(c);
        }
    }

    void* const d = dedipy_malloc_(dsNew_);

    if (d) {
        memcpy(d, d_, c_data_size(s));

        dedipy_free_(c_data(c));
    }

    return d;
}

void* realloc (void* const d_, const size_t dsNew_) {
    dedipy_alloc_acquire();
    void* const ret = dedipy_realloc_(d_, dsNew_);
    dedipy_alloc_release();
    return ret;
}

void* reallocarray (void *ptr, size_t nmemb, size_t size) {

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
void dedipy_verify (void) {

    dbg3("VERIFY");

    // A CHANGE ON THOSE WILL REQUIRE A REVIEW
    assert(8 == sizeof(chunk_size_t));
    assert(8 == sizeof(u64));
    assert(8 == sizeof(off_t));
    assert(8 == sizeof(size_t));
    assert(8 == sizeof(void*));

    assert(BUFFER->version  == DEDIPY_VERSION);
    assert(BUFFER->check    == DEDIPY_CHECK);
    assert(BUFFER->size     == DEDIPY_BUFFER_SIZE);
    assert(BUFFER->workersN == WORKERS_N);

    //
    assert(sizeof(*BUFFER) == DEDIPY_BUFFER_SIZE);

    // LEFT/RIGHT
    assert(BUFFER->l == BUFFER_L);
    assert(BUFFER->r == BUFFER_R);

    u64 totalFree = 0;
    u64 totalUsed = 0;

    u64 countFree = 0;
    u64 countUsed = 0;

    dbg3("VERIFY - ALL CHUNKS");

    const chunk_s* c = (chunk_s*)BUFFER->chunks;

    while ((void*)c != (void*)&BUFFER->r) {

        assert_addr_in_chunks(c, C_SIZE_MIN);
        assert_addr_in_chunks(c, c->size & C_SIZE);

        assert(((uintptr_t)c % CHUNK_ALIGNMENT) == 0);

        const u64 s = c->size & C_SIZE;

        assert(s & C_SIZE); // NÃO É 0
        assert((s & C_SIZE) >= C_SIZE_MIN);
        assert((s & C_SIZE) <= C_SIZE_MAX);
        assert((s & C_SIZE) == s); // O TAMANHO ESTÁ DENTRO DA MASK DE TAMANHO
        assert(((s & C_SIZE) % CHUNK_ALIGNMENT) == 0); // ESTÁ ALINHADO

        assert(c->size == *c_size2(c, s));

        if (c->size & C_FREE) {
            assert(c->ptr);
            assert_addr_in_buffer(c->ptr, sizeof(chunk_s*));
            assert(*c->ptr == c);
            assert(*(c->ptr) == c);
            if (c->next) {
                assert_addr_in_chunks(c->next, C_SIZE_MIN);
                assert_addr_in_chunks(c->next, c->next->size & C_SIZE);
                assert(c->next->size & C_FREE);
                assert(c->next->ptr == &c->next);
                assert(((uintptr_t)c->next % CHUNK_ALIGNMENT) == 0);
                assert(c->next->ptr == &c->next);
            }
            totalFree += s;
            countFree++;
        } else {
            assert(c_from_data(c_data(c)) == c);
            assert(((uintptr_t)c_data(c) % DATA_ALIGNMENT) == 0);
            totalUsed += s;
            countUsed++;
        }

        c = c_right(c, s);
    }

    dbg3("USED COUNT %llu SIZE %llu + FREE COUNT %llu SIZE %llu", (uintll)countUsed, (uintll)totalUsed, (uintll)countFree, (uintll)totalFree);

    assert(sizeof(BUFFER->chunks) == (totalFree + totalUsed));

    // VERIFICA OS FREES
    dbg3("VERIFY - FREES");

    chunk_s** ptrRoot = BUFFER->roots;

    u64 lastSlotMaximum = 0;

    uint idx = 0;

    do {
        chunk_s* const* ptr = ptrRoot;

        u64 thisSlotMaximum = lastSlotMaximum;

        const chunk_s* c = *ptrRoot;

        while (c) {
            assert_addr_in_chunks(c, C_SIZE_MIN);
            assert_addr_in_chunks(c, c->size & C_SIZE);

            const u64 s = c->size & C_SIZE;

            assert(s >= C_SIZE_MIN);
            assert(s <  C_SIZE_MAX);
            assert(c->size & C_FREE);
            assert(s >= lastSlotMaximum);
            assert(c->size == *c_size2(c, s));
            assert(c->ptr == ptr);

            if (thisSlotMaximum < s)
                thisSlotMaximum = s;

            totalFree -= s;
            countFree--;

            ptr = &c->next;

            c = c->next;
        }

        ptrRoot++;

        lastSlotMaximum = thisSlotMaximum;

    } while (++idx != ROOTS_N);

    assert(ptrRoot == &BUFFER->roots[ROOTS_N]);

    assert(totalFree == 0);
    assert(countFree == 0);

    dbg3("VERIFY - DONE");
}

static void dedipy_test_0 (void) {

    dbg("TEST 0");

    for (uint c = 200; c; c--) {

        free(NULL);

        assert(malloc(0) == NULL);

        free(malloc(dedipy_test_size(c + 1)));
        free(malloc(dedipy_test_size(c + 2)));
        free(malloc(dedipy_test_size(c + 3)));

        free(realloc(malloc(dedipy_test_size(c + 4)), dedipy_test_size(c + 10)));
        free(realloc(malloc(dedipy_test_size(c + 5)), dedipy_test_size(c + 11)));

        free(malloc(dedipy_test_size(c + 6)));
        free(malloc(dedipy_test_size(c + 7)));

        free(realloc(malloc(dedipy_test_size(c + 8)), dedipy_test_size(c + 12)));
        free(realloc(malloc(dedipy_test_size(c + 9)), dedipy_test_size(c + 13)));
    }

    dbg("TEST 0 - DONE");
}

static void dedipy_test_1 (void) {

    dbg("TEST 1");

    for (uint c = 500; c; c--) {

        dbg("TEST 1 - COUNTER %u", c);

        void** new; u64 size; void** last = NULL;

        while ((new = malloc((size = sizeof(void**) + dedipy_test_size(c))))) {

            memset(new, 0xFF, size);

            if (dedipy_test_random(c) % 8 == 0) { void** new2; u64 size2;
                if ((new2 = realloc(new, (size2 = sizeof(void**) + dedipy_test_size(c)))))
                    memset((new = new2), 0xFF, (size = size2));
            }

            if (dedipy_test_random(c) % 8 == 0) {
                free(new);
                continue;
            }

            // TODO: FIXME: uma porcentagem, dar free() aqui ao invés de incluir na lista
            *new = last;
            last = new;
        }

        while (last) {
            if (dedipy_test_random(c) % 8 == 0)
                last = realloc(last, sizeof(void**) + dedipy_test_size(c)) ?: last;
            void** old = *last;
            free(last);
            last = old;
        }
    }

    dbg("TEST 1 - DONE");
}

// TODO: FIXME: ALOCAR SIMULTANEAMENTE O MAXIMO POSSIVEL
//      -> quando este blockSize der malloc() null, ir reduzindo ele aqté que nem mesmo size 1 possa ser alocado
// si depois, partir para um novo block size
static void dedipy_test_2 (void) {

    dbg("TEST 2");

    u64 blockSize = 64*1024; // >= sizeof(void**)

    do { u64 count = 0; void** last = NULL; void** this;

        while ((this = malloc(blockSize))) {
            *this = last;
            last = this;
            count += 1;
        }

        while (last) {
            this = *last;
            free(last);
            last = this;
        }

        if (count)
            dbg("TEST 2 - ALLOCATED %llu BLOCKS of %llu BYTES = %llu", (uintll)count, (uintll)blockSize, (uintll)(count * blockSize));

    } while ((blockSize <<= 1));

    dbg("TEST 2 - DONE");
}

void dedipy_test (void) {

    dbg("TEST");
    // TODO: FIXME: tentar dar malloc() e realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // C_SIZE_MAX
    // ROOTS_SIZES_LST
    // ROOTS_SIZES_LMT
    dedipy_test_0();
    dedipy_test_1();
    dedipy_test_2();

    dbg("TEST - DONE");
}

#endif

static void dedipy_init_signals (void) {

    dbg("INSTALLING SIGNAL HANDLER");

    // INSTALL THE SIGNAL HANDLER
    sigTERM = sigUSR1 = sigUSR2 = sigALRM = sigCHLD = 0;

    struct sigaction action;

    memset(&action, 0, sizeof(action));

    action.sa_sigaction = (void*)signal_handler; // TODO: FIXME: correct cast
    action.sa_flags = SA_SIGINFO;

    for (int sig = NSIG ; sig ; sig--)
        if (sigaction(sig, &action, NULL))
            switch (sig) {
                case SIGINT:
                case SIGTERM:
                case SIGUSR1:
                case SIGUSR2:
                case SIGCHLD:
                case SIGALRM:
                    fatal("FAILED TO INSTALL SIGNAL HANDLER");
            }

}

void dedipy_main_daemon (void) {

    log("LIB MAIN");

    log("BUFFER SIZE %llu", (uintll)DEDIPY_BUFFER_SIZE);

    dedipy_init_signals();

    //
    myID = DEDIPY_ID_DAEMON;

    //
    close(0);

    // OPEN THE BUFFER FILE
    if (open(DEDIPY_BUFFER_PATH, O_RDWR | O_CREAT) != DEDIPY_BUFFER_FD)
        fatal("FAILED TO OPEN BUFFER");

    if (ftruncate(DEDIPY_BUFFER_FD, DEDIPY_BUFFER_SIZE))
        fatal("FTRUNCATE FAILED");

    // TODO: FIXME: TER CERTEZA QUE CARREGOU TOOS OS HOLES :S
    //  | MAP_HUGETLB | MAP_HUGE_1GB
    if (mmap(BUFFER, DEDIPY_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_FIXED_NOREPLACE, DEDIPY_BUFFER_FD, 0) != BUFFER)
        fatal("FAILED TO MAP BUFFER");

    // ter certeza de que tem este tamanho
    // AO MESMO TEMPO, PREVINE QUE WRITE() NESTE FD ESCREVE SOBRE A MEMÓRIA
    // TODO: FIXME: isso adianta, ou precisa testar um write()? :/
    if (lseek(DEDIPY_BUFFER_FD, DEDIPY_BUFFER_SIZE, SEEK_SET) != DEDIPY_BUFFER_SIZE)
        fatal("FAILED TO SEEK BUFFER FD");

    dedipy_assert(sizeof(*BUFFER) == DEDIPY_BUFFER_SIZE);

    dedipy_assert(DEDIPY_BUFFER_SIZE > 32*1024*1024);
    dedipy_assert(DEDIPY_BUFFER_SIZE < 16ULL*1024*1024*1024*1024*1024);

    log("CLEARING BUFFER MEMORY...");

    memset((void*)BUFFER, 0, DEDIPY_BUFFER_SIZE);

    BUFFER->check    = DEDIPY_CHECK;
    BUFFER->version  = DEDIPY_VERSION;
    BUFFER->size     = DEDIPY_BUFFER_SIZE;
    BUFFER->workersN = WORKERS_N;
    BUFFER->aOwner   = DEDIPY_ID_NONE;
    BUFFER->iOwner   = DEDIPY_ID_NONE;
    BUFFER->l        = BUFFER_L;
    BUFFER->r        = BUFFER_R;


    //BUFFER->futex1 = &BUFFER->a;
   //*BUFFER->futex1 = 1;        /* State: unavailable */
    //BUFFER->futex2 = &BUFFER->b;
   //*BUFFER->futex2 = 1;        /* State: available */

    log("CREATING FIRST FREE CHUNK OF SIZE %llu.", (uintll)sizeof(BUFFER->chunks));

    // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O C_SIZE_MAX E ROOTS_SIZES_LST SÃO MAIORES DO QUE ELE
    c_free_fill_and_register((chunk_s*)BUFFER->chunks, sizeof(BUFFER->chunks));

#if DEDIPY_TEST
    log("PRE-INITIAL VERIFY");

    dedipy_verify();

    log("INITIAL TESTS");

    dedipy_test();

    log("INITIAL VERIFY");

    dedipy_verify();
#endif

    log("RUNNING");
}

void dedipy_main_worker (void) {

    log("LIB MAIN");

    dedipy_init_signals();

    if (mmap((void*)DEDIPY_BUFFER_ADDR, DEDIPY_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_FIXED_NOREPLACE, DEDIPY_BUFFER_FD, 0) != (void*)DEDIPY_BUFFER_ADDR)
        fatal("FAILED TO MAP BUFFER");

    if (close(DEDIPY_BUFFER_FD))
        fatal("FAILED TO CLOSE BUFFER FD");

    if (BUFFER->version != DEDIPY_VERSION)
        fatal("VERSION MISMATCH");

    if (BUFFER->check != DEDIPY_CHECK)
        fatal("CHECK MISMATCH");

    if (BUFFER->size != DEDIPY_BUFFER_SIZE)
        fatal("BUFFER SIZE MISMATCH");

    if (BUFFER->workersN != WORKERS_N)
        fatal("WORKERS NUMBER MISMATCH");

    const pid_t pid = getpid();

    myID = DEDIPY_ID_NONE;

    for each_worker (worker)
        if (pid == (pid_t)worker->pid) {
            myID = (int)worker->id;
            break;
        }

    if (myID == DEDIPY_ID_NONE)
        fatal("COULT NOT SELF IDENTIFY");

    if ((int)SELF->cpu != sched_getcpu())
        fatal("CPU MISMATCH");

    char name[256];

    if (snprintf(name, sizeof(name), DEDIPY_PROGNAME "-%u-%u@%u", (uint)SELF->id, (uint)SELF->groupID, (uint)SELF->group->id) < 2 || prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0))
        fatal("FAILED TO SET PROCESS NAME");

#if DEDIPY_TEST
    dedipy_test();
#endif
}


// TODO: FIXME: o Python vai dar um overwrite nestes signal handlers :/
//      usar alguma coisa para capturar o handler anderior, e adicionar à funcao do nosso handler, caso old seja diferente dele
