/*
    COMMON BETWEEN THE DAEMON AND THE INTERPRETER
*/

#ifndef PROGNAME
#define PROGNAME "dedipy"
#endif

// TODO: FIXME: este é a quantidade, e nao o máximo :S
#define FD_MAX 16384

//
#define BUFFER_FD (FD_MAX - 1)

//#define BUFFER ((void*)0x2000ULL)
#define BUFF_ADDR ((void*)0x400000000UL)

// QUALQUER UM ESCREVE, QUALQUER UM LE
#define ANY_GET_FD (FD_MAX - 2)
#define ANY_PUT_FD (FD_MAX - 3)

// DAEMON
#define DAEMON_GET_FD (FD_MAX - 4)
#define DAEMON_PUT_FD (FD_MAX - 5)

// THE CURRENT WORKER SO WE DON'T NEED TO KNOW OUR ID
#define SELF_GET_FD (FD_MAX - 6)
#define SELF_PUT_FD (FD_MAX - 7)

// WORKERS ESPECÍFICOS
#define WORKER_GET_FD(workerID) ((int)((FD_MAX - 8) - 2*(workerID)))
#define WORKER_PUT_FD(workerID) ((int)((FD_MAX - 9) - 2*(workerID)))

//
#define ANY_GET(data, size) _GET(ANY_GET_FD, data, size)
#define ANY_PUT(data, size) _PUT(ANY_PUT_FD, data, size)

#define DAEMON_GET(data, size) _GET(DAEMON_GET_FD, data, size)
#define DAEMON_PUT(data, size) _PUT(DAEMON_PUT_FD, data, size)

#define SELF_GET(data, size) _GET(SELF_GET_FD, data, size)
#define SELF_PUT(data, size) _PUT(SELF_PUT_FD, data, size)

#define WORKER_GET(workerID, data, size) _GET(WORKER_GET_FD(workerID), data, size)
#define WORKER_PUT(workerID, data, size) _PUT(WORKER_PUT_FD(workerID), data, size)

// pattern: usa o X_GET/PUT() para coisas blocking
// usar io uring para coisas ascincronas/nonblocking
// passa endereço, size se for necessario repassar coisas grandes

static inline uint _GET (const int fd, void* const restrict data, const uint size) {

    ssize_t received;

    while ((received = read(fd, data, size)) == -1)
        if (errno != EINTR)
            abort();

    if (!(1 <= received && received <= size))
        abort();

    return (uint)received;
}

// É MODO PACOTE, ENTAO SE NAO FALHOU É PORQUE ENVIOU TUDO
static inline void _PUT (const int fd, const void* const restrict data, const uint size) {

    ssize_t sent;

    while ((sent = write(fd, data, size)) == -1)
        if (errno != EINTR)
            abort();

    if (sent != (int)size)
        abort();
}
