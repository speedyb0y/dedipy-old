/*
    TODO: FIXME: TESTAR COM VÁRIAS THREADS/FORKS: WORKER[workerID:subProcessID]

 buff                                buffLMT
 |___________________________________|
 |    ROOTS  | L |    CHUNKS     | R |

  TODO: FIXME: reduzir o PTR e o NEXT para um u32, múltiplo de 16
  TODO: FIXME: interceptar signal(), etc. quem captura eles somos nós. e quando possível, executamos a função deles
  TODO: FIXME: PRECISA DO MSYNC?
  TODO: FIXME: INTERCEPTAR ABORT()
  TODO: FIXME: INTERCEPTAR EXIT()
  TODO: FIXME: INTERCEPTAR _EXIT()
  TODO: FIXME: INTERCEPTAR sched_*()
  TODO: FIXME: INTERCEPTAR exec*()
  TODO: FIXME: INTERCEPTAR system()
  TODO: FIXME: INTERCEPTAR fork()
  TODO: FIXME: INTERCEPTAR clone()
  TODO: FIXME: INTERCEPTAR POSIX ALIGNED MEMORY FUNCTIONS


    cp -v ${HOME}/dedipy/{util.h,dedipy.h,python-dedipy.h,python-dedipy.c} .

    TODO: FIXME: definir malloc como (buff->malloc)
        no main do processo a gente
        todo o codigo vai somente no main
        todos os modulos, libs etc do processo atual a partir dai poderao usar o as definicoes do python-dedipy.h

        BUFFER_MALLOC malloc_f_t malloc;


*/

#ifndef DEDIPY_DEBUG
#define DEDIPY_DEBUG 0
#endif

#ifndef DEDIPY_VERIFY
#define DEDIPY_VERIFY 0
#endif

#ifndef DEDIPY_TEST
#define DEDIPY_TEST 0
#endif

#define _GNU_SOURCE 1

#ifndef _LARGEFILE64_SOURCE
#error "_LARGEFILE64_SOURCE IS NOT DEFINED"
#endif

#if _FILE_OFFSET_BITS != 64
#error "BAD #define _FILE_OFFSET_BITS"
#endif

#define DBG_PREPEND "WORKER [%u] "
#define DBG_PREPEND_ARGS  id

#ifndef DEDIPY_TEST_1_COUNT
#define DEDIPY_TEST_1_COUNT 500
#endif

#ifndef DEDIPY_TEST_2_COUNT
#define DEDIPY_TEST_2_COUNT 150
#endif

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>

#include "util.h"

#include "dedipy.h"

#define BUFF_ROOTS     ((void**)(buff))
#define BUFF_ROOTS_LST ((void**)(buff + sizeof(void*)*(ROOTS_N - 1)))
#define BUFF_ROOTS_LMT ((void**)(buff + sizeof(void*)*(ROOTS_N    )))
#define BUFF_L          (*(u64*)(buff + sizeof(void*)*(ROOTS_N    )))
#define BUFF_CHUNK0             (buff + sizeof(void*)*(ROOTS_N    ) + sizeof(u64))
#define BUFF_R          (*(u64*)(buff + buffSize - sizeof(u64)))
#define BUFF_LMT                (buff + buffSize)

// FOR DEBUGGING
#define BOFFSET(x) ((x) ?  ((uintll)((void*)(x) - (void*)BUFF_ADDR)) : 0ULL)

static uint id;
static uint n;
static uint groupID;
static uint groupN;
static uint cpu;
static u64 pid;
static u64 code;
static u64 started;
static void* buff; // MY BUFFER
static u64 buffSize; // MY SIZE
static u64 buffTotal; // TOTAL, INCLUDING ALL PROCESSES
static int buffFD;

//
#if 0
#define c_clear(c, s) ({ })
#else
#define c_clear(c, s) ({ })
#endif

//
#define C_USED_SIZE__(dataSize) (_C_HDR_SIZE_USED + ((((u64)(dataSize) + 7ULL) & ~0b111ULL)) + C_SIZE2_SIZE__)

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define C_SIZE_FROM_DATA_SIZE(dataSize) (C_USED_SIZE__(dataSize) > C_SIZE_MIN ? C_USED_SIZE__(dataSize) : C_SIZE_MIN)

#define ROOTS_SIZES_FST 32ULL
#define ROOTS_SIZES_LST 31665934879948ULL
#define ROOTS_SIZES_LMT 35184372088832ULL

#define ROOTS_N 200

#define N_START 5
#define X_DIVISOR 5
#define X_SALT 1
#define X_LAST 5

#if DEDIPY_VERIFY || DEBUG >= 3
static const u64 ROOTS_SIZES[] = {0x20ULL,0x26ULL,0x2CULL,0x33ULL,0x39ULL,0x40ULL,0x4CULL,0x59ULL,0x66ULL,0x73ULL,0x80ULL,0x99ULL,0xB3ULL,0xCCULL,0xE6ULL,0x100ULL,0x133ULL,0x166ULL,0x199ULL,0x1CCULL,0x200ULL,0x266ULL,0x2CCULL,0x333ULL,0x399ULL,0x400ULL,0x4CCULL,0x599ULL,0x666ULL,0x733ULL,0x800ULL,0x999ULL,0xB33ULL,0xCCCULL,0xE66ULL,0x1000ULL,0x1333ULL,0x1666ULL,0x1999ULL,0x1CCCULL,0x2000ULL,0x2666ULL,0x2CCCULL,0x3333ULL,0x3999ULL,0x4000ULL,0x4CCCULL,0x5999ULL,0x6666ULL,0x7333ULL,0x8000ULL,0x9999ULL,0xB333ULL,0xCCCCULL,0xE666ULL,0x10000ULL,0x13333ULL,0x16666ULL,0x19999ULL,0x1CCCCULL,0x20000ULL,0x26666ULL,0x2CCCCULL,0x33333ULL,0x39999ULL,0x40000ULL,0x4CCCCULL,0x59999ULL,0x66666ULL,0x73333ULL,0x80000ULL,0x99999ULL,0xB3333ULL,0xCCCCCULL,0xE6666ULL,0x100000ULL,0x133333ULL,0x166666ULL,0x199999ULL,0x1CCCCCULL,0x200000ULL,0x266666ULL,0x2CCCCCULL,0x333333ULL,0x399999ULL,0x400000ULL,0x4CCCCCULL,0x599999ULL,0x666666ULL,0x733333ULL,0x800000ULL,0x999999ULL,0xB33333ULL,0xCCCCCCULL,0xE66666ULL,0x1000000ULL,0x1333333ULL,0x1666666ULL,0x1999999ULL,0x1CCCCCCULL,0x2000000ULL,0x2666666ULL,0x2CCCCCCULL,0x3333333ULL,0x3999999ULL,0x4000000ULL,0x4CCCCCCULL,0x5999999ULL,0x6666666ULL,0x7333333ULL,0x8000000ULL,0x9999999ULL,0xB333333ULL,0xCCCCCCCULL,0xE666666ULL,0x10000000ULL,0x13333333ULL,0x16666666ULL,0x19999999ULL,0x1CCCCCCCULL,0x20000000ULL,0x26666666ULL,0x2CCCCCCCULL,0x33333333ULL,0x39999999ULL,0x40000000ULL,0x4CCCCCCCULL,0x59999999ULL,0x66666666ULL,0x73333333ULL,0x80000000ULL,0x99999999ULL,0xB3333333ULL,0xCCCCCCCCULL,0xE6666666ULL,0x100000000ULL,0x133333333ULL,0x166666666ULL,0x199999999ULL,0x1CCCCCCCCULL,0x200000000ULL,0x266666666ULL,0x2CCCCCCCCULL,0x333333333ULL,0x399999999ULL,0x400000000ULL,0x4CCCCCCCCULL,0x599999999ULL,0x666666666ULL,0x733333333ULL,0x800000000ULL,0x999999999ULL,0xB33333333ULL,0xCCCCCCCCCULL,0xE66666666ULL,0x1000000000ULL,0x1333333333ULL,0x1666666666ULL,0x1999999999ULL,0x1CCCCCCCCCULL,0x2000000000ULL,0x2666666666ULL,0x2CCCCCCCCCULL,0x3333333333ULL,0x3999999999ULL,0x4000000000ULL,0x4CCCCCCCCCULL,0x5999999999ULL,0x6666666666ULL,0x7333333333ULL,0x8000000000ULL,0x9999999999ULL,0xB333333333ULL,0xCCCCCCCCCCULL,0xE666666666ULL,0x10000000000ULL,0x13333333333ULL,0x16666666666ULL,0x19999999999ULL,0x1CCCCCCCCCCULL,0x20000000000ULL,0x26666666666ULL,0x2CCCCCCCCCCULL,0x33333333333ULL,0x39999999999ULL,0x40000000000ULL,0x4CCCCCCCCCCULL,0x59999999999ULL,0x66666666666ULL,0x73333333333ULL,0x80000000000ULL,0x99999999999ULL,0xB3333333333ULL,0xCCCCCCCCCCCULL,0xE6666666666ULL,0x100000000000ULL,0x133333333333ULL,0x166666666666ULL,0x199999999999ULL,0x1CCCCCCCCCCCULL,0x200000000000ULL};
#endif

// FREE: SIZE + PTR + NEXT + ... + SIZE2
// USED: SIZE + DATA...          + SIZE2
// ----- NOTE: o máximo deve ser <= último
// ----- NOTE: estes limites tem que considerar o alinhamento
// ----- NOTE: cuidado com valores grandes, pois ao serem somados com os endereços, haverão overflows
//              REGRA GERAL: (buff + 4*ROOTS_SIZES_LMT) < (1 << 63)   <--- testar no python, pois se ao verificar tiver overflow, não adiantará nada =]
#define C_SIZE_MIN 32ULL
#define C_SIZE_MAX 31665934879944ULL

#define _C_HDR_SIZE_FREE 24ULL
#define _C_HDR_SIZE_USED  8ULL

#define _C_PTR_OFFSET   8ULL
#define _C_NEXT_OFFSET 16ULL

#define C_SIZE2_SIZE__ 8ULL

// TEM QUE SER USED, PARA QUE NUNCA TENTEMOS ACESSÁ-LO NA HORA DE JOIN LEFT/RIGHT
// O MENOR TAMANHO POSSÍVEL; SE TENTARMOS ACESSAR ELE, VAMOS DESCOBRIR COM O ASSERT SIZE >= MIN
#define BUFF_LR_VALUE 2ULL // ass

static inline u64 c_size_decode_isfree (const u64 s) {
    return s & 1ULL;
}

// NAO SEI QUAL É, NAO PODE USAR ASSERT DE FLAG
static inline u64 c_size_decode (u64 s) {
    s &= ~1ULL;
    ASSERT ( C_SIZE_MIN <= s || s == BUFF_LR_VALUE ); //  BUFF_LR_VALUE sem a flag de used
    ASSERT ( C_SIZE_MAX >= s );
    ASSERT ( s % 8 == 0 || s == BUFF_LR_VALUE );
    return s;
}

static inline u64 f_size_decode (u64 s) {
    ASSERT ( (s & 1ULL) == 1ULL );
    s &= ~1ULL;
    ASSERT ( C_SIZE_MIN <= s );
    ASSERT ( C_SIZE_MAX >  s );
    ASSERT ( s % 8 == 0 );
    return s;
}

static inline u64 f_size_encode (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s >= C_SIZE_MIN );
    ASSERT ( s <= C_SIZE_MAX );
    ASSERT ( s % 8 == 0 );
    return s | 1ULL;
}

static inline u64 u_size_decode (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s % 8 == 0 && C_SIZE_MIN <= s && s < C_SIZE_MAX );
    return s;
}

static inline u64 u_size_encode (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s % 8 == 0 && C_SIZE_MIN <= s && s < C_SIZE_MAX );
    return s;
}

#define c_size_decode(s) c_size_decode(FORCE_U64(s))
#define f_size_decode(s) f_size_decode(FORCE_U64(s))
#define f_size_encode(s) f_size_encode(FORCE_U64(s))
#define u_size_decode(s) u_size_decode(FORCE_U64(s))
#define u_size_encode(s) u_size_encode(FORCE_U64(s))

#define C_PTR__(chunk)  ((void**)((chunk) + _C_PTR_OFFSET))
#define C_NEXT__(chunk) ((void**)((chunk) + _C_NEXT_OFFSET))

#define C_SIZE_(chunk) (*(u64*)(chunk))
#define C_SIZE2_(chunk, size) (*(u64*)((chunk) + (size) - C_SIZE2_SIZE__))
#define C_PTR_(chunk)  (*C_PTR__ (chunk))
#define C_NEXT_(chunk) (*C_NEXT__(chunk))

static inline int    c_is_free  (const void* const chunk                  ) { return c_size_decode_isfree(C_SIZE_ (chunk)); }
static inline u64    c_ld_size  (const void* const chunk                  ) { return c_size_decode       (C_SIZE_ (chunk)); }
static inline u64    c_ld_size2 (      void* const chunk, const u64 size  ) { return c_size_decode       (C_SIZE2_(chunk, size)); }
static inline void   u_st_size  (      void* const chunk, const u64 size  ) {                      C_SIZE_(chunk) = u_size_encode(size); }
static inline u64    u_ld_size  (const void* const chunk                  ) { return u_size_decode(C_SIZE_(chunk)); };
static inline void*  u_data     (      void* const chunk                  ) { return chunk + _C_HDR_SIZE_USED; }
static inline void*  u_from_data(      void* const data                   ) { return data - _C_HDR_SIZE_USED; }
static inline u64    u_data_size(                         const u64 size  ) { return size - _C_HDR_SIZE_USED - C_SIZE2_SIZE__; } // DADO UM CHUNK USED DE TAL TAMANHO, CALCULA O TAMANHO DOS DADOS
static inline void   u_st_size2 (      void* const chunk, const u64 size  ) {                      C_SIZE2_(chunk, size) = u_size_encode(size); }
static inline u64    u_ld_size2 (const void* const chunk, const u64 size  ) { return u_size_decode(C_SIZE2_(chunk, size)); }
static inline void   f_st_size  (      void* const chunk, const u64 size  ) {                      C_SIZE_(chunk) = f_size_encode(size); }
static inline u64    f_ld_size  (const void* const chunk                  ) { return f_size_decode(C_SIZE_(chunk)); }
static inline void   f_st_ptr   (      void* const chunk, void** const ptr) {         C_PTR_(chunk) = ptr; }
static inline void** f_ld_ptr   (const void* const chunk                  ) { return  C_PTR_(chunk); }
static inline void   f_st_next  (      void* const chunk, void* const next) {         C_NEXT_(chunk) = next; }
static inline void*  f_ld_next  (const void* const chunk                  ) { return  C_NEXT_(chunk); }
static inline void** f_ld_next_ (const void* const chunk                  ) { return  C_NEXT__(chunk); }
static inline void   f_st_size2 (      void* const chunk, const u64 size  ) {         C_SIZE2_(chunk, size) = f_size_encode(size); }
static inline u64    f_ld_size2 (const void* const chunk, const u64 size  ) { return f_size_decode(C_SIZE2_(chunk, size)); }

static inline void* c_left  (void* const chunk          ) { return chunk - c_size_decode(*(u64*)(chunk - C_SIZE2_SIZE__)); }
static inline void* c_right (void* const chunk, u64 size) { return chunk + size; }

// BASEADO NO TAMANHO TOTAL DO buff
#define BUFF_CHUNK0_SIZE(size) ((size) - ROOTS_N*sizeof(void*) - sizeof(u64) - sizeof(u64))

#define assert_addr_in_buff(addr) ASSERT( buff <= (void*)(addr) && (void*)(addr) < BUFF_LMT )
#define assert_addr_in_chunks(addr) ASSERT( (void*)BUFF_CHUNK0 <= (void*)(addr) && (void*)(addr) < (void*)&BUFF_R )

// ESCOLHE O PRIMEIRO PTR
// BASEADO NESTE SIZE, SELECINAR UM PTR
// A PARTIR DESTE PTR É GARANTIDO QUE TODOS OS CHUNKS TENHAM ESTE TAMANHO

// PARA DEIXAR MAIS SIMPLES/MENOS INSTRUCOES
// - O LAST TEM QUE SER UM TAMANHO TAO GRANDE QUE JAMAIS SOLICITAREMOS
// f_ptr_root_get() na hora de pegar, usar o ANTEPENULTIMO como limite????
//  e o que mais????

// SE SIZE < ROOTS_SIZES_FST, ENTÃO NÃO HAVERIA ONDE SER COLOCADO
//      NOTE: C_SIZE_MIN >= ROOTS_SIZES_FST, ENTÃO size >= ROOTS_SIZES_FST
// É RESPONSABILIDADE DO CALLER TER CERTEZA DE QUE size <= C_SIZE_MAX
//      NOTE: C_SIZE_MAX <= ROOTS_SIZES_LST, ENTÃO size <= ROOTS_SIZES_LST
//      NOTE: size <= LAST, PARA QUE HAJA UM INDEX QUE GARANTA

// QUEREMOS COLOCAR UM FREE
// SOMENTE A LIB USA ISSO, ENTAO NAO PRECISA DE TANTAS CHEGAGENS?
static inline void** C_PTR_ROOT_PUT_ (const u64 size) {

    uint n = N_START;
    uint x = 0;

    // (1 << n) --> (2^n)
    // CONTINUA ANDANDO ENQUANTO PROVIDENCIARMOS TANTO
    while ((((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR)) <= size)
        n += (x = (x + X_SALT) % X_LAST) == 0;
    // O ATUAL NÃO PROVIDENCIAMOS, ENTÃO RETIRA 1
    // voltasGrandes*(voltasMiniN) + voltasMini
    return &BUFF_ROOTS[(n - N_START)*(X_LAST/X_SALT) + x/X_SALT - 1];
}

static inline void** C_PTR_ROOT_GET_ (const u64 size) {

    uint n = N_START; // TODO: FIXME: ambos devem ser uint mesmo?
    uint x = 0;

    // CONTINUA ANDANDO ENQUANTO O PROMETIDO NÃO SATISFAZER O PEDIDO
    while ((((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR)) < size)
        n += (x = (x + X_SALT) % X_LAST) == 0;

    // voltasGrandes*(voltasMiniN) + voltasMini
    return &BUFF_ROOTS[(n - N_START)*(X_LAST/X_SALT) + x/X_SALT];
}

static inline void** f_ptr_root_get (const u64 size) {

    // JÁ TEM QUE TER TRATADO
    ASSERT ( size >= C_SIZE_MIN );
    ASSERT ( size <= C_SIZE_MAX );
    ASSERT ( size % 8 == 0 );

    void **ptr = C_PTR_ROOT_GET_(size);

    ASSERT (ptr >= BUFF_ROOTS );
    ASSERT (ptr <= BUFF_ROOTS_LST );

    // O ULTIMO DEVE SER PRESERVADO COMO NULL
    ASSERT (*BUFF_ROOTS_LST == NULL);

    return ptr;
}

static inline void** f_ptr_root_put (const u64 size) {

    // JÁ TEM QUE TER TRATADO
    ASSERT ( size >= C_SIZE_MIN );
    ASSERT ( size <= C_SIZE_MAX );
    ASSERT ( size % 8 == 0 );

    void **ptr = C_PTR_ROOT_PUT_(size);

    ASSERT (ptr >= BUFF_ROOTS );
    ASSERT (ptr <= BUFF_ROOTS_LST);

    // O ULTIMO DEVE SER PRESERVADO COMO NULL
    ASSERT (*BUFF_ROOTS_LST == NULL);

    return ptr;
}

static inline void f_fill_and_register (void* const chunk, const u64 size) {
    //
    c_clear(chunk, size);
    // FILL
    f_st_size (chunk, size);
    f_st_size2(chunk, size); void** const ptr = f_ptr_root_put(size);
    f_st_ptr  (chunk,  ptr);
    f_st_next (chunk, *ptr);
    // REGISTER
    if (*ptr)
        f_st_ptr(*ptr, f_ld_next_(chunk));
    *ptr = chunk;
    // CONFIRMA QUE O INVERSO ESTÁ CORRETO
    ASSERT(size == f_ld_size (chunk));
    ASSERT(size == f_ld_size2(chunk, size));
    //ASSERT(ptr  == f_ld_ptr  (chunk));
    //ASSERT(*ptr == f_ld_next (chunk));
}

// TODO: FIXME: rename to unregister
static inline void f_remove (void* const chunk) {
    dbg3("UNLINK FREE CHUNK BX%llX SIZE %llu PTR BX%llX NEXT BX%llX", BOFFSET(chunk), (uintll)f_ld_size(chunk), BOFFSET(f_ld_ptr(chunk)), BOFFSET(f_ld_next(chunk)));
    ASSERT (c_is_free(chunk)); // TEM QUE SER UM FREE
    if (f_ld_next(chunk)) { // SE TEM UM NEXT
        ASSERT (c_is_free(f_ld_next(chunk))); // ELE TAMBÉM TEM QUE SER UM FREE
        f_st_ptr(f_ld_next(chunk), f_ld_ptr(chunk)); // ELE ASSUME O NOSSO PTR
    } // O NOSSO PTR ASSUME O NOSSO NEXT
    *f_ld_ptr(chunk) = f_ld_next(chunk);
    c_clear(chunk, f_ld_size(chunk));
    dbg3("UNLINK FREE CHUNK BX%llX - DONE", BOFFSET(chunk));
}

// MUST HAVE SAME ALIGNMENTS AS MALLOC! :/ @_@
static void VERIFY (void) {
#if DEDIPY_VERIFY
    // LEFT/RIGHT
    ASSERT ( BUFF_L == BUFF_LR_VALUE );
    ASSERT ( BUFF_R == BUFF_LR_VALUE );

    ASSERT( (buff + buffSize) == BUFF_LMT );

    u64 totalFree = 0;
    u64 totalUsed = 0;

    void* chunk = BUFF_CHUNK0;

    while (chunk != &BUFF_R) {
        if (c_is_free(chunk)) {
            dbg3("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX PTR BX%llX NEXT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(c_left(chunk)), BOFFSET(c_right(chunk, c_ld_size(chunk))), BOFFSET(f_ld_ptr(chunk)), BOFFSET(f_ld_next(chunk)), (uintll)c_ld_size(chunk), !!c_is_free(chunk));
            ASSERT(*f_ld_ptr (chunk) == chunk);
            if (f_ld_next(chunk)) {
                ASSERT(c_is_free(chunk));
                ASSERT(f_ld_ptr(f_ld_next(chunk)) == f_ld_next_(chunk));
            }
            assert_addr_in_buff(f_ld_ptr(chunk));
            totalFree += f_ld_size(chunk);
        } else {
            dbg3("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(c_left(chunk)), BOFFSET(c_right(chunk, c_ld_size(chunk))), (uintll)c_ld_size(chunk), !!c_is_free(chunk));
            totalUsed += u_ld_size(chunk);
        }
        //
        ASSERT( c_ld_size(chunk) == c_ld_size2(chunk, c_ld_size(chunk)));
        assert_addr_in_chunks(chunk);
        //
        //
        chunk = c_right(chunk, c_ld_size(chunk));
    }

    const u64 total = totalFree + totalUsed;

    dbg3("-- TOTAL %llu ------", (uintll)total);

    ASSERT (total == BUFF_CHUNK0_SIZE(buffSize));

    // VERIFICA OS FREES
    int idx = 0; void** ptrRoot = BUFF_ROOTS;

    do {
        const u64 fst = ROOTS_SIZES[idx]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR NESTE ROOT
        const u64 lmt = ROOTS_SIZES[idx + 1]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR SÓ NOS ROOTS DA FRENTE

        ASSERT (fst < lmt);
        ASSERT (fst >= C_SIZE_MIN);
        //ASSERT (lmt <= C_SIZE_MAX || lmt == ROOTS_SIZES_LMT && idx == (ROOTS_N - 1)));

        void* const* ptr = ptrRoot;
        const void* chunk = *ptrRoot;

        dbg3("FREE ROOT #%d CHUNK BX%llX", idx, BOFFSET(chunk));

        while (chunk) {
            assert_addr_in_chunks(chunk);
            ASSERT ( c_is_free(chunk) );
            ASSERT ( f_ld_size(chunk) >= fst );
            ASSERT ( f_ld_size(chunk) <  lmt );
            ASSERT ( f_ld_size(chunk) == f_ld_size2(chunk, f_ld_size(chunk)) );
            ASSERT ( f_ld_ptr(chunk) == ptr );
            //ASSERT ( f_ptr_root_get(f_ld_size(chunk)) == ptrRoot );
            //ASSERT ( f_ptr_root_put(f_ld_size(chunk)) == ptrRoot ); QUAL DOS DOIS? :S e um <=/>= em umdeles
            //
            totalFree -= f_ld_size(chunk);
            // PRÓXIMO
            ptr = f_ld_next_(chunk);
            chunk = f_ld_next(chunk);
        }

        ptrRoot++;

    } while (++idx != ROOTS_N);

    ASSERT (ptrRoot == BUFF_ROOTS_LMT);

    // CONFIRMA QUE VIU TODOS OS FREES VISTOS AO ANDAR TODOS OS CHUNKS
    ASSERT (totalFree == 0);
#endif
}

void dedipy_free (void* const data) {

    dbg2("=== FREE(BX%llX) ========================================================================", BOFFSET(data));

    VERIFY();

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        void* chunk = u_from_data(data);

        ASSERT(c_is_free(chunk) == 0);
        ASSERT(c_ld_size(chunk) <= C_SIZE_MAX);

        u64 size = u_ld_size(chunk);

        // JOIN WITH THE LEFT CHUNK
        void* const left = c_left(chunk);

        if (c_is_free(left)) {
            size += f_ld_size(left);
            chunk = left;
            f_remove(chunk);
        }

        // JOIN WITH THE RIGHT CHUNK
        void* const right = c_right(chunk, size);

        if (c_is_free(right)) {
            size += f_ld_size(right);
            f_remove(right);
        }

        //
        f_fill_and_register(chunk, size);
    }
}

void dedipy_main (void) {

    // SUPPORT CALLING FROM MULTIPLE PLACES =]
    static int initialized = 0;

    if (initialized)
        return;

    initialized = 1;

    uintll buffFD_    = 0;
    uintll buffFlags_ = 0;
    uintll buffAddr_  = 0;
    uintll buffTotal_ = 0;
    uintll buffStart_ = 0;
    uintll buffSize_  = 0;
    uintll cpu_       = 0;
    uintll pid_       = 0;
    uintll code_      = 0;
    uintll started_   = 0;
    uintll id_        = 0;
    uintll n_         = 0;
    uintll groupID_   = 0;
    uintll groupN_    = 0;

    const char* var = getenv("DEDIPY");

    //fatal("FAILED TO LOAD ENVIROMENT PARAMS");
    if (var) {
        if (sscanf(var, "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX"  "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX" "%016llX",
            &cpu_, &pid_, &buffFD_, &buffFlags_, &buffAddr_, &buffTotal_, &buffStart_, &buffSize_, &code_, &started_, &id_, &n_, &groupID_, &groupN_) != 14)
            fatal("FAILED TO LOAD ENVIROMENT PARAMS");
    } else { // EMERGENCY MODE =]
        cpu_ = sched_getcpu();
        buffFD_ = 0;
        buffFlags_ = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_FIXED_NOREPLACE;
        buffAddr_ = (uintll)BUFF_ADDR;
        buffTotal_ = 256*1024*1024;
        buffStart_ = 0;
        buffSize_ = buffTotal_;
        pid_ = getpid();
        n_ = 1;
        groupN_ = 1;
    }

    // THOSE ARE FOR EMERGENCY/DEBUGGING
    // THEY ARE IN DECIMAL, FOR MANUAL SETTING IN THE C SOURCE/SHELL
    if ((var = getenv("DEDIPY_BUFF_ADDR"  ))) buffAddr_    = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_FD"    ))) buffFD_      = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_FLAGS" ))) buffFlags_   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_TOTAL" ))) buffTotal_   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_START" ))) buffStart_   = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_BUFF_SIZE"  ))) buffSize_    = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_CPU"        ))) cpu_         = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_ID"         ))) id_          = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_N"          ))) n_           = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_GROUP_ID"   ))) groupID_     = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_GROUP_N"    ))) groupN_      = strtoull(var, NULL, 10);
    if ((var = getenv("DEDIPY_PID"        ))) pid_         = strtoull(var, NULL, 10);

    if ((void*)buffAddr_ != BUFF_ADDR)
        fatal("BUFFER ADDRESS MISMATCH");

    if ((pid_t)pid_ != getpid())
        fatal("PID MISMATCH");

    if (buffStart_ >= buffTotal_)
        fatal("BAD BUFFER START");

    if (buffSize_ == 0)
        fatal("BAD BUFFER SIZE");

    if ((int)cpu_ != sched_getcpu())
        fatal("CPU MISMATCH");

    if (n_ == 0 || n_ > 0xFF)
        fatal("BAD N");

    if (id_ >= n_)
        fatal("BAD ID/N");

    if (groupN_ == 0 || groupN_ > 0xFF)
        fatal("BAD GROUP N");

    if (groupID_ >= groupN_)
        fatal("BAD GROUP ID/N");

    //  | MAP_LOCKED | MAP_POPULATE | MAP_HUGETLB | MAP_HUGE_1GB
    if (BUFF_ADDR != mmap(BUFF_ADDR, buffTotal_, PROT_READ | PROT_WRITE, buffFlags_, (int)buffFD_, 0))
        fatal("FAILED TO MAP BUFFER");

    if (buffFD_ && close(buffFD_))
        fatal("FAILED TO CLOSE BUFFER FD");

    // INFO
    buffFD    = (int)buffFD_;
    buff      = BUFF_ADDR + buffStart_;
    buffSize  = buffSize_;
    buffTotal = buffTotal_;
    id        = id_;
    n         = n_;
    groupID   = groupID_;
    groupN    = groupN_;
    cpu       = cpu_;
    code      = code_;
    pid       = pid_;
    started   = started_;

    //
    memset(buff, 0, buffSize);

    // var + 14*16, strlen(var + 14*16)
    char name[256];

    if (snprintf(name, sizeof(name), PROGNAME "#%u", id) < 2 || prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0))
        fatal("FAILED TO SET PROCESS NAME");

    // LEFT AND RIGHT
    BUFF_L = BUFF_LR_VALUE;
    BUFF_R = BUFF_LR_VALUE;

    // THE INITIAL CHUNK
    // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O C_SIZE_MAX E ROOTS_SIZES_LST SÃO MAIORES DO QUE ELE
    f_fill_and_register(BUFF_CHUNK0, BUFF_CHUNK0_SIZE(buffSize));

    // TODO: FIXME: tentar dar malloc() e realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // C_SIZE_MAX
    // ROOTS_SIZES_LST
    // ROOTS_SIZES_LMT

    dbg("ROOTS_N %llu", (uintll)ROOTS_N);
    dbg("ROOTS_SIZES_FST %llu", (uintll)ROOTS_SIZES_FST);
    dbg("ROOTS_SIZES_LST %llu", (uintll)ROOTS_SIZES_LST);
    dbg("ROOTS_SIZES_LMT %llu", (uintll)ROOTS_SIZES_LMT);

    dbg("C_SIZE_MIN %llu", (uintll)C_SIZE_MIN);
    dbg("C_SIZE_MAX %llu", (uintll)C_SIZE_MAX);

    ASSERT (C_SIZE_MIN == ROOTS_SIZES_FST);
    ASSERT (C_SIZE_MAX <= ROOTS_SIZES_LST);
    ASSERT (C_SIZE_MAX <  ROOTS_SIZES_LMT);

    ASSERT (ROOTS_SIZES_FST < ROOTS_SIZES_LST);
    ASSERT (ROOTS_SIZES_LST < ROOTS_SIZES_LMT);

    ASSERT (f_ptr_root_get(C_SIZE_MIN) == (BUFF_ROOTS));
    ASSERT (f_ptr_root_get(C_SIZE_MAX) == (BUFF_ROOTS + (ROOTS_N - 1)));

    ASSERT (f_ptr_root_put(C_SIZE_MIN) == (BUFF_ROOTS));
    ASSERT (f_ptr_root_put(C_SIZE_MAX) <= (BUFF_ROOTS + (ROOTS_N - 1))); // COMO VAI ARREDONDAR PARA BAIXO, PEDIR O MÁXIMO PODE CAIR LOGO ANTES DO ÚLTIMO SLOT

    ASSERT (BUFF_ROOTS_LMT == (BUFF_ROOTS + ROOTS_N));

    dbg("CPU %u",                  (uint)cpu);
    dbg("PID %llu",                (uintll)pid);
    dbg("ID %u",                   (uint)id);
    dbg("CODE %llu",               (uintll)code);

    dbg("BUFF ADDR 0x%016llX",      (uintll)BUFF_ADDR);
    dbg("BUFF TOTAL SIZE %llu",     (uintll)buffTotal);
    dbg("BUFF START %llu",          (uintll)buffStart_);
    dbg("BUFF 0x%016llX",           BOFFSET(buff));
    dbg("BUFF_ROOTS BX%llX",        BOFFSET(BUFF_ROOTS));
    dbg("BUFF_ROOTS_LMT BX%llX",    BOFFSET(BUFF_ROOTS_LMT));
    dbg("BUFF_L BX%llX",            BOFFSET(&BUFF_L));
    dbg(" BUFF_CHUNK0 BX%llX",      BOFFSET(BUFF_CHUNK0));
    dbg(" BUFF_CHUNK0 SIZE %llu",   (uintll)(f_ld_size(BUFF_CHUNK0)));
    dbg(" BUFF_CHUNK0 PTR BX%llX",  BOFFSET( f_ld_ptr (BUFF_CHUNK0)));
    dbg("*BUFF_CHUNK0 PTR BX%llX",  BOFFSET(*f_ld_ptr (BUFF_CHUNK0)));
    dbg(" BUFF_CHUNK0 NEXT BX%llX", BOFFSET( f_ld_next(BUFF_CHUNK0)));
    dbg("BUFF_R BX%llX",            BOFFSET(&BUFF_R));
    dbg("BUFF_LMT BX%llX",          BOFFSET(BUFF_LMT));
    dbg("BUFF SIZE %llu",          (uintll)buffSize);

    dbg("BUFF_L %llu",    (uintll)BUFF_L);
    dbg("BUFF_R %llu",    (uintll)BUFF_R);

    dbg("---");

    assert_addr_in_buff(BUFF_CHUNK0);
    assert_addr_in_chunks(BUFF_CHUNK0);

    ASSERT ( sizeof(u64) == sizeof(off_t) );
    ASSERT ( sizeof(u64) == sizeof(size_t) );
    ASSERT ( sizeof(u64) == 8 );
    ASSERT ( sizeof(void*) == 8 );

    VERIFY();

    dbg("OKAY!");
}

void* dedipy_malloc (size_t size_) {

    dbg2("=== MALLOC(%llu) ========================================================================", (uintll)size_);

    VERIFY();

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 size = C_SIZE_FROM_DATA_SIZE(size_);

    //
    if (size > C_SIZE_MAX)
        return NULL;

    // PEGA UM LIVRE A SER USADO
    void* used;

    // ENCONTRA A PRIMEIRA LISTA LIVRE
    void** ptr = f_ptr_root_get(size);

    // LOGO APÓS O HEADS, HÁ O LEFT CHUNK, COM UM SIZE FAKE 1, PORTANTO ELE É NÃO-NULL, E VAI PARAR SE NAO TIVER MAIS CHUNKS LIVRES
    while ((used = *ptr) == NULL)
        ptr++;

    // SE
    if (ptr == BUFF_ROOTS_LMT) {
        ASSERT ( used == (void*)BUFF_LR_VALUE ); // TERÁ LIDO DESTA FORMA
        return NULL;
    }

    ASSERT(BUFF_ROOTS <= ptr && ptr < BUFF_ROOTS_LMT);
    ASSERT(c_is_free(used));
    //ASSERT(C_FREE(used)->next == NULL || C_FREE((C_FREE(used)->next))->ptr == &C_FREE(used)->next);
    ASSERT(C_SIZE_MIN <= f_ld_size(used) && f_ld_size(used) <= C_SIZE_MAX && (f_ld_size(used) % 8) == 0);

    //assert_addr_in_chunks(C_FREE(used)->next);

    // TAMANHO DESTE CHUNK FREE
    u64 usedSize = f_ld_size(used);

    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = usedSize - size;

    // REMOVE ELE DE SUA LISTA, MESMO QUE SÓ PEGUEMOS UM PEDAÇO, VAI TER REALOCÁ-LO NA TREE
    f_remove(used);

    // SE DER, CONSOME SÓ UMA PARTE, NO FINAL DELE
    if (C_SIZE_MIN <= freeSizeNew) {
        f_fill_and_register(used, freeSizeNew);
        used += freeSizeNew;
        usedSize = size;
    }

    c_clear(used, usedSize);

    u_st_size (used, usedSize);
    u_st_size2(used, usedSize);

    return u_data(used);
}

// If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
// If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.
void* dedipy_calloc (size_t n, size_t size_) {

    dbg2("=== CALLOC (%llu, %llu) ========================================================================", (uintll)(n), (uintll)size_);

    const u64 size = (u64)n * (u64)size_;

    void* const data = dedipy_malloc(size);

    dbg2("RETURNING DATA SIZE %llu DATA BX%llX", (uintll)size, BOFFSET(data));

    // INITIALIZE IT
    if (data)
        memset(data, 0, size);

    return data;
}

// The  realloc()  function returns a pointer to the newly allocated memory, which is suitably aligned for any built-in type, or NULL if the request failed.
// The returned pointer may be the same as ptr if the allocation was not moved (e.g., there was room to expand the allocation in-place),
//    or different from ptr if the allocation was moved to a new address.
// If size was equal to 0, either NULL or a pointer suitable to be passed to free() is returned.
// If realloc() fails, the original block is left untouched; it is not freed or moved.
void* dedipy_realloc (void* const data_, const size_t dataSizeNew_) {

    dbg2("=== REALLOC (BX%llX, %llu) ========================================================================", BOFFSET(data_), (uintll)dataSizeNew_);

    VERIFY();

    if (data_ == NULL)
        return dedipy_malloc(dataSizeNew_);

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 sizeNew = C_SIZE_FROM_DATA_SIZE(dataSizeNew_);

    //
    if (sizeNew > C_SIZE_MAX)
        return NULL;

    // FOI NOS PASSADO O DATA; VAI PARA O CHUNK
    void* const chunk = u_from_data(data_);

    ASSERT(c_is_free(chunk) == 0);

    //
    u64 size = u_ld_size(chunk);

    dbg2("WILL REALLOC CHUNK BX%llX SIZE %llu -> %llu", BOFFSET(chunk), (uintll)size, (uintll)sizeNew);

    if (size >= sizeNew) {
        // ELE SE AUTOFORNECE
        if ((size - sizeNew) < 64)
            // MAS NÃO VALE A PENA DIMINUIR
            return data_;
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return data_;
    }

    void* right = c_right(chunk, size);

    // SÓ SE FOR FREE E SUFICIENTE
    if (c_is_free(right)) {

        dbg2("RIGHT CHUNK BX%llX IS FREE", BOFFSET(right));

        const u64 rightSize = f_ld_size(right);
        // O QUANTO VAMOS TENTAR RETIRAR DA DIREITA
        const u64 sizeNeeded = sizeNew - size;

        if (rightSize >= sizeNeeded) {
            // DÁ SIM; VAMOS ALOCAR ARRANCANDO DA DIREITA

            // O TAMANHO NOVO DA DIREITA
            const u64 rightSizeNew = rightSize - sizeNeeded;
            dbg2("UA UAU UAU");

            // REMOVE ELE DE SUA LISTA
            f_remove(right);

            if (rightSizeNew >= C_SIZE_MIN) {
                // PEGA ESTE PEDAÇO DELE, PELA ESQUERDA
                size += sizeNeeded; // size -> sizeNew
                // MOVE O COMEÇO PARA A DIREITA
                right += sizeNeeded;
                // REESCREVE
                f_fill_and_register(right, rightSizeNew);
                dbg2("RIGHT CONSUMED PARTIALLY; RIGHT CHUNK BX%llX SIZE %llu PTR BX%llX NEXT BX%llX", BOFFSET(right), (uintll)f_ld_size(right), BOFFSET(f_ld_ptr(right)), BOFFSET(f_ld_next(right)));
            } else { // CONSOME ELE POR INTEIRO
                size += rightSize; // size -> o que ja era + o free chunk
                dbg2("RIGHT CONSUMED ENTIRELY");
            }

            // ESCREVE ELE
            u_st_size (chunk, size);
            u_st_size2(chunk, size);

            dbg2("RETURNING CHUNK BX%llX SIZE %llu DATA BX%llX", BOFFSET(chunk), (uintll)u_ld_size(chunk), BOFFSET(u_data(chunk)));

            return u_data(chunk);
        }
    }

    dbg2("RIGHT CHUNK BX%llX NOT USED; ALLOCATING A NEW ONE", BOFFSET(right));

    // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
    void* const data = dedipy_malloc(dataSizeNew_);

    if (data) {
        // CONSEGUIU
        // COPIA DO CHUNK ORIGINAL
        dbg2("COPYING DATA SIZE %llu BX%llX TO BX%llX", (uintll)u_data_size(size), BOFFSET(data_), BOFFSET(data));
        memcpy(data, data_, u_data_size(size));
        // LIBERA O CHUNK ORIGINAL
        dedipy_free(u_data(chunk));

        dbg2("RETURNING CHUNK BX%llX SIZE %llu DATA BX%llX", BOFFSET(u_from_data(data)), (uintll)u_ld_size(u_from_data(data)), BOFFSET(u_data(u_from_data(data))));

        ASSERT(u_data(u_from_data(data)) == data);
    }

    dbg2("RETURNING DATA BX%llX", BOFFSET(data));

    return data;
}

void* dedipy_reallocarray (void *ptr, size_t nmemb, size_t size) {

    (void)ptr;
    (void)nmemb;
    (void)size;

    fatal("MALLOC - REALLOCARRAY");
}

#if DEDIPY_TEST
static uintll _rand = 0;

static inline u64 RANDOM (const u64 x) {

    _rand += x;
    _rand += rdtsc() & 0xFFFULL;
#if 0
    _rand += __builtin_ia32_rdrand64_step(&_rand);
#endif
    return _rand;
}

static inline u64 TEST_SIZE (u64 x) {

    x = RANDOM(x);

    return (x >> 2) & (
        (x & 0b1ULL) ? (
            (x & 0b10ULL) ? 0xFFFFFULL :   0xFFULL
        ) : (
            (x & 0b10ULL) ?   0xFFFULL : 0xFFFFULL
        ));
}

#endif

//  começa depois disso
//   usar = sizeof(Buffer) + 8 + BUFF->mallocSize + 8;
// le todos os seus de /proc/self/maps
//      if ((usar + ORIGINALSIZE) > BUFF_LMT)
//            abort();
//      pwrite(BUFFER_FD, ORIGINALADDR, ORIGINALSIZE, usar);
//   agora remapeia
//      mmap(ORIGINALADDR, ORIGINALSIZE, prot, flags, BUFFER_FD, usar);
//      usar += ORIGINALSIZE;

// TODO: FIXME: WE WILL NEED A SIGNAL HANDLER

void dedipy_test (void) {

#if DEDIPY_TEST
    // PRINT HOW MUCH MEMORY WE CAN ALLOCATE
    { u64 blockSize = 64*4096; // >= sizeof(void**)

        do { u64 count = 0; void** last = NULL; void** this;
            //
            while ((this = dedipy_malloc(blockSize))) {
#if 1
                memset(this, 0, blockSize);
#endif
                *this = last;
                last = this;
                count += 1;
            }
            // NOW FREE THEM ALL
            while (last) {
                this = *last;
                dedipy_free(last);
                last = this;
            }
            //
            if (count)
                dbg("ALLOCATED %llu BLOCKS of %llu BYTES = %llu", (uintll)count, (uintll)blockSize, (uintll)(count * blockSize));
        } while ((blockSize <<= 1));
    }

    dbg("TEST 0");

    { uint c = DEDIPY_TEST_1_COUNT;
        while (c--) {

            dedipy_free(NULL);

            dedipy_free(dedipy_malloc(TEST_SIZE(c + 1)));
            dedipy_free(dedipy_malloc(TEST_SIZE(c + 2)));
            dedipy_free(dedipy_malloc(TEST_SIZE(c + 3)));

            dedipy_free(dedipy_realloc(dedipy_malloc(TEST_SIZE(c + 4)), TEST_SIZE(c + 10)));
            dedipy_free(dedipy_realloc(dedipy_malloc(TEST_SIZE(c + 5)), TEST_SIZE(c + 11)));

            dedipy_free(dedipy_malloc(TEST_SIZE(c + 6)));
            dedipy_free(dedipy_malloc(TEST_SIZE(c + 7)));

            dedipy_free(dedipy_realloc(dedipy_malloc(TEST_SIZE(c + 8)), TEST_SIZE(c + 12)));
            dedipy_free(dedipy_realloc(dedipy_malloc(TEST_SIZE(c + 9)), TEST_SIZE(c + 13)));
        }
    }

    // TODO: FIXME: LEMBRAR O TAMANHO PEDIDO, E DAR UM MEMSET()
    { uint c = DEDIPY_TEST_2_COUNT;
        while (c--) {

            dbg("COUNTER %u", c);

            u64 size;

            void** last = NULL;
            void** new;

            // NOTE: cuidato com o dedipy_realloc(), só podemos realocar o ponteiro atual, e não os anteriores
            while ((new = dedipy_malloc((size = sizeof(void**) + TEST_SIZE(c))))) {
                if (size & 1)
                    memset(new, size & 0xFF, size);
                if (RANDOM(c) % 10 == 0)
                    new = dedipy_realloc(new, TEST_SIZE(c)) ?: new;
                // TODO: FIXME: uma porcentagem, dar dedipy_free() aqui ao invés de incluir na lista
                *new = last;
                last = new;
            }

            while (last) {
                if (RANDOM(c) % 10 == 0)
                    last = dedipy_realloc(last, sizeof(void**) + TEST_SIZE(c)) ?: last;
                void** old = *last;
                dedipy_free(last);
                last = old;
            }
        }
    }

    // TODO: FIXME: OUTRO TESTE: aloca todos com 4GB, depois com 1GB, até 8 bytes,
    // u64 size = 1ULL << 63;
    // do {
    //      while((this = dedipy_malloc(size)))
    //          ...
    //      while (last) {
    //          dedipy_free()
    //      }
    // } while ((size >>= 1));

#if 0
    dbg("RECEIVING SELF");

    char received[65536];

    const uint receivedSize = SELF_GET(received, sizeof(received));

    dbg("RECEIVED %u BYTES:", receivedSize);

    write(STDOUT_FILENO, received, receivedSize);
#endif

    dbg("TEST DONE");
#endif
}

//int posix_memalign(void **memptr, size_t alignment, size_t size) {
//void *aligned_alloc(size_t alignment, size_t size) {
//void *valloc(size_t size) {
//void *memalign(size_t alignment, size_t size) {
//void *pvalloc(size_t size) {
//int brk(void *addr) {
//void *sbrk(intptr_t increment) ;
//void sync (void) {
//int syncfs (int fd) {
