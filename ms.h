/*

*/

#define PROGNAME "ms"

// TODO: FIXME: este é a quantidade, e nao o máximo :S
#define FD_MAX 16384

//
#define BUFFER_FD (FD_MAX - 1)

#define BUFFER ((void*)0x2000ULL)

// FOR DEBUGGING
#define BOFFSET(x) ((x) ?  ((uintll)((void*)(x) - (void*)BUFFER_INFO)) : 0ULL)

typedef struct BufferInfo BufferInfo;

struct BufferInfo {
    u8 initialized;
    u8 id;
    u8 n; // quantos processos tem
    u8 groupID;
    u8 groupN;
    u8 cpu;
    u16 code;
    u64 pid;
    u64 started;
    u64 start;
    u64 size;
};

// QUALQUER UM ESCREVE, QUALQUER UM LE
#define ANY_GET_FD (FD_MAX - 2)
#define ANY_PUT_FD (FD_MAX - 3)

// MASTER
#define MASTER_GET_FD (FD_MAX - 4)
#define MASTER_PUT_FD (FD_MAX - 5)

// THE CURRENT SLAVE SO WE DON'T NEED TO KNOW OUR ID
#define SELF_GET_FD (FD_MAX - 6)
#define SELF_PUT_FD (FD_MAX - 7)

// SLAVES ESPECÍFICOS
#define SLAVE_GET_FD(slaveID) ((FD_MAX - 8) - 2*(slaveID))
#define SLAVE_PUT_FD(slaveID) ((FD_MAX - 9) - 2*(slaveID))

//
#define ANY_GET(data, size) _GET(ANY_GET_FD, data, size)
#define ANY_PUT(data, size) _PUT(ANY_PUT_FD, data, size)

#define MASTER_GET(data, size) _GET(MASTER_GET_FD, data, size)
#define MASTER_PUT(data, size) _PUT(MASTER_PUT_FD, data, size)

#define SELF_GET(data, size) _GET(SELF_GET_FD, data, size)
#define SELF_PUT(data, size) _PUT(SELF_PUT_FD, data, size)

#define SLAVE_GET(slaveID, data, size) _GET(SLAVE_GET_FD(slaveID), data, size)
#define SLAVE_PUT(slaveID, data, size) _PUT(SLAVE_PUT_FD(slaveID), data, size)

// pattern: usa o X_GET/PUT() para coisas blocking
// usar io uring para coisas ascincronas/nonblocking
// passa endereço, size se for necessario repassar coisas grandes

static inline uint _GET (const int fd, void* const restrict data, const uint size) {

    int received;

    while ((received = read(fd, data, size)) == -1)
        if (errno != EINTR)
            abort();

    if (!(1 <= received && received <= size))
        abort();

    return received;
}

// É MODO PACOTE, ENTAO SE NAO FALHOU É PORQUE ENVIOU TUDO
static inline void _PUT (const int fd, const void* const restrict data, const uint size) {

    int sent;

    while ((sent = write(fd, data, size)) == -1)
        if (errno != EINTR)
            abort();

    if (sent != (int)size)
        abort();
}
