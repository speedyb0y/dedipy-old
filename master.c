/*

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

#include "util.h"

#include "ms.h"

//
#ifndef BUFF_PATH
#error ""
#endif

#ifndef BUFF_SIZE
#error ""
#endif // --> TODO: FIXME: esta variável é usada pelo malloc() para o processo atual; deve usar outra para o total

// A CPU 0 DEVE SER DEIXADA PARA O KERNEL, INTERRUPTS, E ADMIN
#define MASTER_CPU 1
#define MASTER_SIZE 65536

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
    u64 started; // TIME IT WAS LAUNCHED, IN RDTSC()
    u64 start;
    u64 size;
    char* path;
    char** args;
    char** env;
};

#define SLAVES_N 4

#define SLAVES_LMT (&slaves[SLAVES_N])

#define SLAVE_PATH(_)     .path = (_)
#define SLAVE_ARGS(...)   .args = (char*[]){ __VA_ARGS__, NULL }
#define SLAVE_ENV(...)    .env = (char*[]) { NULL, "LD_PRELOAD=" LIB_MALLOC_PATH, "TERM=linux", "PATH=/usr/local/bin:/bin:/usr/bin", ##__VA_ARGS__, NULL }
#define SLAVE_GROUP_ID(_) .groupID = (_)
#define SLAVE_GROUP_N(_)  .groupN = (_)
#define SLAVE_CPU(_)      .cpu = (_)
#define SLAVE_SIZE(_)     .size = (_)

// EASY AND TOTAL CONTROL OF THE SLAVES EXECUTION PARAMETERS
static Slave slaves[SLAVES_N] = {
    //{ SLAVE_PATH("/usr/bin/python"), SLAVE_ARGS("python"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU(2), SLAVE_SIZE(768*1024*1024), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave"), SLAVE_GROUP_ID(0), SLAVE_GROUP_N(6), SLAVE_CPU(2), SLAVE_SIZE(32*1024*1024), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave"), SLAVE_GROUP_ID(1), SLAVE_GROUP_N(6), SLAVE_CPU(3), SLAVE_SIZE(1*1024*1024), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave"), SLAVE_GROUP_ID(2), SLAVE_GROUP_N(6), SLAVE_CPU(4), SLAVE_SIZE(1*1024*1024), SLAVE_ENV() },
    { SLAVE_PATH("./slave"), SLAVE_ARGS("slave"), SLAVE_GROUP_ID(3), SLAVE_GROUP_N(6), SLAVE_CPU(5), SLAVE_SIZE(1*1024*1024), SLAVE_ENV() },
};

static uint slavesCounter;

//
static volatile sig_atomic_t sigTERM = 0;
static volatile sig_atomic_t sigUSR1 = 0;
static volatile sig_atomic_t sigUSR2 = 0;
static volatile sig_atomic_t sigALRM = 0;

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
    }
}

static pid_t sid;

int main (void) {

    if (prctl(PR_SET_NAME, (unsigned long)PROGNAME, 0, 0, 0))
        return 1;

    int status = 1;

    sid = setsid();

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
      ) goto EXIT;

#if 0
    // FIFO SCHEDULING
    struct sched_param params;

    memset(&params, 0, sizeof(params));

    params.sched_priority = 1;

    if (sched_setscheduler(0, SCHED_FIFO, &params) == -1)
        goto EXIT;
#endif
    // CPU AFFINITY
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(MASTER_CPU, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        goto EXIT;

    // TODO: FIXME: CLOSE STDIN, AND REOPEN AS /DEV/NULL

    // TODO: FIXME: FORK?

    // SETUP LIMITS
    const struct rlimit limit = {
        .rlim_cur = FD_MAX,
        .rlim_max = FD_MAX,
        };

    if (setrlimit(RLIMIT_NOFILE, &limit))
        goto EXIT;

    // TODO: FIXME: other limits

    // TODO: FIXME: setup /proc pipe limits
    // TODO: FIXME: setup /proc scheduling

    // TODO: FIXME: setup pipes sizes

    // TODO: FIXME: setup malloc.c for us to use in the master process -> called manually, not injected

    //
    slavesCounter = 0;

    { uint slaveID = 0; Slave* slave = slaves; u64


        slaveStart = MASTER_SIZE


        ; u64 cpus = 1ULL << MASTER_CPU;
        do {
            slave->id = slaveID++;
            slave->pid = 0;
            slave->code = 0;
            slave->started = 0;
            slave->start = slaveStart;
            slave->size += 65535;
            slave->size /= 65536;
            slave->size *= 65536;
            slaveStart += slave->size;
            cpus |= (1ULL << slave->cpu);
        } while (++slave != SLAVES_LMT);
        // MAKE SURE EVERYTHING HAS FIT
        if (slaveStart > BUFF_SIZE) {
            DBGPRINT("INSUFFICIENT BUFFER");
            return 1;
        }
        // ONE, AND ONLY ONE PROCESS PER CPU
        if (__builtin_popcountll(cpus) != (SLAVES_N + 1)) {
            DBGPRINT("BAD CPUS");
            return 1;
        }
    }

    // THE PROCESS AND ALL ITS THREADS MUST RUN ON THE SAME CPU, IN FIFO SCHEDULING MODE
    // THIS MAKES EVERYTHING THREAD SAFE (BETWEEN TWO POINTS WITH NO SYSCALLS/BLOCKING OPERATIONS)
    // - WILL SIGNALS INTERFER IN CONTEXT SWITCHING?
    // - WILL STACK INCREASE / PAGE FAULTS INTERFER IN CONTEXT SWITCHING?

    { // OPEN THE BUFFER FILE
            // O_DIRECT
        const int fd = open(BUFF_PATH, O_RDWR | O_NOATIME | O_NOCTTY);
        if (fd == -1)
            return 1;
        if (dup2(fd, BUFFER_FD) != BUFFER_FD)
            return 1;
        if (close(fd))
            return 1;
    }

    // ter certeza de que tem este tamanho
    // AO MESMO TEMPO, PREVINE QUE WRITE() NESTE FD ESCREVE SOBRE A MEMÓRIA
    // TODO: FIXME: isso adianta, ou precisa testar um write()? :/
    if (lseek(BUFFER_FD, BUFF_SIZE, SEEK_SET) != BUFF_SIZE)
        goto EXIT;

    // MAP_LOCKED vs MAP_NORESERVE
    // TAMANHO TEM QUE SER MULTIPLO DO PAGE SIZE A SER USADO
    if (mmap(BUFFER, BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_LOCKED | MAP_POPULATE, BUFFER_FD, 0) != BUFFER)
        goto EXIT;

    //
    { int fds[2] = { -1, -1 };
        if (pipe2(fds, O_DIRECT) ||
            dup2(fds[0], ANY_GET_FD) != ANY_GET_FD || close(fds[0]) ||
            dup2(fds[1], ANY_PUT_FD) != ANY_PUT_FD || close(fds[1])
        ) goto EXIT;
    }

    { int fds[2] = { -1, -1 };
        if (pipe2(fds, O_DIRECT) ||
            dup2(fds[0], MASTER_GET_FD) != MASTER_GET_FD || close(fds[0]) ||
            dup2(fds[1], MASTER_PUT_FD) != MASTER_PUT_FD || close(fds[1])
        ) goto EXIT;
    }

    { //
        uint slaveID = SLAVES_N;

        while (slaveID--) { int fds[2] = { -1, -1 };
            if (pipe2(fds, O_DIRECT) ||
              dup2(fds[0], SLAVE_GET_FD(slaveID)) != SLAVE_GET_FD(slaveID) || close(fds[0]) ||
              dup2(fds[1], SLAVE_PUT_FD(slaveID)) != SLAVE_PUT_FD(slaveID) || close(fds[1])
            ) goto EXIT;
        }
    }

    { // INSTALA O TIMER
        const struct itimerval interval = {
            .it_interval.tv_sec = 30,
            .it_interval.tv_usec = 0,
            .it_value.tv_sec = 30,
            .it_value.tv_usec = 0,
            };
        if (setitimer(ITIMER_REAL, &interval, NULL))
            goto EXIT;
    }

    { // LANÇA TODOS OS PROCESSOS
        Slave* slave = slaves;

        do {
if (slave->id ==0) {
            // TODO: FIXME: limpar os pipes antes de iniciar o processo?
            // TODO: FIXME: A INICIALIZACAO DESTA MEMORIA DEVE SER FEITA AQUI, PARA QUE SUBPROCESSOS TAMBÉM POSSAM TODOS COMPARTILHAR ESTA ALOCACAO DE MEMORIA
            const u64 slaveStarted = rdtsc();

            const pid_t slavePID = fork();

            if (slavePID == -1)
                return 1;

            // TODO: FIXME: poderia simplesmente usar o cpuid(), ler o header do buffer, e ler num array[cpusN] o offset seguida com um pread()
            if (slavePID == 0) {
                // CPU AFFINITY
                cpu_set_t set;
                CPU_ZERO(&set);
                CPU_SET(slave->cpu, &set);
                if (sched_setaffinity(0, sizeof(set), &set))
                    return 1;
                //
                char var[512];
                snprintf(var, sizeof(var), "SLAVEPARAMS=" "%02X" "%02X" "%02X" "%02X" "%016llX" "%016llX" "%02X" "%016llX" "%016llX",
                    (uint)slave->id, (uint)SLAVES_N, (uint)slave->groupID, (uint)slave->groupN, (uintll)slaveStarted, (uintll)slavesCounter, (uint)slave->cpu, (uintll)slave->start, (uintll)slave->size);
                slave->env[0] = var;
                // EXECUTE IT
                execve(slave->path, slave->args, slave->env);
                return 1;
            }

            //
            slave->pid      = slavePID;
            slave->started  = slaveStarted;
            slave->code     = slavesCounter++;
}
        } while (++slave != SLAVES_LMT);
    }

    SLAVE_PUT(1, "EU SOU O BOZO\n", 14);

    //
    while (pause() == -1 && errno == EINTR && sigTERM == 0);

    //if (errno != ECHILD)
        //goto EXIT_SLAVES;

    // ALL PROCESSES TERMINATED

    //
    if (lseek(BUFFER_FD, 0, SEEK_CUR) != BUFF_SIZE)
        goto EXIT_SLAVES;

    if (close(BUFFER_FD))
        goto EXIT_SLAVES;

    status = 0;

EXIT_SLAVES:
    // TODO: FIXME: wait() non blocking, e limpa todos

    // SEND SIGNAL TERM TO ALL SLAVES
#if 1
    // NOTE: VAI MANDAR PARA SI MESMO TAMBÉM
    kill(0, SIGTERM);
    // NOTE: EM ALGUM MOMENTO DAQUI PARA FRENTE ELE TERÁ O sigTERM SETADO, CONFUNDÍVEL COM ALGO ENVIADO DE FORA
#else // ESTA OUTRA VERSÃO NÃO ATINGE POSSÍVEIS SUBPROCESSOS
    // NOTE: É POSSÍVEL QUE ALGO ESCAPE SE FIZER FORK(); MAS O MESMO É POSSÍVEL COM UM SETSID(); ENTÃO AMBOS SÃO IGUALMENTE INSEGUROS
    // NOTE: O PROJETO TODO PODE ATÉ CRIAR THREADS, MAS NÃO OUTROS PROCESSOS
    // SE FOR CRIADO OUTROS PROCESSOS ALÉM DAS THREADS, ELES FICARÃO RODANDO E IMPEDIRÃO NOSSO WAIT
    { Slave* slave = slaves;
        do {
            if (slave->pid)
                kill(slave->pid, SIGTERM);
        } while (++slave != SLAVES_LMT);
    }
#endif

#if 0
    // TODO: FIXME: wait() non blocking, e limpa todos

    // KILL THE REMAINING
    { Slave* slave = slaves;
        do {
            if (slave->pid)
                kill(slave->pid, SIGKILL);
        } while (++slave != SLAVES_LMT);
    }
#endif

    // WAIT ALL SUBPROCESSES TO TERMINATE
    while (wait(NULL) != -1 || errno == EINTR);

    // NO PROBLEM TO CLOSE IF NOT OPEN
    close(ANY_GET_FD);
    close(ANY_PUT_FD);
    close(MASTER_GET_FD);
    close(MASTER_PUT_FD);
    close(BUFFER_FD);

    { uint slaveID = 0;
        do {
            close(SLAVE_GET_FD(slaveID));
            close(SLAVE_PUT_FD(slaveID));
        } while (++slaveID != SLAVES_N);
    }

    // NO PROBLEM TO UNMAP IF NOT MAPPED

EXIT:
    return status;
}

// COMO LIMPAR UM PIPE:
// é modo packet
// write("FIM:COOKIE")
//  while(read() != "FIM:COOKIE");
// =]



// TODO: FIXME: for NUMA systems, it's better to test CPU<->memory regions, on start
// then we only set first cpu0 for the first slave
