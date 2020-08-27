
#define PROGNAME "ms"

// TODO: FIXME: este é a quantidade, e nao o máximo :S
#define FD_MAX 16384

#define HEADS_N 128

//
#define _CHUNK_USED_SIZE(dataSize) (sizeof(ChunkUsed) + ((((u64)(dataSize) + 7ULL) & ~0b111ULL)) + sizeof(ChunkTail))

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define CHUNK_SIZE_FROM_DATA_SIZE(dataSize) (_CHUNK_USED_SIZE(dataSize) > CHUNK_SIZE_MIN ? _CHUNK_USED_SIZE(dataSize) : CHUNK_SIZE_MIN)

// TODO: FIXME: forçar compilacao a falhar :/
#define _CHUNK(t, c) \
    __builtin_choose_expr(( \
        __builtin_types_compatible_p(typeof(c), Chunk*) || \
        __builtin_types_compatible_p(typeof(c), ChunkUnkn*) || \
        __builtin_types_compatible_p(typeof(c), ChunkFree*) || \
        __builtin_types_compatible_p(typeof(c), ChunkUsed*) \
        ), (t*)(c), (long double)0 )

// STORE
#define CHUNK(chunk)      _CHUNK(Chunk,     chunk)
#define CHUNK_UNKN(chunk) _CHUNK(ChunkUnkn, chunk)
#define CHUNK_FREE(chunk) _CHUNK(ChunkFree, chunk)
#define CHUNK_USED(chunk) _CHUNK(ChunkUsed, chunk)

//
#define CHUNK_IS_FREE(chunk) (CHUNK_UNKN(chunk)->size & 1ULL)

// FREE: SIZE + PTR + NEXT + TAIL
// USED: SIZE + DATA + TAIL
#define CHUNK_SIZE_MIN 32ULL
#define CHUNK_SIZE_MAX LAST_SIZE

#define CHUNK_SIZE_FREE(size) ((size) | 1ULL)
#define CHUNK_SIZE_USED(size) (size)

#define CHUNK_UNKN_SIZE(chunk) (CHUNK_UNKN(chunk)->size & ~1ULL)
#define CHUNK_FREE_SIZE(chunk) (CHUNK_FREE(chunk)->size & ~1ULL)
#define CHUNK_USED_SIZE(chunk) (CHUNK_USED(chunk)->size)

#define CHUNK_TAIL(chunk, size) ((ChunkTail*)((void*)(chunk) + (size) - sizeof(ChunkTail)))
#define CHUNK_TAIL_SIZE_(chunk, size) (CHUNK_TAIL(chunk, size)->size & ~1ULL)

// NOTE: deveriausar offseof(Chunk.free, data)
#define CHUNK_FROM_CHUNK_USED_DATA(data) ((Chunk*)((void*)(data) - sizeof(u64)))

#define CHUNK_LEFT(chunk)         ((Chunk*)((void*)(chunk) - ((ChunkTail*)((void*)(chunk) - sizeof(ChunkTail)))->size))
#define CHUNK_RIGHT(chunk, size)  ((Chunk*)((void*)(chunk) + (size)))

#define CHUNK_START_LEFT(c, s)  ((Chunk*)((void*)(c) - (s)))
#define CHUNK_START_RIGHT(c, s) ((Chunk*)((void*)(c) + (s)))

typedef union Chunk Chunk;
typedef struct ChunkUnkn ChunkUnkn;
typedef struct ChunkFree ChunkFree;
typedef struct ChunkUsed ChunkUsed;
typedef struct ChunkTail ChunkTail;

struct ChunkUnkn { u64 size; };
struct ChunkFree { u64 size; Chunk** ptr; Chunk* next; };
struct ChunkUsed { u64 size; char data[]; };

union Chunk { ChunkUnkn unknown; ChunkFree free; ChunkUsed used; };

struct ChunkTail { u64 size; };

//
#define BUFFER_FD (FD_MAX - 1)

#define BUFFER ((Buffer*)0x20000000ULL)

#define BUFFER_L          (*(u64*)((void*)BUFFER + sizeof(Buffer)))
#define BUFFER_CHUNK0    ((Chunk*)((void*)BUFFER + sizeof(Buffer) + sizeof(u64)))
#define BUFFER_R(size)    (*(u64*)((void*)BUFFER + (size) - sizeof(u64)))

// BASEADO NO TAMANHO TOTAL DO BUFFER
#define BUFFER_CHUNK0_SIZE(size) ((size) - sizeof(Buffer) - sizeof(u64) - sizeof(u64))

typedef struct Buffer Buffer;

struct Buffer {
    u8 id;
    u8 n; // quantos processos tem
    u8 groupID;
    u8 groupN;
    u8 cpu;
    u8 reserved;
    u16 code;
    u64 pid;
    u64 started;
    u64 start;
    u64 size;
    Chunk* heads[HEADS_N];
};

// 32 38 44 51 57 64 76 89 102 115 128 153 179 204 230 256 307 358 409 460 512 614 716 819 921 1024 1228 1433 1638 1843 2048 2457 2867 3276 3686 4096 4915 5734 6553 7372 8192 9830 11468 13107 14745 16384 19660 22937 26214 29491 32768 39321 45875 52428 58982 65536 78643 91750 104857 117964 131072 157286 183500 209715 235929 262144 314572 367001 419430 471859 524288 629145 734003 838860 943718 1048576 1258291 1468006 1677721 1887436 2097152 2516582 2936012 3355443 3774873 4194304 5033164 5872025 6710886 7549747 8388608 10066329 11744051 13421772 15099494 16777216 20132659 23488102 26843545 30198988 33554432 40265318 46976204 53687091 60397977 67108864 80530636 93952409 107374182 120795955 134217728 161061273 187904819 214748364 241591910 268435456 322122547 375809638 429496729 483183820 536870912 644245094 751619276 858993459 966367641 1073741824 1288490188 1503238553 1717986918

#define N_START 5
#define X_DIVISOR 5
#define X_SALT 1
#define X_LAST 5

#define FIRST_SIZE 32ULL
#define LAST_SIZE 1503238553ULL

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
