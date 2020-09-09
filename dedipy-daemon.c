/*
    mount -t hugetlbfs /dev/hugepages /dev/hugepages -o pagesize=1G,min_size=16106127360
*/

#define _GNU_SOURCE 1

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <sched.h>
#include <errno.h>

#include "util.c"

#include "dedipy.c"

#ifndef DEDIPY_PYTHON_PATH
#define DEDIPY_PYTHON_PATH "/usr/bin/python"
#endif

#ifndef DAEMON_CPU
#define DAEMON_CPU 1 // A CPU 0 DEVE SER DEIXADA PARA O KERNEL, INTERRUPTS, E ADMIN
#endif

typedef struct worker_cfg_s { u32 group; u32 cpu; char** args; } worker_cfg_s;

// "PYTHONMALLOC=malloc",
#define WORKER_ARGS(...) (char*[]) { "python", ##__VA_ARGS__, NULL }

// TODO: FIXME: memory allocation;usage for NUMA systems

static pid_t sid;

static const worker_cfg_s workersCfg[WORKERS_N] = {
    // GROUP  CPU  PYTHON ARGUMENTS
    {  0,       2, WORKER_ARGS(DEDIPY_PROGNAME "-0", "F93850BE41375DB0", "0") },
    {  0,       3, WORKER_ARGS(DEDIPY_PROGNAME "-1", "F93850BE41375DB0", "1") },
    {  0,       4, WORKER_ARGS(DEDIPY_PROGNAME "-2", "F93850BE41375DB0", "2") },
    {  0,       5, WORKER_ARGS(DEDIPY_PROGNAME "-3", "F93850BE41375DB0", "3") },
    {  0,       6, WORKER_ARGS(DEDIPY_PROGNAME "-4", "F93850BE41375DB0", "4") },
    {  0,       7, WORKER_ARGS(DEDIPY_PROGNAME "-5", "F93850BE41375DB0", "5") },
    {  0,       8, WORKER_ARGS(DEDIPY_PROGNAME "-6", "F93850BE41375DB0", "6") },
    {  0,       9, WORKER_ARGS(DEDIPY_PROGNAME "-7", "F93850BE41375DB0", "7") },
    {  0,      10, WORKER_ARGS(DEDIPY_PROGNAME "-8", "F93850BE41375DB0", "8") },
    {  0,      11, WORKER_ARGS(DEDIPY_PROGNAME "-9", "F93850BE41375DB0", "9") },
    {  1,       0, WORKER_ARGS("xpi.py") },
};

static volatile sig_atomic_t sigTERM;
static volatile sig_atomic_t sigUSR1;
static volatile sig_atomic_t sigUSR2;
static volatile sig_atomic_t sigALRM;
static volatile sig_atomic_t sigCHLD;

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

static void launch_worker (const worker_s* const worker) {

    // CPU AFFINITY
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(worker->cpu, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        fatal("FAILED TO SET CPU AFFINITY");

    const pid_t pid = getpid();

    log("EXECUTING WORKER %u/%u GROUP %u/%u #%u ON CPU %u PID %llu",
        (uint)worker->id, WORKERS_N, (uint)worker->groupID, (uint)worker->group->count, (uint)worker->group->id, (uint)worker->cpu, (uintll)pid);

    // EXECUTE IT
    execve(DEDIPY_PYTHON_PATH, workersCfg[worker->id].args, (char*[]) { "PATH=/usr/local/bin:/bin:/usr/bin", NULL });

    //
    fatal("EXECVE FAILED");
}

// TODO: FIXME: TER CERTEZA DE QUE É BOOT TIME, PARA QUE JAMAIS TENHA OVERFLOW
static inline u64 getseconds (void) {

    struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &t))
        fatal("FAILED TO GET MONOTONIC TIME");

    return (u64)t.tv_sec;
}

// LAUNCH ALL PROCESSES STOPPED
static void launch_workers (void) {

    const u64 now = getseconds();

    for each_worker (worker) {
        if (worker->pid == 0) {
            // SÓ SE JÁ ESTIVER NA HORA
            if (worker->startAgain <= now) {
                worker->startAgain = now + 30;
                worker->started = now;
                const pid_t pid = fork();
                if (pid == 0)
                    launch_worker(worker);
                worker->pid = (u64)pid;
            }
        }
    }
}

static void init_workers (void) {

    log("BUFFER SIZE %llu", (uintll)DEDIPY_BUFFER_SIZE);

    // TODO: FIXME: INITIALIZE GROUPS

    log("INITIALIZING WORKERS");

    u64 cpus = 1ULL << DAEMON_CPU;
    u64 groups = 0;

    for (uint workerID = 0; workerID != WORKERS_N ; workerID++) {

        const worker_cfg_s* workerCfg = &workersCfg[workerID];

        worker_s* worker = &BUFFER->workers[workerID];

        worker->id = (u16)workerID;
        worker->cpu = (u16)workerCfg->cpu;
        worker->pid = 0;
        worker->started = 0;
        worker->startAgain = 0;
        worker->group = &BUFFER->groups[workerCfg->group];
        worker->groupID = worker->group->count++;
        worker->groupNext = worker->group->workers;
        worker->group->workers = worker;

        cpus   |= (1ULL << workerCfg->cpu);
        groups |= (1ULL << workerCfg->group);

        log("WORKER %u/%u GROUP %u/%u #%u CPU %u", worker->id, WORKERS_N, worker->groupID, worker->group->count, worker->group->id, worker->cpu);
    }

    // ONE, AND ONLY ONE PROCESS PER CPU
    if (popcount64(cpus) != (WORKERS_N + 1))
        fatal("INVALID CPU USAGE");
}

static void init_buffer (void) {

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

    myID = DEDIPY_ID_DAEMON;

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

static void init (void) {

    // SETUP LIMITS
    const struct rlimit limit = {
        .rlim_cur = FDS_N,
        .rlim_max = FDS_N,
        };

    if (setrlimit(RLIMIT_NOFILE, &limit))
        fatal("FAILED TO SET MAX OPEN FILES LIMIT");

#if 0
    // FIFO SCHEDULING
    struct sched_param params;

    memset(&params, 0, sizeof(params));

    params.sched_priority = 1;

    if (sched_setscheduler(0, SCHED_FIFO, &params) == -1)
        fatal("FAILED TO SET SCHEDULING");
#endif

    // CPU AFFINITY
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(DAEMON_CPU, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        fatal("FAILED TO SET CPU AFFINITY");

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

    init_buffer();
    init_workers();

    // INSTALA O TIMER
    const struct itimerval interval = {
        .it_interval.tv_sec = 30,
        .it_interval.tv_usec = 0,
        .it_value.tv_sec = 30,
        .it_value.tv_usec = 0,
        };

    if (setitimer(ITIMER_REAL, &interval, NULL))
        fatal("FAILED TO INSTALL TIMER");
}

static void handle_sig_child (void) {
    if (sigCHLD) {
        sigCHLD = 0;

        pid_t pid; int status;

        // 0 -> CHILDS BUT NONE EXITED
        while ((pid = waitpid(-1, &status, WNOHANG))) {

            if (pid == -1) {
                if (errno == EINTR)
                    continue;
                if (errno == ECHILD)
                    break; // NO MORE CHILDS
                fatal_group("WAIT FAILED");
            }

            // IDENTIFICA QUEM FOI E ESQUECE SEU PID
            for each_worker (worker) {
                if (worker->pid == (u64)pid) {
                    worker->pid = 0;
                    log("WORKER %u WITH PID %llu EXITED WITH STATUS %d AFTER %llu", worker->id, (uintll)pid, status, (uintll)(getseconds() - worker->started));
                }
            }
        }
    }
}

static void handle_sig_usr1 (void) {
    if (sigUSR1) {
        sigUSR1 = 0;

    }
}

static void handle_sig_usr2 (void) {
    if (sigUSR2) {
        sigUSR2 = 0;

    }
}

static void handle_sig_alrm (void) {
    if (sigALRM) {
        sigALRM = 0;

    }
}

static void main_loop (void) {

    while (sigTERM == 0) {

        handle_sig_alrm();
        handle_sig_usr1();
        handle_sig_usr2();
        handle_sig_child();

        launch_workers();

        // DO WHATEVER WE HAVE TO DO
        pause();
    }
}

// RETORNA: SE TEM QUE DAR WAIT
static int main_wait_workers (void) {

    loop { int status; const pid_t pid = wait(&status);

        if (pid == -1)
            return errno != ECHILD;

        for each_worker (worker) {
            if (worker->pid == (u64)pid) {
                worker->pid = 0;
                log("WORKER %u WITH PID %llu EXITED WITH STATUS %d AFTER %llu", worker->id, (uintll)pid, status, (uintll)(getseconds() - worker->started));
            }
        }
    }
}

int main (void) {

    if (prctl(PR_SET_NAME, (unsigned long)DEDIPY_PROGNAME, 0, 0, 0))
        fatal_group("FAILED TO SET PROCESS NAME");

    sid = setsid();

    init();

    // TODO: FIXME: CLOSE STDIN, AND REOPEN AS /DEV/NULL

    // TODO: FIXME: FORK?

    // TODO: FIXME: other limits

    // TODO: FIXME: setup /proc pipe limits
    // TODO: FIXME: setup /proc scheduling

    // TODO: FIXME: setup pipes sizes

    // TODO: FIXME: setup malloc.c for us to use in the daemon process -> called manually, not injected

    // THE PROCESS AND ALL ITS THREADS MUST RUN ON THE SAME CPU, IN FIFO SCHEDULING MODE
    // THIS MAKES EVERYTHING THREAD SAFE (BETWEEN TWO POINTS WITH NO SYSCALLS/BLOCKING OPERATIONS)
    // - WILL SIGNALS INTERFER IN CONTEXT SWITCHING?
    // - WILL STACK INCREASE / PAGE FAULTS INTERFER IN CONTEXT SWITCHING?

    // LANÇA TODOS OS PROCESSOS
    launch_workers();

    //WORKER_PUT(1, "EU SOU O BOZO\n", 14);

    //
    main_loop();

    // TODO: FIXME: wait() non blocking, e limpa todos

    // TELL ALL WORKERS TO TERMINATE
    // NOTE: VAI MANDAR PARA SI MESMO TAMBÉM
    log("SENDING SIGTERM TO ALL WORKERS");

    kill(0, SIGTERM);

    // WAIT ALL SUBPROCESSES TO TERMINATE
    // WE WILL TIMEOUT BECAUSE OF THE TIMER
    // SEND SIGALRM TO THE DAEMON TO EXIT FASTER

    if (main_wait_workers()) { // IF STILL NOT, DON'T WAIT ANYMORE: KILL THEM ALL
        if (main_wait_workers()) {
            // MAKE SURE NONE IS TOUCHING IT
            dedipy_alloc_acquire();
            for each_worker (worker) {
                if (worker->pid) {
                    log("KILLING WORKER %u WITH PID %llu", worker->id, (uintll)worker->pid);
                    kill((pid_t)worker->pid, SIGKILL);
                }
            }
            // TODO: FIXME: THIS ONE MUST BE NON BLOCKING
            if (main_wait_workers())
                fatal_group("KILLING PROCESS GROUP");
            // RESTORE IT
            dedipy_alloc_release();
        }
    }

#if DEDIPY_TEST
    log("PRE-FINAL VERIFY");

    dedipy_verify();

    log("FINAL TESTS");

    dedipy_test();

    log("FINAL VERIFY");

    dedipy_verify();
#endif

    // PARA SABER QUE NINGUEM MECHEU NO FD
    if (lseek(DEDIPY_BUFFER_FD, 0, SEEK_CUR) != DEDIPY_BUFFER_SIZE)
        fatal_group("BUFFER FD OFFSET WAS CHANGED");

    log("EXITING SUCESSFULLY");

    return 0;
}
