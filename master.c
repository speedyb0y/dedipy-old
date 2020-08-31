/*
    TODO: FIXME: cada slave um process group? :/
*/

#define _GNU_SOURCE 1
#define _LARGEFILE64_SOURCE 64
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stddef.h>
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

//
#define abort() kill(0, SIGKILL)

#include "util.h"

#include "ms.h"

//
#ifndef BUFF_PATH
#error ""
#endif

// OBS: BUFF_SIZE tem que ser ULL
#ifndef BUFF_SIZE
#error ""
#endif // --> TODO: FIXME: esta variável é usada pelo malloc() para o processo atual; deve usar outra para o total

#ifndef LIB_MS_PATH
#define LIB_MS_PATH "libms.so"
#endif

// SLAVES
typedef struct Slave Slave;

struct Slave {
    u8 id;
    u8 groupID; // IT'S ID INSIDE IT'S GROUP
    u8 groupN; // NUMBER OF PROCESSES IN THIS GROUP
    u8 cpu;
    u16 reserved;
    u16 code; // NAO PRECISA SER PALAVRA GRANDE; É SÓ PARA TORNAR MAIS ÚNICO, JUNTO COM O PID E LAUNCHED
    u64 pid;
    u64 started; // TIME IT WAS LAUNCHED, TODO: FIXME: IN RDTSC()
    u64 start;
    u64 size;
    char* path;
    char** args;
    char** env;
};

#define SLAVES_LMT (&slaves[SLAVES_N])

#define SLAVE_PATH(_)     .path = (_)
#define SLAVE_ARGS(...)   .args = (char*[]){ __VA_ARGS__, NULL }
#define SLAVE_ENV(...)    .env = (char*[]) { NULL, "LD_PRELOAD=" LIB_MS_PATH, "PATH=/usr/local/bin:/bin:/usr/bin", ##__VA_ARGS__, NULL }
#define SLAVE_GROUP_ID(_) .groupID = (_)
#define SLAVE_GROUP_N(_)  .groupN = (_)
#define SLAVE_CPU(_)      .cpu = (_)
#define SLAVE_SIZE(_)     .size = (_##ULL)

// EASY AND TOTAL CONTROL OF THE SLAVES EXECUTION PARAMETERS
#if 0
#define MASTER_CPU 1
#define MASTER_SIZE 65536

#define SLAVES_N 4

static Slave slaves[SLAVES_N] = {
    //{ SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU(2), SLAVE_SIZE(768*1024*1024), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave", "0"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU(2), SLAVE_SIZE(BUFF_SIZE/5), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave", "1"), SLAVE_GROUP_ID(1), SLAVE_GROUP_N(6), SLAVE_CPU(3), SLAVE_SIZE(BUFF_SIZE/5), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave", "2"), SLAVE_GROUP_ID(2), SLAVE_GROUP_N(6), SLAVE_CPU(4), SLAVE_SIZE(BUFF_SIZE/5), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave", "3"), SLAVE_GROUP_ID(3), SLAVE_GROUP_N(6), SLAVE_CPU(5), SLAVE_SIZE(BUFF_SIZE/5), SLAVE_ENV() },
};
#else
// A CPU 0 DEVE SER DEIXADA PARA O KERNEL, INTERRUPTS, E ADMIN
#define MASTER_CPU 1
#define MASTER_SIZE 0

#define SLAVES_N 10

static Slave slaves[SLAVES_N] = {
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-0", "F93850BE41375DB0", "0"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 2), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-1", "F93850BE41375DB0", "1"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 3), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-2", "F93850BE41375DB0", "2"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 4), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-3", "F93850BE41375DB0", "3"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 5), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-4", "F93850BE41375DB0", "4"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 6), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-5", "F93850BE41375DB0", "5"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 7), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-6", "F93850BE41375DB0", "6"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 8), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-7", "F93850BE41375DB0", "7"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU( 9), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-8", "F93850BE41375DB0", "8"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU(10), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
    { SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python", "slave-9", "F93850BE41375DB0", "9"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU(11), SLAVE_SIZE((BUFF_SIZE - SLAVES_N*65536)/10), SLAVE_ENV() },
};
#endif

static uint slavesCounter;

//
static volatile sig_atomic_t sigTERM = 0;
static volatile sig_atomic_t sigUSR1 = 0;
static volatile sig_atomic_t sigUSR2 = 0;
static volatile sig_atomic_t sigALRM = 0;
static volatile sig_atomic_t sigCHLD = 0;

static void signal_handler (int signal, siginfo_t* signalInfo, void* signalData) {

    (void)signalInfo;
    (void)signalData;

    switch (signal) {
        case SIGTERM:
        case SIGINT:
            sigTERM = 1;
            break;
        case SIGUSR1:
            sigUSR1 = 1;
            break;
        case SIGUSR2:
            sigUSR2 = 1;
            break;
        case SIGALRM:
            sigALRM = 1;
            break;
        case SIGCHLD:
            sigCHLD = 1;
            break;
    }
}

static pid_t sid;

static void launch_slave (const Slave* const slave) {

    // CPU AFFINITY
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(slave->cpu, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        fatal("FAILED TO SET CPU AFFINITY");

    //
    void* const buff = BUFFER + slave->start;

    //
    memset(buff, 0, slave->size);

    // INFO
    ((BufferInfo*)buff)->initialized = 0;
    ((BufferInfo*)buff)->id       = slave->id;
    ((BufferInfo*)buff)->n        = SLAVES_N;
    ((BufferInfo*)buff)->groupID  = slave->groupID;
    ((BufferInfo*)buff)->groupN   = slave->groupN;
    ((BufferInfo*)buff)->cpu      = slave->cpu;
    ((BufferInfo*)buff)->code     = slave->code;
    ((BufferInfo*)buff)->pid      = getpid();
    ((BufferInfo*)buff)->started  = slave->started;
    ((BufferInfo*)buff)->start    = slave->start;
    ((BufferInfo*)buff)->size     = slave->size;

    // ROOTS IS ALREADY INITIALIZED BY MEMSET

    // IF A PROCESS TRY TO RUN WITHOUT INITIALIZING, MALLOC WILL JUST FAIL, AS ALL FREE ROOTS ARE NULL

    //
    if (dup2(SLAVE_GET_FD(slave->id), SELF_GET_FD) != SELF_GET_FD ||
        dup2(SLAVE_PUT_FD(slave->id), SELF_PUT_FD) != SELF_PUT_FD)
        fatal("FAILED TO DUPLICATE SLAVE -> SELF FDS");

    if (close(MASTER_GET_FD))
        fatal("FAILED TO CLOSE MASTER GET FD");

    //
    char var[512];

    snprintf(var, sizeof(var), "SLAVEPARAMS=" "%016llX" "%016llX" "%016llX" "%016llX", (uintll)slave->start, (uintll)slave->size, (uintll)slave->started, (uintll)slave->code);

    slave->env[0] = var;

    DBGPRINTF("EXECUTING SLAVE %u ON CPU %u PID %llu START %llu SIZE %llu",
        (uint)slave->id, (uint)slave->cpu, (uintll)((BufferInfo*)buff)->pid, (uintll)slave->start, (uintll)slave->size);

    // EXECUTE IT
    execve(slave->path, slave->args, slave->env);

    //
    fatal("EXECVE FAILED");
}

//
static inline u64 getseconds (void) {
    struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
    if (clock_gettime(CLOCK_MONOTONIC, &t))
        fatal("FAILED TO GET MONOTONIC TIME");
    return t.tv_sec;
}

// LAUNCH ALL PROCESSES STOPPED
static void launch_slaves (void) {

    const u64 now = getseconds();

    Slave* slave = slaves;

    do { // TODO: FIXME: limpar os pipes antes de iniciar o processo?
        if (slave->pid == 0) {
            // SÓ SE JÁ ESTIVER NA HORA
            if ((slave->started + 30) <= now) {
                slave->started = now;
                slave->code = slavesCounter++;
                const pid_t pid = fork();
                if (pid == 0)
                    launch_slave(slave);
                slave->pid = pid;
            }
        }
    } while (++slave != SLAVES_LMT);
}

static void init_slaves (void) {

    //
    slavesCounter = 0;

    uint slaveID = 0;

    Slave* slave = slaves;

    u64 slaveStart = MASTER_SIZE;

    u64 cpus = 1ULL << MASTER_CPU;

    DBGPRINTF("MASTER SIZE %llu", (uintll)MASTER_SIZE);

    do {
        slave->id = slaveID++;
        slave->pid = 0;
        slave->code = 0;
        slave->started = 0;
        slave->start = slaveStart;
        slave->size += 65535; // ALINHA PARA CIMA
        slave->size /= 65536;
        slave->size *= 65536;
        slaveStart += slave->size;
        cpus |= (1ULL << slave->cpu);
        DBGPRINTF("SLAVE %u START %llu SIZE %llu", slave->id, (uintll)slave->start, (uintll)slave->size);
    } while (++slave != SLAVES_LMT);

    DBGPRINTF("TOTAL USED %llu", (uintll)slaveStart);

    // MAKE SURE EVERYTHING HAS FIT
    if (slaveStart > BUFF_SIZE)
        fatal("INSUFFICIENT BUFFER");

    // ONE, AND ONLY ONE PROCESS PER CPU
    if (__builtin_popcountll(cpus) != (SLAVES_N + 1))
        fatal("INVALID CPU USAGE");

    //
    { int fds[2] = { -1, -1 };
        if (pipe2(fds, O_DIRECT) ||
            dup2(fds[0], ANY_GET_FD) != ANY_GET_FD || close(fds[0]) ||
            dup2(fds[1], ANY_PUT_FD) != ANY_PUT_FD || close(fds[1])
        ) fatal("");
    }

    { int fds[2] = { -1, -1 };
        if (pipe2(fds, O_DIRECT) ||
            dup2(fds[0], MASTER_GET_FD) != MASTER_GET_FD || close(fds[0]) ||
            dup2(fds[1], MASTER_PUT_FD) != MASTER_PUT_FD || close(fds[1])
        ) fatal("");
    }

    { //
        uint slaveID = SLAVES_N;

        while (slaveID--) { int fds[2] = { -1, -1 };
            if (pipe2(fds, O_DIRECT) ||
              dup2(fds[0], SLAVE_GET_FD(slaveID)) != SLAVE_GET_FD(slaveID) || close(fds[0]) ||
              dup2(fds[1], SLAVE_PUT_FD(slaveID)) != SLAVE_PUT_FD(slaveID) || close(fds[1])
            ) fatal("");
        }
    }
}

static void init_timer (void) {

    // INSTALA O TIMER
    const struct itimerval interval = {
        .it_interval.tv_sec = 15,
        .it_interval.tv_usec = 0,
        .it_value.tv_sec = 15,
        .it_value.tv_usec = 0,
        };

    if (setitimer(ITIMER_REAL, &interval, NULL))
        fatal("");
}

static void init_buffer (void) {

    // OPEN THE BUFFER FILE
    // O_DIRECT
    const int fd = open(BUFF_PATH, O_RDWR | O_NOATIME | O_NOCTTY);

    if (fd == -1)
        fatal("");
    if (dup2(fd, BUFFER_FD) != BUFFER_FD)
        fatal("");
    if (close(fd))
        fatal("");

    // ter certeza de que tem este tamanho
    // AO MESMO TEMPO, PREVINE QUE WRITE() NESTE FD ESCREVE SOBRE A MEMÓRIA
    // TODO: FIXME: isso adianta, ou precisa testar um write()? :/
    if (lseek(BUFFER_FD, BUFF_SIZE, SEEK_SET) != BUFF_SIZE)
        fatal("");

    // MAP_LOCKED vs MAP_NORESERVE
    // TAMANHO TEM QUE SER MULTIPLO DO PAGE SIZE A SER USADO
    if (mmap(BUFFER, BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_LOCKED | MAP_POPULATE, BUFFER_FD, 0) != BUFFER)
        fatal("");
}

static void init_limits (void) {

    // SETUP LIMITS
    const struct rlimit limit = {
        .rlim_cur = FD_MAX,
        .rlim_max = FD_MAX,
        };

    if (setrlimit(RLIMIT_NOFILE, &limit))
        fatal("");
}

static void init_sched (void) {

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
    CPU_SET(MASTER_CPU, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        fatal("FAILED TO SET CPU AFFINITY");
}

static void init_signals (void) {

    // INSTALL THE SIGNAL HANDLER
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_sigaction = signal_handler;
    action.sa_flags = SA_SIGINFO;

    if (sigaction(SIGTERM,   &action, NULL) ||
        sigaction(SIGUSR1,   &action, NULL) ||
        sigaction(SIGUSR2,   &action, NULL) ||
        sigaction(SIGIO,     &action, NULL) ||
        sigaction(SIGURG,    &action, NULL) ||
        sigaction(SIGPIPE,   &action, NULL) ||
        sigaction(SIGHUP,    &action, NULL) ||
        sigaction(SIGQUIT,   &action, NULL) ||
        sigaction(SIGINT,    &action, NULL) ||
        sigaction(SIGCHLD,   &action, NULL) ||
        sigaction(SIGALRM,   &action, NULL)
      ) fatal("FAILED TO INSTALL SIGNAL HANDLER");
}

static void handle_sig_child (void) {
    if (sigCHLD) {
        sigCHLD = 0;

        pid_t pid;

        // 0 -> CHILDS BUT NONE EXITED
        while ((pid = waitpid(-1, NULL, WNOHANG))) {

            if (pid == -1) {
                if (errno == EINTR)
                    continue;
                if (errno == ECHILD)
                    break; // NO MORE CHILDS
                fatal("WAIT FAILED");
            }

            // IDENTIFICA QUEM FOI E ESQUECE SEU PID
            Slave* slave = slaves;

            do {
                if (slave->pid == pid)
                    slave->pid = 0;
            } while (++slave != SLAVES_LMT);
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

        launch_slaves();

        // DO WHATEVER WE HAVE TO DO
        pause();
    }
}

// RETORNA: SE TEM QUE DAR WAIT
static inline int main_wait_slaves (void) {

    loop {
        const pid_t pid = wait(NULL);

        if (pid == -1)
            return errno != ECHILD;

        // TODO: FIXME: identifica qual foi e limpa ele
    }
}

int main (void) {

    if (prctl(PR_SET_NAME, (unsigned long)PROGNAME, 0, 0, 0))
        fatal("FAILED TO SET PROCESS NAME");

    sid = setsid();

    init_signals();
    init_sched();
    init_limits();

    // TODO: FIXME: CLOSE STDIN, AND REOPEN AS /DEV/NULL

    // TODO: FIXME: FORK?

    // TODO: FIXME: other limits

    // TODO: FIXME: setup /proc pipe limits
    // TODO: FIXME: setup /proc scheduling

    // TODO: FIXME: setup pipes sizes

    // TODO: FIXME: setup malloc.c for us to use in the master process -> called manually, not injected

    // THE PROCESS AND ALL ITS THREADS MUST RUN ON THE SAME CPU, IN FIFO SCHEDULING MODE
    // THIS MAKES EVERYTHING THREAD SAFE (BETWEEN TWO POINTS WITH NO SYSCALLS/BLOCKING OPERATIONS)
    // - WILL SIGNALS INTERFER IN CONTEXT SWITCHING?
    // - WILL STACK INCREASE / PAGE FAULTS INTERFER IN CONTEXT SWITCHING?

    //
    init_slaves();
    init_buffer();
    init_timer();

    // LANÇA TODOS OS PROCESSOS
    launch_slaves();

    //SLAVE_PUT(1, "EU SOU O BOZO\n", 14);

    //
    main_loop();

    // PARA SABER QUE NINGUEM MECHEU NO FD
    if (lseek(BUFFER_FD, 0, SEEK_CUR) != BUFF_SIZE)
        fatal("BUFFER FD OFFSET WAS CHANGED");

    // TODO: FIXME: wait() non blocking, e limpa todos

    // TELL ALL SLAVES TO TERMINATE
    // NOTE: VAI MANDAR PARA SI MESMO TAMBÉM
    DBGPRINTF("SENDING SIGTERM TO ALL SLAVES");

    kill(0, SIGTERM);

    // WAIT ALL SUBPROCESSES TO TERMINATE
    // WE WILL TIMEOUT BECAUSE OF THE TIMER
    // SEND SIGALRM TO THE MASTER TO EXIT FASTER

    if (main_wait_slaves()) { // IF STILL NOT, DON'T WAIT ANYMORE: KILL THEM ALL
        if (main_wait_slaves()) { Slave* slave = slaves;
            do {
                if (slave->pid) {
                    DBGPRINTF("KILLING SLAVE %u WITH PID %llu", slave->id, (uintll)slave->pid);
                    kill(slave->pid, SIGKILL);
                }
            } while (++slave != SLAVES_LMT);
            // TODO: FIXME: THIS ONE MUST BE NON BLOCKING
            // WILL KILL OURSELVES TOO; BUT AT LEAST WILL HAVE A NICE EXIT STATUS - KILLED
            if (main_wait_slaves()) {
                DBGPRINTF("KILLING PROCESS GROUP");
                kill(0, SIGKILL);
            }
        }
    }

    DBGPRINTF("EXITING SUCESSFULLY");

    return 0;
}

// COMO LIMPAR UM PIPE:
// é modo packet
// write("FIM:COOKIE")
//  while(read() != "FIM:COOKIE");
// =]



// TODO: FIXME: for NUMA systems, it's better to test CPU<->memory regions, on start
// then we only set first cpu0 for the first slave
