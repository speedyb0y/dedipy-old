/*

*/

#ifndef DEBUG
#define DEBUG 1
#endif

#define PROGNAME "ms"

// TODO: FIXME: este é a quantidade, e nao o máximo :S
#define FD_MAX 16384

//
#define ADDR(x) ((uintll)(uintptr_t)(x))
#define BOFFSET(x) ((uintll)((void*)(x) - (void*)BUFFER))

#define HEADS_N 128

#define N_START 5
#define X_DIVISOR 5
#define X_SALT 1
#define X_LAST 5

#define FIRST_SIZE 32ULL
#define LAST_SIZE 1503238553ULL

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
    void* heads[HEADS_N];
};

//
#define _CHUNK_USED_SIZE(dataSize) (_CHUNK_HDR_SIZE_USED + ((((u64)(dataSize) + 7ULL) & ~0b111ULL)) + _CHUNK_SIZE2_SIZE)

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define CHUNK_SIZE_FROM_DATA_SIZE(dataSize) (_CHUNK_USED_SIZE(dataSize) > CHUNK_SIZE_MIN ? _CHUNK_USED_SIZE(dataSize) : CHUNK_SIZE_MIN)

/*
 TODO: FIXME: forçar compilacao a falhar :/
#define _CHUNK(t, c) \
    __builtin_choose_expr(( \
        __builtin_types_compatible_p(typeof(c), Chunk*) || \
        __builtin_types_compatible_p(typeof(c), ChunkUnkn*) || \
        __builtin_types_compatible_p(typeof(c), ChunkFree*) || \
        __builtin_types_compatible_p(typeof(c), ChunkUsed*) \
        ), (t*)(c), (long double)1 )
*/

#define _FORCE(t, c) __builtin_choose_expr(__builtin_types_compatible_p(t, typeof(c)), (c), (void)0)

#define FORCE_U64(c) _FORCE(u64, c)

// FREE: SIZE + PTR + NEXT + SIZE2
// USED: SIZE + DATA...    + SIZE2
#define CHUNK_SIZE_MIN 32ULL
#define CHUNK_SIZE_MAX LAST_SIZE

// USED; DUMMY SIZE; MAS TEM QUE SER UM SIZE VÁLIDO POR CAUSA DOS ASSERTS
#define BUFFER_LR_VALUE 32ULL

#define _CHUNK_HDR_SIZE_FREE 24
#define _CHUNK_HDR_SIZE_USED  8

#define _CHUNK_PTR_OFFSET   8
#define _CHUNK_NEXT_OFFSET 16

#define _CHUNK_SIZE2_SIZE 8

//
#define BUFFER_FD (FD_MAX - 1)

#define BUFFER_L          (*(u64*)((void*)BUFFER + sizeof(Buffer)))
#define BUFFER_CHUNK0             ((void*)BUFFER + sizeof(Buffer) + sizeof(u64))
#define BUFFER_R(size)    (*(u64*)((void*)BUFFER + (size) - sizeof(u64)))

#define BUFFER ((Buffer*)0x20000000ULL)

#define BUFFER_LMT ((void*)BUFFER + BUFFER->size)

typedef u64 ChunkSize;

#define _UNKN_SIZE_FREE_LD(s) ((s) & 1ULL)

// NAO SEI QUAL É, NAO PODE USAR ASSERT DE FLAG
static inline u64 _UNKN_SIZE_LD (ChunkSize s) {
    s &= ~1ULL;
    ASSERT ( s % 8 == 0 );
    ASSERT ( CHUNK_SIZE_MIN <= s && s < CHUNK_SIZE_MAX );
    return s;
}

static inline u64 _FREE_SIZE_LD (ChunkSize s) {
    ASSERT ( (s & 1ULL) == 1ULL );
    s &= ~1ULL;
    ASSERT ( s % 8 == 0 && CHUNK_SIZE_MIN <= s && s < CHUNK_SIZE_MAX );
    return s;
}

static inline ChunkSize _FREE_SIZE_ST (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s % 8 == 0 && CHUNK_SIZE_MIN <= s && s < CHUNK_SIZE_MAX );
    return s | 1ULL;
}

static inline u64 _USED_SIZE_LD (const ChunkSize s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s % 8 == 0 && CHUNK_SIZE_MIN <= s && s < CHUNK_SIZE_MAX );
    return s;
}

static inline ChunkSize _USED_SIZE_ST (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s % 8 == 0 && CHUNK_SIZE_MIN <= s && s < CHUNK_SIZE_MAX );
    return s;
}

#define _FREE_SIZE_LD(s) _FREE_SIZE_LD(FORCE_U64(s))
#define _FREE_SIZE_ST(s) _FREE_SIZE_ST(FORCE_U64(s))
#define _USED_SIZE_ST(s) _USED_SIZE_ST(FORCE_U64(s))
#define _USED_SIZE_LD(s) _USED_SIZE_LD(FORCE_U64(s))
#define _UNKN_SIZE_LD(s) _UNKN_SIZE_LD(FORCE_U64(s))

#define __CHUNK_PTR(chunk)  ((void**)((chunk) + _CHUNK_PTR_OFFSET))
#define __CHUNK_NEXT(chunk) ((void**)((chunk) + _CHUNK_NEXT_OFFSET))

#define _CHUNK_SIZE(chunk) (*(ChunkSize*)(chunk))
#define _CHUNK_SIZE2(chunk, size) (*(ChunkSize*)((chunk) + (size) - _CHUNK_SIZE2_SIZE))
#define _CHUNK_PTR(chunk)  (*__CHUNK_PTR (chunk))
#define _CHUNK_NEXT(chunk) (*__CHUNK_NEXT(chunk))

static inline int CHUNK_UNKN_IS_FREE_AND_NOT_LR (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | SIZE (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE(chunk));
    return _CHUNK_SIZE(chunk) != BUFFER_LR_VALUE && _UNKN_SIZE_FREE_LD(_CHUNK_SIZE(chunk));
}

static inline int CHUNK_UNKN_IS_FREE (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | SIZE (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE(chunk));
    return _UNKN_SIZE_FREE_LD(_CHUNK_SIZE(chunk));
}

static inline u64 CHUNK_UNKN_LD_SIZE (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | SIZE (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE(chunk));
    return _UNKN_SIZE_LD(_CHUNK_SIZE(chunk));
}

static inline void CHUNK_USED_ST_SIZE (void* const chunk, const u64 size) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu)", ADDR(chunk), BOFFSET(chunk), (uintll)size);
    _CHUNK_SIZE(chunk) = _USED_SIZE_ST(size);
    DBGPRINTF("FICOU CHUNK %llu BUFFER+%llu | SIZE (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE(chunk));
}

static inline u64 CHUNK_USED_LD_SIZE (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | SIZE (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE(chunk));
    return _USED_SIZE_LD(_CHUNK_SIZE(chunk));
};

static inline void*  CHUNK_USED_FROM_DATA(      void* const data)                  { return data - _CHUNK_HDR_SIZE_USED; }
static inline void*  CHUNK_USED_DATA     (      void* const chunk)                 { return chunk + _CHUNK_HDR_SIZE_USED; }
static inline u64    CHUNK_USED_DATA_SIZE(                         const u64 size) { return size - _CHUNK_HDR_SIZE_USED - _CHUNK_SIZE2_SIZE; } // DADO UM CHUNK USED DE TAL TAMANHO, CALCULA O TAMANHO DOS DADOS

static inline void CHUNK_USED_ST_SIZE2 (void* const chunk, const u64 size) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu)", ADDR(chunk), BOFFSET(chunk), (uintll)size);
    _CHUNK_SIZE2(chunk, size) = _USED_SIZE_ST(size);
    DBGPRINTF("FICOU CHUNK %llu BUFFER+%llu | SIZE2 (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE2(chunk, size));
}

static inline u64 CHUNK_USED_LD_SIZE2 (const void* const chunk, const u64 size) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu)", ADDR(chunk), BOFFSET(chunk), (uintll)size);
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | SIZE2 (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE2(chunk, size));
    return _USED_SIZE_LD(_CHUNK_SIZE2(chunk, size));
}

static inline void CHUNK_FREE_ST_SIZE (void* const chunk, const u64 size) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu)", ADDR(chunk), BOFFSET(chunk), (uintll)size);
    _CHUNK_SIZE(chunk) = _FREE_SIZE_ST(size);
    DBGPRINTF("FICOU CHUNK %llu BUFFER+%llu | SIZE (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE(chunk));
}

static inline u64 CHUNK_FREE_LD_SIZE (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | SIZE (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE(chunk));
    return _FREE_SIZE_LD(_CHUNK_SIZE(chunk));
}

static inline void CHUNK_FREE_ST_PTR (void* const chunk, void** const ptr) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk), ADDR(ptr), BOFFSET(ptr));
    _CHUNK_PTR(chunk) = ptr;
}

static inline void** CHUNK_FREE_LD_PTR (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | PTR %llu", ADDR(chunk), BOFFSET(chunk), ADDR(_CHUNK_PTR(chunk)));
    return _CHUNK_PTR(chunk);
}

static inline void CHUNK_FREE_ST_NEXT (void* const chunk, void* const next) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk), ADDR(next), BOFFSET(next));
    _CHUNK_NEXT(chunk) = next;
}

static inline void* CHUNK_FREE_LD_NEXT (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | PTR %llu", ADDR(chunk), BOFFSET(chunk), ADDR(_CHUNK_NEXT(chunk)));
    return _CHUNK_NEXT(chunk);
}

static inline void** CHUNK_FREE_LD_NEXT_ (const void* const chunk) {
    DBGPRINTF("(%llu BUFFER+%llu)", ADDR(chunk), BOFFSET(chunk));
    return __CHUNK_NEXT(chunk);
}

static inline void CHUNK_FREE_ST_SIZE2 (void* const chunk, const u64 size) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu)", ADDR(chunk), BOFFSET(chunk), (uintll)size);
    _CHUNK_SIZE2(chunk, size) = _FREE_SIZE_ST(size);
    DBGPRINTF("FICOU CHUNK %llu BUFFER+%llu | SIZE2 (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE2(chunk, size));
}

static inline u64 CHUNK_FREE_LD_SIZE2 (const void* const chunk, const u64 size) {
    DBGPRINTF("(%llu BUFFER+%llu, %llu)", ADDR(chunk), BOFFSET(chunk), (uintll)size);
    DBGPRINTF("ESTA  CHUNK %llu BUFFER+%llu | SIZE2 (RAW) %llu", ADDR(chunk), BOFFSET(chunk), (uintll)_CHUNK_SIZE2(chunk, size));
    ASSERT (CHUNK_FREE_LD_SIZE(chunk) == size);
    ASSERT (1); // O LEFT E O RIGHT (RAW) TEM QUE SER O MESMO
    return _FREE_SIZE_LD(_CHUNK_SIZE2(chunk, size));
}

#define CHUNK_LEFT(chunk)         ((void*)(chunk) - *(ChunkSize*)((void*)(chunk) - _CHUNK_SIZE2_SIZE))
#define CHUNK_RIGHT(chunk, size)  ((void*)(chunk) + (size))

#define BUFFER_HEADS_LMT (&BUFFER->heads[HEADS_N])

// BASEADO NO TAMANHO TOTAL DO BUFFER
#define BUFFER_CHUNK0_SIZE(size) ((size) - sizeof(Buffer) - sizeof(u64) - sizeof(u64))

#define ASSERT_ADDR_IN_BUFFER(addr) ASSERT( (void*)BUFFER <= (void*)(addr) && (void*)(addr) < BUFFER_LMT )
#define ASSERT_ADDR_IN_CHUNKS(addr) ASSERT( (void*)BUFFER_CHUNK0 <= (void*)(addr) && (void*)(addr) < (void*)&BUFFER_R(BUFFER->size) )

#if DEBUG
static const u64 MYHEADS[] = {32ULL, 38ULL, 44ULL, 51ULL, 57ULL, 64ULL, 76ULL, 89ULL, 102ULL, 115ULL, 128ULL, 153ULL, 179ULL, 204ULL, 230ULL, 256ULL, 307ULL, 358ULL, 409ULL, 460ULL, 512ULL, 614ULL, 716ULL, 819ULL, 921ULL, 1024ULL, 1228ULL, 1433ULL, 1638ULL, 1843ULL, 2048ULL, 2457ULL, 2867ULL, 3276ULL, 3686ULL, 4096ULL, 4915ULL, 5734ULL, 6553ULL, 7372ULL, 8192ULL, 9830ULL, 11468ULL, 13107ULL, 14745ULL, 16384ULL, 19660ULL, 22937ULL, 26214ULL, 29491ULL, 32768ULL, 39321ULL, 45875ULL, 52428ULL, 58982ULL, 65536ULL, 78643ULL, 91750ULL, 104857ULL, 117964ULL, 131072ULL, 157286ULL, 183500ULL, 209715ULL, 235929ULL, 262144ULL, 314572ULL, 367001ULL, 419430ULL, 471859ULL, 524288ULL, 629145ULL, 734003ULL, 838860ULL, 943718ULL, 1048576ULL, 1258291ULL, 1468006ULL, 1677721ULL, 1887436ULL, 2097152ULL, 2516582ULL, 2936012ULL, 3355443ULL, 3774873ULL, 4194304ULL, 5033164ULL, 5872025ULL, 6710886ULL, 7549747ULL, 8388608ULL, 10066329ULL, 11744051ULL, 13421772ULL, 15099494ULL, 16777216ULL, 20132659ULL, 23488102ULL, 26843545ULL, 30198988ULL, 33554432ULL, 40265318ULL, 46976204ULL, 53687091ULL, 60397977ULL, 67108864ULL, 80530636ULL, 93952409ULL, 107374182ULL, 120795955ULL, 134217728ULL, 161061273ULL, 187904819ULL, 214748364ULL, 241591910ULL, 268435456ULL, 322122547ULL, 375809638ULL, 429496729ULL, 483183820ULL, 536870912ULL, 644245094ULL, 751619276ULL, 858993459ULL, 966367641ULL, 1073741824ULL, 1288490188ULL, 1503238553ULL, 1717986918ULL};
#endif

// ESCOLHE O PRIMEIRO PTR
// BASEADO NESTE SIZE, SELECINAR UM PTR
// A PARTIR DESTE PTR É GARANTIDO QUE TODOS OS CHUNKS TENHAM ESTE TAMANHO
static inline void** CHOOSE_HEAD_PTR (const u64 size) {

    ASSERT ( size >= FIRST_SIZE );
    ASSERT ( size <  LAST_SIZE );
    ASSERT ( size >= CHUNK_SIZE_MIN );
    ASSERT ( size <= CHUNK_SIZE_MAX );
    ASSERT ( size % 8 == 0 );

    uint n = N_START;
    uint x = 0;

    // (1 << n) --> (2^n)
    while (size >= (((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR)))
        n += (x = (x + X_SALT) % X_LAST) == 0;

    // voltasGrandes*(voltasMiniN) + voltasMini - (1 pq é para usar o antes deste)
    const int idx = (n - N_START)*(X_LAST/X_SALT) + x/X_SALT - 1;

#if DEBUG
    DBGPRINTF("head(%llu) = #%d", (uintll)size, idx);
    ASSERT ( idx < HEADS_N );
    ASSERT ( MYHEADS[idx] <= size && size < MYHEADS[idx + 1] );
#endif
    return &BUFFER->heads[idx];
}

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


// MUST HAVE SAME ALIGNMENTS AS MALLOC! :/ @_@
