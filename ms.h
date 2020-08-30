/*

*/

#define CLEAR_CHUNK(c, s) ({ })

#define PROGNAME "ms"

// TODO: FIXME: este é a quantidade, e nao o máximo :S
#define FD_MAX 16384

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

#define ROOTS_SIZES_FST 32ULL
#define ROOTS_SIZES_LST 31665934879948ULL
#define ROOTS_SIZES_LMT 35184372088832ULL

#define ROOTS_N 200

#define N_START 5
#define X_DIVISOR 5
#define X_SALT 1
#define X_LAST 5

static const u64 ROOTS_SIZES[] = {0x20ULL,0x26ULL,0x2CULL,0x33ULL,0x39ULL,0x40ULL,0x4CULL,0x59ULL,0x66ULL,0x73ULL,0x80ULL,0x99ULL,0xB3ULL,0xCCULL,0xE6ULL,0x100ULL,0x133ULL,0x166ULL,0x199ULL,0x1CCULL,0x200ULL,0x266ULL,0x2CCULL,0x333ULL,0x399ULL,0x400ULL,0x4CCULL,0x599ULL,0x666ULL,0x733ULL,0x800ULL,0x999ULL,0xB33ULL,0xCCCULL,0xE66ULL,0x1000ULL,0x1333ULL,0x1666ULL,0x1999ULL,0x1CCCULL,0x2000ULL,0x2666ULL,0x2CCCULL,0x3333ULL,0x3999ULL,0x4000ULL,0x4CCCULL,0x5999ULL,0x6666ULL,0x7333ULL,0x8000ULL,0x9999ULL,0xB333ULL,0xCCCCULL,0xE666ULL,0x10000ULL,0x13333ULL,0x16666ULL,0x19999ULL,0x1CCCCULL,0x20000ULL,0x26666ULL,0x2CCCCULL,0x33333ULL,0x39999ULL,0x40000ULL,0x4CCCCULL,0x59999ULL,0x66666ULL,0x73333ULL,0x80000ULL,0x99999ULL,0xB3333ULL,0xCCCCCULL,0xE6666ULL,0x100000ULL,0x133333ULL,0x166666ULL,0x199999ULL,0x1CCCCCULL,0x200000ULL,0x266666ULL,0x2CCCCCULL,0x333333ULL,0x399999ULL,0x400000ULL,0x4CCCCCULL,0x599999ULL,0x666666ULL,0x733333ULL,0x800000ULL,0x999999ULL,0xB33333ULL,0xCCCCCCULL,0xE66666ULL,0x1000000ULL,0x1333333ULL,0x1666666ULL,0x1999999ULL,0x1CCCCCCULL,0x2000000ULL,0x2666666ULL,0x2CCCCCCULL,0x3333333ULL,0x3999999ULL,0x4000000ULL,0x4CCCCCCULL,0x5999999ULL,0x6666666ULL,0x7333333ULL,0x8000000ULL,0x9999999ULL,0xB333333ULL,0xCCCCCCCULL,0xE666666ULL,0x10000000ULL,0x13333333ULL,0x16666666ULL,0x19999999ULL,0x1CCCCCCCULL,0x20000000ULL,0x26666666ULL,0x2CCCCCCCULL,0x33333333ULL,0x39999999ULL,0x40000000ULL,0x4CCCCCCCULL,0x59999999ULL,0x66666666ULL,0x73333333ULL,0x80000000ULL,0x99999999ULL,0xB3333333ULL,0xCCCCCCCCULL,0xE6666666ULL,0x100000000ULL,0x133333333ULL,0x166666666ULL,0x199999999ULL,0x1CCCCCCCCULL,0x200000000ULL,0x266666666ULL,0x2CCCCCCCCULL,0x333333333ULL,0x399999999ULL,0x400000000ULL,0x4CCCCCCCCULL,0x599999999ULL,0x666666666ULL,0x733333333ULL,0x800000000ULL,0x999999999ULL,0xB33333333ULL,0xCCCCCCCCCULL,0xE66666666ULL,0x1000000000ULL,0x1333333333ULL,0x1666666666ULL,0x1999999999ULL,0x1CCCCCCCCCULL,0x2000000000ULL,0x2666666666ULL,0x2CCCCCCCCCULL,0x3333333333ULL,0x3999999999ULL,0x4000000000ULL,0x4CCCCCCCCCULL,0x5999999999ULL,0x6666666666ULL,0x7333333333ULL,0x8000000000ULL,0x9999999999ULL,0xB333333333ULL,0xCCCCCCCCCCULL,0xE666666666ULL,0x10000000000ULL,0x13333333333ULL,0x16666666666ULL,0x19999999999ULL,0x1CCCCCCCCCCULL,0x20000000000ULL,0x26666666666ULL,0x2CCCCCCCCCCULL,0x33333333333ULL,0x39999999999ULL,0x40000000000ULL,0x4CCCCCCCCCCULL,0x59999999999ULL,0x66666666666ULL,0x73333333333ULL,0x80000000000ULL,0x99999999999ULL,0xB3333333333ULL,0xCCCCCCCCCCCULL,0xE6666666666ULL,0x100000000000ULL,0x133333333333ULL,0x166666666666ULL,0x199999999999ULL,0x1CCCCCCCCCCCULL,0x200000000000ULL};

// FREE: SIZE + PTR + NEXT + ... + SIZE2
// USED: SIZE + DATA...          + SIZE2
// ----- NOTE: o máximo deve ser <= último
// ----- NOTE: estes limites tem que considerar o alinhamento
// ----- NOTE: cuidado com valores grandes, pois ao serem somados com os endereços, haverão overflows
//              REGRA GERAL: (BUFFER + 4*ROOTS_SIZES_LMT) < (1 << 63)   <--- testar no python, pois se ao verificar tiver overflow, não adiantará nada =]
#define CHUNK_SIZE_MIN 32ULL
#define CHUNK_SIZE_MAX 31665934879944ULL

#define _CHUNK_HDR_SIZE_FREE 24ULL
#define _CHUNK_HDR_SIZE_USED  8ULL

#define _CHUNK_PTR_OFFSET   8ULL
#define _CHUNK_NEXT_OFFSET 16ULL

#define _CHUNK_SIZE2_SIZE 8ULL

//
#define BUFFER_FD (FD_MAX - 1)

#define BUFFER            ((void*)0x20000ULL)
#define BUFFER_INFO     ((Buffer*)(BUFFER))
#define BUFFER_ROOTS     ((void**)(BUFFER + sizeof(Buffer)))
#define BUFFER_ROOTS_LST ((void**)(BUFFER + sizeof(Buffer) + sizeof(void*)*(ROOTS_N - 1)))
#define BUFFER_ROOTS_LMT ((void**)(BUFFER + sizeof(Buffer) + sizeof(void*)*(ROOTS_N    )))
#define BUFFER_L          (*(u64*)(BUFFER + sizeof(Buffer) + sizeof(void*)*(ROOTS_N    )))
#define BUFFER_CHUNK0             (BUFFER + sizeof(Buffer) + sizeof(void*)*(ROOTS_N    ) + sizeof(u64))
#define BUFFER_R(size)    (*(u64*)(BUFFER + (size) - sizeof(u64)))
#define BUFFER_LMT                (BUFFER + BUFFER_INFO->size)

// FOR DEBUGGING
#define BOFFSET(x) ((x) ?  ((uintll)((void*)(x) - (void*)BUFFER_INFO)) : 0ULL)

// TEM QUE SER USED, PARA QUE NUNCA TENTEMOS ACESSÁ-LO NA HORA DE JOIN LEFT/RIGHT
// O MENOR TAMANHO POSSÍVEL; SE TENTARMOS ACESSAR ELE, VAMOS DESCOBRIR COM O ASSERT SIZE >= MIN
#define BUFFER_LR_VALUE 2ULL

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
};

typedef u64 ChunkSize;

#define _UNKN_SIZE_FREE_LD(s) ((s) & 1ULL)

// NAO SEI QUAL É, NAO PODE USAR ASSERT DE FLAG
static inline u64 _UNKN_SIZE_LD (ChunkSize s) {
    s &= ~1ULL;
    ASSERT ( CHUNK_SIZE_MIN <= s || s == BUFFER_LR_VALUE ); //  BUFFER_LR_VALUE sem a flag de used
    ASSERT ( CHUNK_SIZE_MAX >= s );
    ASSERT ( s % 8 == 0 );
    return s;
}

static inline u64 _FREE_SIZE_LD (ChunkSize s) {
    ASSERT ( (s & 1ULL) == 1ULL );
    s &= ~1ULL;
    ASSERT ( s % 8 == 0 );
    ASSERT ( CHUNK_SIZE_MIN <= s );
    ASSERT ( CHUNK_SIZE_MAX >  s );
    return s;
}

static inline ChunkSize _FREE_SIZE_ST (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s >= CHUNK_SIZE_MIN );
    ASSERT ( s <= CHUNK_SIZE_MAX );
    ASSERT ( s % 8 == 0 );
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

static inline int    CHUNK_UNKN_IS_FREE  (const void* const chunk                  ) { return _UNKN_SIZE_FREE_LD(_CHUNK_SIZE(chunk)); }
static inline u64    CHUNK_UNKN_LD_SIZE  (const void* const chunk                  ) { return _UNKN_SIZE_LD(_CHUNK_SIZE(chunk)); }
static inline u64    CHUNK_UNKN_LD_SIZE2 (      void* const chunk, const u64 size  ) { return _UNKN_SIZE_LD(_CHUNK_SIZE2(chunk, size)); }
static inline void   CHUNK_USED_ST_SIZE  (      void* const chunk, const u64 size  ) { _CHUNK_SIZE(chunk) = _USED_SIZE_ST(size); }
static inline u64    CHUNK_USED_LD_SIZE  (const void* const chunk                  ) { return _USED_SIZE_LD(_CHUNK_SIZE(chunk)); };
static inline void*  CHUNK_USED_FROM_DATA(      void* const data                   ) { return data - _CHUNK_HDR_SIZE_USED; }
static inline void*  CHUNK_USED_DATA     (      void* const chunk                  ) { return chunk + _CHUNK_HDR_SIZE_USED; }
static inline u64    CHUNK_USED_DATA_SIZE(                         const u64 size  ) { return size - _CHUNK_HDR_SIZE_USED - _CHUNK_SIZE2_SIZE; } // DADO UM CHUNK USED DE TAL TAMANHO, CALCULA O TAMANHO DOS DADOS
static inline void   CHUNK_USED_ST_SIZE2 (      void* const chunk, const u64 size  ) { _CHUNK_SIZE2(chunk, size) = _USED_SIZE_ST(size); }
static inline u64    CHUNK_USED_LD_SIZE2 (const void* const chunk, const u64 size  ) { return _USED_SIZE_LD(_CHUNK_SIZE2(chunk, size)); }
static inline void   CHUNK_FREE_ST_SIZE  (      void* const chunk, const u64 size  ) {        _CHUNK_SIZE(chunk) = _FREE_SIZE_ST(size); }
static inline u64    CHUNK_FREE_LD_SIZE  (const void* const chunk                  ) { return _FREE_SIZE_LD(_CHUNK_SIZE(chunk)); }
static inline void   CHUNK_FREE_ST_PTR   (      void* const chunk, void** const ptr) {         _CHUNK_PTR(chunk) = ptr; }
static inline void** CHUNK_FREE_LD_PTR   (const void* const chunk                  ) { return  _CHUNK_PTR(chunk); }
static inline void   CHUNK_FREE_ST_NEXT  (      void* const chunk, void* const next) {         _CHUNK_NEXT(chunk) = next; }
static inline void*  CHUNK_FREE_LD_NEXT  (const void* const chunk                  ) { return  _CHUNK_NEXT(chunk); }
static inline void** CHUNK_FREE_LD_NEXT_ (const void* const chunk                  ) { return __CHUNK_NEXT(chunk); }
static inline void   CHUNK_FREE_ST_SIZE2 (      void* const chunk, const u64 size  ) {         _CHUNK_SIZE2(chunk, size) = _FREE_SIZE_ST(size); }
static inline u64    CHUNK_FREE_LD_SIZE2 (const void* const chunk, const u64 size  ) { return _FREE_SIZE_LD(_CHUNK_SIZE2(chunk, size)); }

static inline void* CHUNK_LEFT  (void* const chunk          ) { return chunk - _UNKN_SIZE_LD(*(ChunkSize*)(chunk - _CHUNK_SIZE2_SIZE)); }
static inline void* CHUNK_RIGHT (void* const chunk, u64 size) { return chunk + size; }

// BASEADO NO TAMANHO TOTAL DO BUFFER
#define BUFFER_CHUNK0_SIZE(size) ((size) - sizeof(Buffer) - ROOTS_N*sizeof(void*) - sizeof(u64) - sizeof(u64))

#define ASSERT_ADDR_IN_BUFFER(addr) ASSERT( BUFFER <= (void*)(addr) && (void*)(addr) < BUFFER_LMT )
#define ASSERT_ADDR_IN_CHUNKS(addr) ASSERT( (void*)BUFFER_CHUNK0 <= (void*)(addr) && (void*)(addr) < (void*)&BUFFER_R(BUFFER_INFO->size) )

// ESCOLHE O PRIMEIRO PTR
// BASEADO NESTE SIZE, SELECINAR UM PTR
// A PARTIR DESTE PTR É GARANTIDO QUE TODOS OS CHUNKS TENHAM ESTE TAMANHO

// PARA DEIXAR MAIS SIMPLES/MENOS INSTRUCOES
// - O LAST TEM QUE SER UM TAMANHO TAO GRANDE QUE JAMAIS SOLICITAREMOS
// CHUNK_PTR_ROOT_GET() na hora de pegar, usar o ANTEPENULTIMO como limite????
//  e o que mais????

// SE SIZE < ROOTS_SIZES_FST, ENTÃO NÃO HAVERIA ONDE SER COLOCADO
//      NOTE: CHUNK_SIZE_MIN >= ROOTS_SIZES_FST, ENTÃO size >= ROOTS_SIZES_FST
// É RESPONSABILIDADE DO CALLER TER CERTEZA DE QUE size <= CHUNK_SIZE_MAX
//      NOTE: CHUNK_SIZE_MAX <= ROOTS_SIZES_LST, ENTÃO size <= ROOTS_SIZES_LST
//      NOTE: size <= LAST, PARA QUE HAJA UM INDEX QUE GARANTA

// QUEREMOS COLOCAR UM FREE
static inline void** CHUNK_PTR_ROOT_PUT (const u64 size) {

    // JÁ TEM QUE TER TRATADO
    ASSERT ( size >= CHUNK_SIZE_MIN );
    ASSERT ( size <= CHUNK_SIZE_MAX );
    ASSERT ( size % 8 == 0 );

    uint n = N_START;
    uint x = 0;

    // (1 << n) --> (2^n)
    // CONTINUA ANDANDO ENQUANTO PROVIDENCIARMOS TANTO
    while ((((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR)) <= size)
        n += (x = (x + X_SALT) % X_LAST) == 0;
    // O ATUAL NÃO PROVIDENCIAMOS, ENTÃO RETIRA 1
    // voltasGrandes*(voltasMiniN) + voltasMini
    return &BUFFER_ROOTS[(n - N_START)*(X_LAST/X_SALT) + x/X_SALT - 1];
}

static inline void** CHUNK_PTR_ROOT_GET (const u64 size) {

    // JÁ TEM QUE TER TRATADO
    ASSERT ( size >= CHUNK_SIZE_MIN );
    ASSERT ( size <= CHUNK_SIZE_MAX );
    ASSERT ( size % 8 == 0 );

    uint n = N_START;
    uint x = 0;

    // (1 << n) --> (2^n)
    // CONTINUA ANDANDO ENQUANTO O PROMETIDO NÃO SATISFAZER O PEDIDO
    while ((((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR)) < size)
        n += (x = (x + X_SALT) % X_LAST) == 0;

    // voltasGrandes*(voltasMiniN) + voltasMini
    return &BUFFER_ROOTS[(n - N_START)*(X_LAST/X_SALT) + x/X_SALT];
}

static inline void CHUNK_FREE_FILL_AND_REGISTER (void* const chunk, const u64 size) {
    //
    CLEAR_CHUNK(chunk, size);
    // FILL
    CHUNK_FREE_ST_SIZE (chunk, size);
    CHUNK_FREE_ST_SIZE2(chunk, size); void** const ptr = CHUNK_PTR_ROOT_PUT(size);
    CHUNK_FREE_ST_PTR  (chunk,  ptr);
    CHUNK_FREE_ST_NEXT (chunk, *ptr);
    // REGISTER
    if (*ptr)
        CHUNK_FREE_ST_PTR(*ptr, CHUNK_FREE_LD_NEXT_(chunk));
    *ptr = chunk;
    // CONFIRMA QUE O INVERSO ESTÁ CORRETO
    ASSERT(size == CHUNK_FREE_LD_SIZE (chunk));
    ASSERT(size == CHUNK_FREE_LD_SIZE2(chunk, size));
    //ASSERT(ptr  == CHUNK_FREE_LD_PTR  (chunk));
    //ASSERT(*ptr == CHUNK_FREE_LD_NEXT (chunk));
}

// TODO: FIXME: rename to unregister
static inline void CHUNK_FREE_REMOVE (void* const chunk) {
    DBGPRINTF2("UNLINK FREE CHUNK BX%llX SIZE %llu PTR BX%llX NEXT BX%llX", BOFFSET(chunk), (uintll)CHUNK_FREE_LD_SIZE(chunk), BOFFSET(CHUNK_FREE_LD_PTR(chunk)), BOFFSET(CHUNK_FREE_LD_NEXT(chunk)));
    ASSERT (CHUNK_UNKN_IS_FREE(chunk)); // TEM QUE SER UM FREE
    if (CHUNK_FREE_LD_NEXT(chunk)) { // SE TEM UM NEXT
        ASSERT (CHUNK_UNKN_IS_FREE(CHUNK_FREE_LD_NEXT(chunk))); // ELE TAMBÉM TEM QUE SER UM FREE
        CHUNK_FREE_ST_PTR(CHUNK_FREE_LD_NEXT(chunk), CHUNK_FREE_LD_PTR(chunk)); // ELE ASSUME O NOSSO PTR
    } // O NOSSO PTR ASSUME O NOSSO NEXT
    *CHUNK_FREE_LD_PTR(chunk) = CHUNK_FREE_LD_NEXT(chunk);
    CLEAR_CHUNK(chunk, CHUNK_FREE_LD_SIZE(chunk));
    DBGPRINTF2("UNLINK FREE CHUNK BX%llX - DONE", BOFFSET(chunk));
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
static inline void CHUNKS_VERIFY (void) {

    // LEFT/RIGHT
    ASSERT ( BUFFER_L                    == BUFFER_LR_VALUE );
    ASSERT ( BUFFER_R(BUFFER_INFO->size) == BUFFER_LR_VALUE );

    ASSERT( (BUFFER + BUFFER_INFO->size) == BUFFER_LMT );

    u64 totalFree = 0;
    u64 totalUsed = 0;

    void* chunk = BUFFER_CHUNK0;

    while (chunk != &BUFFER_R(BUFFER_INFO->size)) {
        if (CHUNK_UNKN_IS_FREE(chunk)) {
            DBGPRINTF2("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX PTR BX%llX NEXT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(CHUNK_LEFT(chunk)), BOFFSET(CHUNK_RIGHT(chunk, CHUNK_UNKN_LD_SIZE(chunk))), BOFFSET(CHUNK_FREE_LD_PTR(chunk)), BOFFSET(CHUNK_FREE_LD_NEXT(chunk)), (uintll)CHUNK_UNKN_LD_SIZE(chunk), !!CHUNK_UNKN_IS_FREE(chunk));
            ASSERT(*CHUNK_FREE_LD_PTR (chunk) == chunk);
            if (CHUNK_FREE_LD_NEXT(chunk)) {
                ASSERT(CHUNK_UNKN_IS_FREE(chunk));
                ASSERT(CHUNK_FREE_LD_PTR(CHUNK_FREE_LD_NEXT(chunk)) == CHUNK_FREE_LD_NEXT_(chunk));
            }
            ASSERT_ADDR_IN_BUFFER(CHUNK_FREE_LD_PTR(chunk));
            totalFree += CHUNK_FREE_LD_SIZE(chunk);
        } else {
            DBGPRINTF2("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(CHUNK_LEFT(chunk)), BOFFSET(CHUNK_RIGHT(chunk, CHUNK_UNKN_LD_SIZE(chunk))), (uintll)CHUNK_UNKN_LD_SIZE(chunk), !!CHUNK_UNKN_IS_FREE(chunk));
            totalUsed += CHUNK_USED_LD_SIZE(chunk);
        }
        //
        ASSERT( CHUNK_UNKN_LD_SIZE(chunk) == CHUNK_UNKN_LD_SIZE2(chunk, CHUNK_UNKN_LD_SIZE(chunk)));
        ASSERT_ADDR_IN_CHUNKS(chunk);
        //
        //
        chunk = CHUNK_RIGHT(chunk, CHUNK_UNKN_LD_SIZE(chunk));
    }

    const u64 total = totalFree + totalUsed;

    DBGPRINTF2("-- TOTAL %llu ------", (uintll)total);

    ASSERT (total == BUFFER_CHUNK0_SIZE(BUFFER_INFO->size));

    // VERIFICA OS FREES
    int idx = 0; void** ptrRoot = BUFFER_ROOTS;

    do {
        const u64 fst = ROOTS_SIZES[idx]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR NESTE ROOT
        const u64 lmt = ROOTS_SIZES[idx + 1]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR SÓ NOS ROOTS DA FRENTE

        ASSERT (fst < lmt);
        ASSERT (fst >= CHUNK_SIZE_MIN);
        //ASSERT (lmt <= CHUNK_SIZE_MAX || lmt == ROOTS_SIZES_LMT && idx == (ROOTS_N - 1)));

        void* const* ptr = ptrRoot;
        const void* chunk = *ptrRoot;

        DBGPRINTF2("FREE ROOT #%d CHUNK BX%llX", idx, BOFFSET(chunk));

        while (chunk) {
            ASSERT_ADDR_IN_CHUNKS(chunk);
            ASSERT ( CHUNK_UNKN_IS_FREE(chunk) );
            ASSERT ( CHUNK_FREE_LD_SIZE(chunk) >= fst );
            ASSERT ( CHUNK_FREE_LD_SIZE(chunk) <  lmt );
            ASSERT ( CHUNK_FREE_LD_SIZE(chunk) == CHUNK_FREE_LD_SIZE2(chunk, CHUNK_FREE_LD_SIZE(chunk)) );
            ASSERT ( CHUNK_FREE_LD_PTR(chunk) == ptr );
            //ASSERT ( CHUNK_PTR_ROOT_GET(CHUNK_FREE_LD_SIZE(chunk)) == ptrRoot );
            //ASSERT ( CHUNK_PTR_ROOT_PUT(CHUNK_FREE_LD_SIZE(chunk)) == ptrRoot ); QUAL DOS DOIS? :S e um <=/>= em umdeles
            //
            totalFree -= CHUNK_FREE_LD_SIZE(chunk);
            // PRÓXIMO
            ptr = CHUNK_FREE_LD_NEXT_(chunk);
            chunk = CHUNK_FREE_LD_NEXT(chunk);
        }

        ptrRoot++;

    } while (++idx != ROOTS_N);

    ASSERT (ptrRoot == BUFFER_ROOTS_LMT);

    // CONFIRMA QUE VIU TODOS OS FREES VISTOS AO ANDAR TODOS OS CHUNKS
    ASSERT (totalFree == 0);
}
