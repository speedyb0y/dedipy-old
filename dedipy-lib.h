/*

*/

#ifndef DEDIPY_MASTER
#define DEDIPY_MASTER 0
#endif

#include "dedipy-config.c"

#ifndef DEDIPY_PYTHON_PATH
#define DEDIPY_PYTHON_PATH "/usr/bin/python"
#endif

#ifndef DAEMON_CPU
#define DAEMON_CPU 1 // A CPU 0 DEVE SER DEIXADA PARA O KERNEL, INTERRUPTS, E ADMIN
#endif

#include "dedipy-gen.c"

#if!(defined DEDIPY_BUFFER_PATH && defined DEDIPY_BUFFER_SIZE && defined DEDIPY_DEBUG && defined DEDIPY_ASSERT && defined DEDIPY_TEST)
#error
#endif

#define FD_MAX 65535
#define FDS_N  65536

#define WORKERS_N 11
#define GROUPS_N 16

#define DEDIPY_ID_DAEMON WORKERS_N
#define DEDIPY_ID_NONE (WORKERS_N + 1)

#define BUFFER_L 0ULL // USED / ASSERT BY SIZE MUST FAIL / C_LEFT()
#define BUFFER_R 0ULL

#define TOPS0_N ((ROOTS_N/64)/64)
#define TOPS1_N (ROOTS_N/64)

#define CHUNK_ALIGNMENT 8ULL
#define DATA_ALIGNMENT 8ULL

#define each_worker(v) (worker_s* v = BUFFER->workers; v != &BUFFER->workers[WORKERS_N]; v++)

#define DEDIPY_BUFFER_ADDR 0x900000000ULL

#define BUFFER ((buffer_s*)DEDIPY_BUFFER_ADDR)

typedef struct worker_s worker_s;
typedef struct group_s group_s;
typedef struct buffer_s buffer_s;

typedef u64 chunk_size_t;

typedef struct chunk_s chunk_s;

struct group_s {
    u16 id;
    u16 count;
    u32 reserved;
    worker_s* workers;
};

struct worker_s {
    u16 id;
    u16 groupID; // ID DELE DENTRO DE SEU GRUPO
    u16 cpu;
    u16 reserved;
    u64 pid;
    u64 started; // TIME IT WAS LAUNCHED: IN RDTSC()
    u64 startAgain; // WHEN TO LAUNCH IT AGAIN, IN MONOTONIC TIME
    group_s* group;
    worker_s* groupNext;
};

struct buffer_s {
    volatile int aOwner; // ALLOCATION OWNER
    volatile int iOwner; // I/O OWNER
    group_s groups[GROUPS_N];
    worker_s workers[WORKERS_N];
    u64 tops0[TOPS0_N]; // BITMASK OF NON EMPTY ENTRIES IN TOPS1
    u64 tops1[TOPS1_N]; // BITMASK OF NON EMPTY ENTRIES IN ROOTS
    chunk_s* roots[ROOTS_N]; // THE POINTERS TO THE FREE CHUNKS
    chunk_size_t l;
    char chunks[DEDIPY_BUFFER_SIZE - 2*sizeof(int) - 0*sizeof(int*) - WORKERS_N*sizeof(worker_s) - GROUPS_N*sizeof(group_s) - ROOTS_N*sizeof(chunk_s*) - (TOPS0_N + TOPS1_N + 4)*sizeof(u64) - 2*sizeof(chunk_size_t)];
    chunk_size_t r;
    u64 workersN;
    u64 size;
    u64 version;
    u64 check;
};

#define SELF (&BUFFER->workers[myID])

#define GROUP (BUFFER->group)

#if 0
extern void* calloc (size_t n, size_t size_);
extern void free (void* const data);
extern void* malloc (size_t size_);
extern void* realloc (void* const data_, const size_t dataSizeNew_);
extern void* reallocarray (void *ptr, size_t nmemb, size_t size);
#endif

#if DEDIPY_MASTER
extern void dedipy_main_daemon (void);
#define dedipy_main dedipy_main_daemon
#else
extern void dedipy_main_worker (void);
#define dedipy_main dedipy_main_worker
#endif

#if DEDIPY_MASTER
extern void dedipy_verify (void);
#endif

extern void dedipy_test (void);

extern volatile sig_atomic_t sigTERM;
extern volatile sig_atomic_t sigUSR1;
extern volatile sig_atomic_t sigUSR2;
extern volatile sig_atomic_t sigALRM;
extern volatile sig_atomic_t sigCHLD;

#if !DEDIPY_MASTER
extern uint myID;
#endif

#if DEDIPY_MASTER
#define log(fmt, ...) ({ char b[4096]; write(STDERR_FILENO, b, (size_t)snprintf(b, sizeof(b), "MASTER " fmt "\n", ##__VA_ARGS__)); })
#else
#define log(fmt, ...) ({ char b[4096]; write(STDERR_FILENO, b, (size_t)snprintf(b, sizeof(b), "WORKER [%u] " fmt "\n", myID, ##__VA_ARGS__)); })
#endif

#if DEDIPY_DEBUG
#define dbg log
#else
#define dbg(fmt, ...) ({ })
#endif

#if DEDIPY_DEBUG >= 2
#define dbg2 dbg
#else
#define dbg2(fmt, ...) ({ })
#endif

#if DEDIPY_DEBUG >= 3
#define dbg3 dbg
#else
#define dbg3(fmt, ...) ({ })
#endif

#define fatal(reason) ({ log("FATAL: " reason); abort(); })

#if DEDIPY_TEST
#undef DEDIPY_ASSERT
#define DEDIPY_ASSERT 1
#endif

#if DEDIPY_ASSERT
#define dedipy_assert(c) assert(c)
#else
#define dedipy_assert(c) ({ })
#endif

static inline void dedipy_alloc_acquire (void) {

    const int myID = sched_getcpu();

    int noID = 0;

    while (!__atomic_compare_exchange_n(&BUFFER->aOwner, &noID, myID, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        noID = 0;
        __atomic_thread_fence(__ATOMIC_SEQ_CST); // TODO: FIXME: SOME BUSY LOOP :/
    }
}

static inline void dedipy_alloc_release (void) {
    __atomic_store_n(&BUFFER->aOwner, DEDIPY_ID_NONE, __ATOMIC_SEQ_CST);
}
