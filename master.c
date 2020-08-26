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
#include <fcntl.h>
#include <sched.h>
#include <errno.h>

#define loop while (1)
#define elif else if

typedef unsigned int uint;

typedef uint64_t u64;

#include "malloc.h"

#define BUFF_SIZE (2ULL*1024*1024*1024)
#define BUFF_PATH "malloc-buffer"

#define CPU0 4
#define PROCESSES_N 8

typedef struct ProcessKnown ProcessKnown;

struct ProcessKnown {
    u64 pid;
    u64 code;
    u64 launched; // TIME IT WAS LAUNCHED
    u64 start;
    u64 size;
};

// TODO: FIXME: certificar que a soma de todos caberá
static u64 processesSizes[PROCESSES_N] = {
       8*1024*1024,
      16*1024*1024,
      32*1024*1024,
      64*1024*1024,
     128*1024*1024,
     256*1024*1024,
     512*1024*1024,
    1024*1024*1024,
};

static ProcessKnown processes[PROCESSES_N];

static uint processesCounter;

int main (void) {

    //
    const struct rlimit limit = {
        .rlim_cur = FD_MAX,
        .rlim_max = FD_MAX,
        };

    setrlimit(RLIMIT_NOFILE, &limit);

    //
    processesCounter = 0;

    //
    { u64 processStart = 0;
        uint processID = PROCESSES_N;
        while (processID--) { ProcessKnown* const process = &processes[processID];
            process->pid = 0;
            process->code = 0;
            process->launched = 0;
            process->start = processStart;
            process->size = processesSizes[processID];
            processStart += processesSizes[processID]; // UM DEPOIS DO OUTRO
        }
    }

    // THE PROCESS AND ALL ITS THREADS MUST RUN ON THE SAME CPU, IN FIFO SCHEDULING MODE
    // THIS MAKES EVERYTHING THREAD SAFE

    // FIFO SCHEDULING
    struct sched_param params;

    memset(&params, 0, sizeof(params));

    params.sched_priority = 1;

    if (sched_setscheduler(0, SCHED_FIFO, &params) == -1)
        return 1;

    { //
        const int fd = open(BUFF_PATH, O_RDWR | O_DIRECT | O_NOATIME | O_NOCTTY);

        if (fd == -1)
            return 1;
        if (dup2(fd, BUFFER_FD) != BUFFER_FD)
            return 1;
        if (close(fd))
            return 1;
    }

    // MAP_LOCKED vs MAP_NORESERVE
    // TAMANHO TEM QUE SER MULTIPLO DO PAGE SIZE A SER USADO
    if (mmap(BUFFER, BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE| MAP_SHARED, BUFFER_FD, 0) != BUFFER)
        return 1;

    //
    int fds[2];

    if (pipe2(fds, O_DIRECT) ||
        dup2(fds[0], ALL_RD_FD) != ALL_RD_FD || close(fds[0]) ||
        dup2(fds[1], ALL_WR_FD) != ALL_WR_FD || close(fds[1])
      ) return 1;

    if (pipe2(fds, O_DIRECT) ||
        dup2(fds[0], DAEMON_RD_FD) != DAEMON_RD_FD || close(fds[0]) ||
        dup2(fds[1], DAEMON_WR_FD) != DAEMON_WR_FD || close(fds[1])
      ) return 1;

    //
    { uint processID = PROCESSES_N;
        while (processID--) {
            if (pipe2(fds, O_DIRECT) ||
              dup2(fds[0], PROCESS_RD_FD(processID)) != PROCESS_RD_FD(processID) || close(fds[0]) ||
              dup2(fds[1], PROCESS_WR_FD(processID)) != PROCESS_WR_FD(processID) || close(fds[1])
            ) return 1;
        }
    }

    { // INSTALA O TIMER
        const struct itimerval interval = {
            .it_interval.tv_sec = 30,
            .it_interval.tv_usec = 0,
            .it_value.tv_sec = 30,
            .it_value.tv_usec = 0,
            };
        setitimer(ITIMER_REAL, &interval, NULL);
    }

    // LANÇA TODOS OS PROCESSOS
    uint processID = PROCESSES_N;

    while (processID--) {

          const u64 processLaunched = time(NULL);

          const pid_t processPID = fork();

          if (processPID == -1)
              return 1;

          if (processPID == 0) {
              //
              if (dup2(PROCESS_RD_FD(processID), SELF_RD_FD) != SELF_RD_FD ||
                  dup2(PROCESS_WR_FD(processID), SELF_WR_FD) != SELF_WR_FD
                ) return 1;
              // CPU AFFINITY
              cpu_set_t set;
              CPU_ZERO(&set);
              CPU_SET(CPU0 + processID, &set);
              if (sched_setaffinity(0, sizeof(set), &set))
                  return 1;
              // EXECUTE IT
              execve(SLAVE_BIN_PATH, (char*[]) { SLAVE_ARG0, NULL }, (char*[]) { "TERM=linux", "PATH=/usr/local/bin:/bin:/usr/bin", "LD_PRELOAD=" LIB_MALLOC_PATH, NULL });
              return 1;
          }

          //
          ProcessKnown* const process = &processes[processID];

          process->pid      = processPID;
          process->code     = processesCounter++;
          process->launched = processLaunched;

          //
          struct BufferProcessInfo processInfo;

          processInfo.id    = processID;
          processInfo.n     = PROCESSES_N;
          processInfo.pid   = process->pid;
          processInfo.code  = process->code;
          processInfo.start = process->start;
          processInfo.size  = process->size;

          // TODO: FIXME: cuidado para nao bloquear aqui :/
          if (write(PROCESS_WR_FD(processID), &processInfo, sizeof(BufferProcessInfo)) != sizeof(BufferProcessInfo))
              return 1;
    }

    // WAIT ALL PROCESSES TO EXIT
    while (wait(NULL) != -1 || errno == EINTR);

    if (errno != ECHILD)
        return 1;

    // ALL PROCESSES TERMINATED
    return 0;
}
