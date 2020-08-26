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

#include "util.h"

#include "malloc.h"

#define BUFF_SIZE (2ULL*1024*1024*1024)
#define BUFF_PATH "malloc-buffer"

#define CPU0 4
#define SLAVES_N 8

typedef struct Slave Slave;

struct Slave {
    u64 pid;
    u64 code;
    u64 launched; // TIME IT WAS LAUNCHED
    u64 start;
    u64 size;
};

// TODO: FIXME: certificar que a soma de todos caberá
static const u64 slavesSizes[SLAVES_N] = {
       8*1024*1024,
      16*1024*1024,
      32*1024*1024,
      64*1024*1024,
     128*1024*1024,
     256*1024*1024,
     512*1024*1024,
    1024*1024*1024,
};

static Slave slaves[SLAVES_N];

static uint slavesCounter;

int main (void) {

    //
    const struct rlimit limit = {
        .rlim_cur = FD_MAX,
        .rlim_max = FD_MAX,
        };

    setrlimit(RLIMIT_NOFILE, &limit);

    // TODO: FIXME: other limits

    // TODO: FIXME: setup /proc pipe limits
    // TODO: FIXME: setup /proc scheduling

    // TODO: FIXME: setup pipes sizes

    //
    slavesCounter = 0;

    //
    { u64 slaveStart = 0;
        uint slaveID = SLAVES_N;
        while (slaveID--) { Slave* const slave = &slaves[slaveID];
            slave->pid = 0;
            slave->code = 0;
            slave->launched = 0;
            slave->start = slaveStart;
            slave->size = slavesSizes[slaveID];
            slaveStart += slavesSizes[slaveID]; // UM DEPOIS DO OUTRO
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

    // CPU AFFINITY
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(1, &set);

    if (sched_setaffinity(0, sizeof(set), &set))
        return 1;

    { // OPEN THE BUFFER FILE
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
    if (mmap(BUFFER, BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_SHARED, BUFFER_FD, 0) != BUFFER)
        return 1;

    { //
        int fds[2] = { -1, -1 };

        if (pipe2(fds, O_DIRECT) ||
            dup2(fds[0], ANY_GET_FD) != ANY_GET_FD || close(fds[0]) ||
            dup2(fds[1], ANY_PUT_FD) != ANY_PUT_FD || close(fds[1])
          ) return 1;

        if (pipe2(fds, O_DIRECT) ||
            dup2(fds[0], MASTER_GET_FD) != MASTER_GET_FD || close(fds[0]) ||
            dup2(fds[1], MASTER_PUT_FD) != MASTER_PUT_FD || close(fds[1])
          ) return 1;

        uint slaveID = SLAVES_N;

        while (slaveID--)
            if (pipe2(fds, O_DIRECT) ||
              dup2(fds[0], SLAVE_GET_FD(slaveID)) != SLAVE_GET_FD(slaveID) || close(fds[0]) ||
              dup2(fds[1], SLAVE_PUT_FD(slaveID)) != SLAVE_PUT_FD(slaveID) || close(fds[1])
            ) return 1;
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
    uint slaveID = SLAVES_N;

    while (slaveID--) {

          const u64 slaveLaunched = time(NULL);

          const pid_t slavePID = fork();

          if (slavePID == -1)
              return 1;

          if (slavePID == 0) {
              //
              if (dup2(SLAVE_GET_FD(slaveID), SELF_GET_FD) != SELF_GET_FD ||
                  dup2(SLAVE_PUT_FD(slaveID), SELF_PUT_FD) != SELF_PUT_FD
                ) return 1;
              // CPU AFFINITY
              cpu_set_t set;
              CPU_ZERO(&set);
              CPU_SET(CPU0 + slaveID, &set);
              if (sched_setaffinity(0, sizeof(set), &set))
                  return 1;
              // EXECUTE IT
              execve(SLAVE_BIN_PATH, (char*[]) { SLAVE_ARG0, NULL }, (char*[]) { "TERM=linux", "PATH=/usr/local/bin:/bin:/usr/bin", "LD_PRELOAD=" LIB_MALLOC_PATH, NULL });
              return 1;
          }

          //
          Slave* const slave = &slaves[slaveID];

          slave->pid      = slavePID;
          slave->code     = slavesCounter++;
          slave->launched = slaveLaunched;

          //
          struct SlaveInfo slaveInfo;

          slaveInfo.id    = slaveID;
          slaveInfo.n     = SLAVES_N;
          slaveInfo.pid   = slave->pid;
          slaveInfo.code  = slave->code;
          slaveInfo.start = slave->start;
          slaveInfo.size  = slave->size;

          // TODO: FIXME: cuidado para nao bloquear aqui :/
          if (write(SLAVE_PUT_FD(slaveID), &slaveInfo, sizeof(SlaveInfo)) != sizeof(SlaveInfo))
              return 1;
    }

    write(SLAVE_PUT_FD(1), "EU SOU O BOZO\n", 14);

    // WAIT ALL PROCESSES TO EXIT
    while (wait(NULL) != -1 || errno == EINTR)
        WRITESTR("MASTER - WAITED/INTERRUPTED")
        ;

    if (errno != ECHILD)
        return 1;

    // ALL PROCESSES TERMINATED
    return 0;
}
