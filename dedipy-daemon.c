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

#include "util.h"

#define DEDIPY_MASTER 1

#include "dedipy-lib.h"

// "PYTHONMALLOC=malloc",
#define WORKER_ARGS(...) (char*[]) { "python", ##__VA_ARGS__, NULL }

static pid_t sid;

static const struct { u32 group; u32 cpu; char** args; } workersCfg[WORKERS_N] = {
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

static void launch_worker (const worker_s* const worker) {

    // CPU AFFINITY
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(worker->cpu, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        fatal("FAILED TO SET CPU AFFINITY");

    log("EXECUTING WORKER %u/%u GROUP %u/%u #%u ON CPU %u PID %llu",
        (uint)worker->id, WORKERS_N, (uint)worker->groupID, (uint)worker->group->count, (uint)worker->group->id, (uint)worker->cpu, (uintll)getpid());

    // EXECUTE IT
    execve(DEDIPY_PYTHON_PATH, workersCfg[worker->id].args, (char*[]) { "LD_PRELOAD=./dedipy-lib.so", "PATH=/usr/bin:/bin", NULL });

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

    // TODO: FIXME: INITIALIZE GROUPS

    log("INITIALIZING WORKERS");

    u64 cpus = 1ULL << DAEMON_CPU;
    u64 groups = 0;

    for (uint workerID = 0; workerID != WORKERS_N ; workerID++) {

        const typeof(&workersCfg[0]) workerCfg = &workersCfg[workerID];

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

static void init (void) {

    // SETUP LIMITS
    const struct rlimit limit = { .rlim_cur = FDS_N, .rlim_max = FDS_N };

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

    dedipy_main();

    init_workers();

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
    //if (lseek(DEDIPY_BUFFER_FD, 0, SEEK_CUR) != DEDIPY_BUFFER_SIZE)
        //fatal_group("BUFFER FD OFFSET WAS CHANGED");

    log("EXITING SUCESSFULLY");

    return 0;
}
