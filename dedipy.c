/*
    TODO: FIXME: cada worker um process group? :/

    mount -t hugetlbfs hugetlbfs hugetlbfs -o pagesize=1G,min_size=16106127360

    touch hugetlbfs/buffer

    echo $((16*512)) > /proc/sys/vm/nr_hugepages

    grep -i huge /proc/meminfo
*/

#define _GNU_SOURCE 1

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

#include "util.h"

#include "dedipy.h"

#ifndef DEDIPY_PYTHON_PATH
#define DEDIPY_PYTHON_PATH "/usr/bin/python"
#endif

//
#ifndef BUFF_PATH
#error ""
#endif

// OBS: BUFF_SIZE tem que ser ULL
#ifndef BUFF_SIZE
#error ""
#endif // --> TODO: FIXME: esta variável é usada pelo malloc() para o processo atual; deve usar outra para o total

#ifndef ALLOC_ALIGNMENT // PER PROCESS
#define ALLOC_ALIGNMENT 65536
#endif

#define BUFFER_MMAP_FLAGS (MAP_SHARED | MAP_FIXED | MAP_FIXED_NOREPLACE)

static uint workersCounter;

// WORKERS
typedef struct Worker Worker;

struct Worker {
    u8 id;
    u8 groupID; // IT'S ID INSIDE IT'S GROUP
    u8 groupN; // NUMBER OF PROCESSES IN THIS GROUP
    u8 cpu;
    u16 reserved;
    u16 code; // NAO PRECISA SER PALAVRA GRANDE; É SÓ PARA TORNAR MAIS ÚNICO, JUNTO COM O PID E LAUNCHED
    u64 pid;
    u32 again; // WHEN TO LAUNCH IT AGAIN, IN SECONDS
    u32 sizeWeight;
    u64 started; // TIME IT WAS LAUNCHED: IN RDTSC()
    u64 start;
    u64 size;
    char** args;
};

#define WORKERS_LMT (&workers[WORKERS_N])

// "PYTHONMALLOC=malloc",
#define WORKER_ARGS(...)   .args = (char*[]) { "python", ##__VA_ARGS__, NULL }
#define WORKER_GROUP_ID(_) .groupID = (_)
#define WORKER_GROUP_N(_)  .groupN = (_)
#define WORKER_CPU(_)      .cpu = (_)
#define WORKER_SIZE(_)     .size = (_##ULL) // MINIMAL
#define WORKER_SIZE_WEIGHT(_)  .sizeWeight = ((_##U) << 18) // IF SET, THE REMAINING WILL BE DIVIDED INTO THOSE, ON EACH GROUP

// (1 << 31) / (1 << 18) -> aguenta weights 8192x acima

// TODO: FIXME: se WORKER_SIZE_WEIGHT FOR FLOAT/DOUBLE, fazer (_)*4194304

// EASY AND TOTAL CONTROL OF THE WORKERS EXECUTION PARAMETERS
// A CPU 0 DEVE SER DEIXADA PARA O KERNEL, INTERRUPTS, E ADMIN
#define DAEMON_CPU 1
#define DAEMON_SIZE 0

#define WORKERS_N 10

#define WORKER_SIZE_X WORKER_SIZE(BUFF_SIZE - DAEMON_SIZE - WORKERS_N*65536)/WORKERS_N

// PROBLEMA: esta alocando por multiplos de paginas :/ entao nao dá para dividir minuciosamente as coisas
static Worker workers[WORKERS_N] = {
    { WORKER_ARGS(PROGNAME "-0", "F93850BE41375DB0", "0"), WORKER_GROUP_ID(0), WORKER_GROUP_N(10), WORKER_CPU( 2), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-1", "F93850BE41375DB0", "1"), WORKER_GROUP_ID(1), WORKER_GROUP_N(10), WORKER_CPU( 3), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(2) },
    { WORKER_ARGS(PROGNAME "-2", "F93850BE41375DB0", "2"), WORKER_GROUP_ID(2), WORKER_GROUP_N(10), WORKER_CPU( 4), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-3", "F93850BE41375DB0", "3"), WORKER_GROUP_ID(3), WORKER_GROUP_N(10), WORKER_CPU( 5), WORKER_SIZE(4688379904), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-4", "F93850BE41375DB0", "4"), WORKER_GROUP_ID(4), WORKER_GROUP_N(10), WORKER_CPU( 6), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-5", "F93850BE41375DB0", "5"), WORKER_GROUP_ID(5), WORKER_GROUP_N(10), WORKER_CPU( 7), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-6", "F93850BE41375DB0", "6"), WORKER_GROUP_ID(6), WORKER_GROUP_N(10), WORKER_CPU( 8), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-7", "F93850BE41375DB0", "7"), WORKER_GROUP_ID(7), WORKER_GROUP_N(10), WORKER_CPU( 9), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-8", "F93850BE41375DB0", "8"), WORKER_GROUP_ID(8), WORKER_GROUP_N(10), WORKER_CPU(10), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
    { WORKER_ARGS(PROGNAME "-9", "F93850BE41375DB0", "9"), WORKER_GROUP_ID(9), WORKER_GROUP_N(10), WORKER_CPU(11), WORKER_SIZE(0), WORKER_SIZE_WEIGHT(1) },
};

//
static volatile sig_atomic_t sigTERM = 0;
static volatile sig_atomic_t sigUSR1 = 0;
static volatile sig_atomic_t sigUSR2 = 0;
static volatile sig_atomic_t sigALRM = 0;
static volatile sig_atomic_t sigCHLD = 0;

static void signal_handler (const int signal, const siginfo_t* const restrict signalInfo, const void* const signalData) {

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

static void launch_worker (const Worker* const worker) {

    // CPU AFFINITY
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(worker->cpu, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        fatal("FAILED TO SET CPU AFFINITY");

    // ROOTS IS ALREADY INITIALIZED BY MEMSET

    // IF A PROCESS TRY TO RUN WITHOUT INITIALIZING, MALLOC WILL JUST FAIL, AS ALL FREE ROOTS ARE NULL

    //
    if (dup2(WORKER_GET_FD(worker->id), SELF_GET_FD) != SELF_GET_FD ||
        dup2(WORKER_PUT_FD(worker->id), SELF_PUT_FD) != SELF_PUT_FD)
        fatal("FAILED TO DUPLICATE WORKER -> SELF FDS");

    if (close(DAEMON_GET_FD))
        fatal("FAILED TO CLOSE DAEMON GET FD");

    const pid_t pid = getpid();

    char env0[512];

    snprintf(env0, sizeof(env0), "DEDIPY=" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX"  "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX",
        (uintll)worker->cpu, (uintll)pid, (uintll)BUFFER_FD, (uintll)BUFFER_MMAP_FLAGS, (uintll)BUFF_ADDR, (uintll)BUFF_SIZE, (uintll)worker->start, (uintll)worker->size, (uintll)worker->code, (uintll)worker->started, (uintll)worker->id, (uintll)WORKERS_N, (uintll)worker->groupID, (uintll)worker->groupN);

    dbg("EXECUTING WORKER %u/%u ON CPU %u START %llu SIZE %llu PID %llu", (uint)worker->id, (uint)WORKERS_N, (uint)worker->cpu, (uintll)worker->start, (uintll)worker->size, (uintll)pid);

    // EXECUTE IT
    execve(DEDIPY_PYTHON_PATH, worker->args, (char*[]) { env0, "PATH=/usr/local/bin:/bin:/usr/bin", NULL });

    //
    fatal("EXECVE FAILED");
}

// TODO: FIXME: TER CERTEZA DE QUE É BOOT TIME, PARA QUE JAMAIS TENHA OVERFLOW
static inline uint getseconds (void) {

    struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &t))
        fatal("FAILED TO GET MONOTONIC TIME");

    return t.tv_sec;
}

// LAUNCH ALL PROCESSES STOPPED
static void launch_workers (void) {

    const u64 now = getseconds();

    Worker* worker = workers;

    do { // TODO: FIXME: limpar os pipes antes de iniciar o processo?
        if (worker->pid == 0) {
            // SÓ SE JÁ ESTIVER NA HORA
            if (worker->again <= now) {
                worker->again = now + 30;
                worker->started = rdtsc();
                worker->code = workersCounter++;
                const pid_t pid = fork();
                if (pid == 0)
                    launch_worker(worker);
                worker->pid = pid;
            }
        }
    } while (++worker != WORKERS_LMT);
}

static void init_workers (void) {

    dbg("BUFFER SIZE %llu", (uintll)BUFF_SIZE);
    dbg("DAEMON SIZE %llu", (uintll)DAEMON_SIZE);

    //
    workersCounter = 0;

    uint workerID = 0;

    // TODO: FIXME: fazer por grupos
    // last = 0 // ultimo grupoID visto
    //   loopa de novo, aumentando, até que nao haja nenhum grupo ID > que este
    //     dai calcula o tamanho total do grupo atual
    // depois vai pegar a memória que sobra, e dividir ela igualmente entre os grupos
    // e dai, dividir a memória que sobrou para o grupom entre os membros dele
    { Worker* worker = workers; u64 weights = 0;
        // CALCULA A PROPORÇÃO DE CADA UM
        // SE O MÍNIMO DE UM É MAIOR DO QUE SUA PROPORÇÃO, USA ESTE MÍNIMO/DISPONIVEL
        // SOMA O TOTAL DOS PESOS
        while (worker != WORKERS_LMT)
            weights += (worker++)->sizeWeight;
        // PEGA O QUE TEM DISPONÍVEL
        u64 available = BUFF_SIZE - DAEMON_SIZE - WORKERS_N*ALLOC_ALIGNMENT;
        // DISTRIBUI ESTE DISPONÍVEL
        while (worker-- != workers) {
            if ((((u64)worker->sizeWeight * (u64)available) / (u64)weights) <= worker->size) {
                available -= worker->size; // ESTE PRECISA DE MAIS DO QUE O PROPORCIONAL, ENTAO RESERVA ELE NO TOTAL
                worker->sizeWeight = 0; // NAO USAR O PESO
            }
        } // RECALCULAR OS PESOS
        weights = 0;
        while (++worker != WORKERS_LMT)
            weights += worker->sizeWeight;
        // DIVIDE O RESTANTE ENTRE OS QUE VÃO USAR O WEIGHT
        // OS DEMAIS JA TEM SEU TAMANHO TOTAL, QUE JÁ É >= SEU MÍNIMO
        // O QUANTO CADA UM REPRESENTA DENTRO DESSE TOTAL
        while (worker-- != workers)
            if (worker->sizeWeight)
                worker->size = ((u64)worker->sizeWeight * (u64)available) / (u64)weights;
    }

    Worker* worker = workers;

    u64 offset = DAEMON_SIZE;

    u64 cpus = 1ULL << DAEMON_CPU;

    do {
        worker->id = workerID++;
        worker->pid = 0;
        worker->code = 0;
        worker->again = 0;
        worker->started = 0;
        worker->start = offset;
        worker->size += ALLOC_ALIGNMENT - 1; // ALINHA PARA CIMA
        worker->size /= ALLOC_ALIGNMENT;
        worker->size *= ALLOC_ALIGNMENT;
        offset += worker->size;
        cpus |= (1ULL << worker->cpu);
        dbg("WORKER %u START %llu SIZE %llu", worker->id, (uintll)worker->start, (uintll)worker->size);
    } while (++worker != WORKERS_LMT);

    dbg("TOTAL USED %llu; UNUSED %llu", (uintll)offset, (uintll)(BUFF_SIZE - offset));

    // MAKE SURE EVERYTHING HAS FIT
    if (offset > BUFF_SIZE)
        fatal("INSUFFICIENT BUFFER");

    // ONE, AND ONLY ONE PROCESS PER CPU
    if (__builtin_popcountll(cpus) != (WORKERS_N + 1))
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
            dup2(fds[0], DAEMON_GET_FD) != DAEMON_GET_FD || close(fds[0]) ||
            dup2(fds[1], DAEMON_PUT_FD) != DAEMON_PUT_FD || close(fds[1])
        ) fatal("");
    }

    { //
        uint workerID = WORKERS_N;

        while (workerID--) { int fds[2] = { -1, -1 };
            if (pipe2(fds, O_DIRECT) ||
              dup2(fds[0], WORKER_GET_FD(workerID)) != WORKER_GET_FD(workerID) || close(fds[0]) ||
              dup2(fds[1], WORKER_PUT_FD(workerID)) != WORKER_PUT_FD(workerID) || close(fds[1])
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
    //
    const int fd = open(BUFF_PATH, O_RDWR | O_CREAT);
    // MAP_LOCKED vs MAP_NORESERVE
    // TAMANHO TEM QUE SER MULTIPLO DO PAGE SIZE A SER USADO
    // MAP_POPULATE | MAP_LOCKED | MAP_FIXED_NOREPLACE

    if (fd == -1)
        fatal("FAILED TO OPEN BUFFER");
    if (dup2(fd, BUFFER_FD) != BUFFER_FD)
        fatal("");
    if (close(fd))
        fatal("");

#if 0
    if (ftruncate(BUFFER_FD, BUFF_SIZE))
        fatal("FTRUNCATE FAILED");
#endif

    // TODO: FIXME: TER CERTEZA QUE CARREGOU TOOS OS HOLES :S
    //  | MAP_HUGETLB | MAP_HUGE_1GB
    if (mmap(BUFF_ADDR, BUFF_SIZE, PROT_READ | PROT_WRITE, BUFFER_MMAP_FLAGS, BUFFER_FD, 0) != BUFF_ADDR)
        fatal("FAILED TO MAP BUFFER");

    // ter certeza de que tem este tamanho
    // AO MESMO TEMPO, PREVINE QUE WRITE() NESTE FD ESCREVE SOBRE A MEMÓRIA
    // TODO: FIXME: isso adianta, ou precisa testar um write()? :/
    if (lseek(BUFFER_FD, BUFF_SIZE, SEEK_SET) != BUFF_SIZE)
        fatal("FAILED TO SEEK BUFFER FD");
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
    CPU_SET(DAEMON_CPU, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        fatal("FAILED TO SET CPU AFFINITY");
}

static void init_signals (void) {

    // INSTALL THE SIGNAL HANDLER
    struct sigaction action;

    memset(&action, 0, sizeof(action));

    action.sa_sigaction = (void*)signal_handler; // TODO: FIXME: correct cast
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
                fatal_group("WAIT FAILED");
            }

            // IDENTIFICA QUEM FOI E ESQUECE SEU PID
            Worker* worker = workers;

            do {
                if (worker->pid == pid) {
                    worker->pid = 0;
                    dbg("WORKER %u WITH PID %llu DIED", worker->id, (uintll)pid);
                }
            } while (++worker != WORKERS_LMT);
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
static inline int main_wait_workers (void) {

    loop {
        const pid_t pid = wait(NULL);

        if (pid == -1)
            return errno != ECHILD;

        // TODO: FIXME: identifica qual foi e limpa ele
    }
}

int main (void) {

    if (prctl(PR_SET_NAME, (unsigned long)PROGNAME, 0, 0, 0))
        fatal_group("FAILED TO SET PROCESS NAME");

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

    // TODO: FIXME: setup malloc.c for us to use in the daemon process -> called manually, not injected

    // THE PROCESS AND ALL ITS THREADS MUST RUN ON THE SAME CPU, IN FIFO SCHEDULING MODE
    // THIS MAKES EVERYTHING THREAD SAFE (BETWEEN TWO POINTS WITH NO SYSCALLS/BLOCKING OPERATIONS)
    // - WILL SIGNALS INTERFER IN CONTEXT SWITCHING?
    // - WILL STACK INCREASE / PAGE FAULTS INTERFER IN CONTEXT SWITCHING?

    //
    init_workers();
    init_buffer();
    init_timer();

    // LANÇA TODOS OS PROCESSOS
    launch_workers();

    //WORKER_PUT(1, "EU SOU O BOZO\n", 14);

    //
    main_loop();

    // PARA SABER QUE NINGUEM MECHEU NO FD
    if (lseek(BUFFER_FD, 0, SEEK_CUR) != BUFF_SIZE)
        fatal_group("BUFFER FD OFFSET WAS CHANGED");

    // TODO: FIXME: wait() non blocking, e limpa todos

    // TELL ALL WORKERS TO TERMINATE
    // NOTE: VAI MANDAR PARA SI MESMO TAMBÉM
    dbg("SENDING SIGTERM TO ALL WORKERS");

    kill(0, SIGTERM);

    // WAIT ALL SUBPROCESSES TO TERMINATE
    // WE WILL TIMEOUT BECAUSE OF THE TIMER
    // SEND SIGALRM TO THE DAEMON TO EXIT FASTER

    if (main_wait_workers()) { // IF STILL NOT, DON'T WAIT ANYMORE: KILL THEM ALL
        if (main_wait_workers()) { Worker* worker = workers;
            do {
                if (worker->pid) {
                    dbg("KILLING WORKER %u WITH PID %llu", worker->id, (uintll)worker->pid);
                    kill(worker->pid, SIGKILL);
                }
            } while (++worker != WORKERS_LMT);
            // TODO: FIXME: THIS ONE MUST BE NON BLOCKING
            if (main_wait_workers())
                fatal_group("KILLING PROCESS GROUP");
        }
    }

    dbg("EXITING SUCESSFULLY");

    return 0;
}

// COMO LIMPAR UM PIPE:
// é modo packet
// write("FIM:COOKIE")
//  while(read() != "FIM:COOKIE");
// =]



// TODO: FIXME: for NUMA systems, it's better to test CPU<->memory regions, on start
// then we only set first cpu0 for the first worker
