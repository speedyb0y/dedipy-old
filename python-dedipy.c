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

#define LOG_PREPEND "WORKER [%u] "
#define LOG_PREPEND_ARGS  myID

#include "util.c"

#include "dedipy.c"

#define SELF (&BUFFER->workers[myID])

#define GROUP (BUFFER->group)

//
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

void dedipy_main (void) {

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
