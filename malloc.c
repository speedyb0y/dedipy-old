/*

  BUFFER_INFO                       BUFFER_INFO + processSize
 |___________________________________|
 | INFO | ROOTS | L |    CHUNKS  | R |

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

*/

#define VERIFY 1

#define _GNU_SOURCE 1
#define _LARGEFILE64_SOURCE 64
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>

#include "util.h"

#include "ms.h"

//
#define CLEAR_CHUNK(c, s) ({ })

//
#define _CHUNK_USED_SIZE(dataSize) (_CHUNK_HDR_SIZE_USED + ((((u64)(dataSize) + 7ULL) & ~0b111ULL)) + _CHUNK_SIZE2_SIZE)

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define CHUNK_SIZE_FROM_DATA_SIZE(dataSize) (_CHUNK_USED_SIZE(dataSize) > CHUNK_SIZE_MIN ? _CHUNK_USED_SIZE(dataSize) : CHUNK_SIZE_MIN)

#define BUFFER_INFO     ((BufferInfo*)(BUFFER))
#define BUFFER_ROOTS     ((void**)(BUFFER + sizeof(BufferInfo)))
#define BUFFER_ROOTS_LST ((void**)(BUFFER + sizeof(BufferInfo) + sizeof(void*)*(ROOTS_N - 1)))
#define BUFFER_ROOTS_LMT ((void**)(BUFFER + sizeof(BufferInfo) + sizeof(void*)*(ROOTS_N    )))
#define BUFFER_L          (*(u64*)(BUFFER + sizeof(BufferInfo) + sizeof(void*)*(ROOTS_N    )))
#define BUFFER_CHUNK0             (BUFFER + sizeof(BufferInfo) + sizeof(void*)*(ROOTS_N    ) + sizeof(u64))
#define BUFFER_R(size)    (*(u64*)(BUFFER + (size) - sizeof(u64)))
#define BUFFER_LMT                (BUFFER + BUFFER_INFO->size)

#define ROOTS_SIZES_FST 32ULL
#define ROOTS_SIZES_LST 31665934879948ULL
#define ROOTS_SIZES_LMT 35184372088832ULL

#define ROOTS_N 200

#define N_START 5
#define X_DIVISOR 5
#define X_SALT 1
#define X_LAST 5

#if DEBUG || VERIFY
static const u64 ROOTS_SIZES[] = {0x20ULL,0x26ULL,0x2CULL,0x33ULL,0x39ULL,0x40ULL,0x4CULL,0x59ULL,0x66ULL,0x73ULL,0x80ULL,0x99ULL,0xB3ULL,0xCCULL,0xE6ULL,0x100ULL,0x133ULL,0x166ULL,0x199ULL,0x1CCULL,0x200ULL,0x266ULL,0x2CCULL,0x333ULL,0x399ULL,0x400ULL,0x4CCULL,0x599ULL,0x666ULL,0x733ULL,0x800ULL,0x999ULL,0xB33ULL,0xCCCULL,0xE66ULL,0x1000ULL,0x1333ULL,0x1666ULL,0x1999ULL,0x1CCCULL,0x2000ULL,0x2666ULL,0x2CCCULL,0x3333ULL,0x3999ULL,0x4000ULL,0x4CCCULL,0x5999ULL,0x6666ULL,0x7333ULL,0x8000ULL,0x9999ULL,0xB333ULL,0xCCCCULL,0xE666ULL,0x10000ULL,0x13333ULL,0x16666ULL,0x19999ULL,0x1CCCCULL,0x20000ULL,0x26666ULL,0x2CCCCULL,0x33333ULL,0x39999ULL,0x40000ULL,0x4CCCCULL,0x59999ULL,0x66666ULL,0x73333ULL,0x80000ULL,0x99999ULL,0xB3333ULL,0xCCCCCULL,0xE6666ULL,0x100000ULL,0x133333ULL,0x166666ULL,0x199999ULL,0x1CCCCCULL,0x200000ULL,0x266666ULL,0x2CCCCCULL,0x333333ULL,0x399999ULL,0x400000ULL,0x4CCCCCULL,0x599999ULL,0x666666ULL,0x733333ULL,0x800000ULL,0x999999ULL,0xB33333ULL,0xCCCCCCULL,0xE66666ULL,0x1000000ULL,0x1333333ULL,0x1666666ULL,0x1999999ULL,0x1CCCCCCULL,0x2000000ULL,0x2666666ULL,0x2CCCCCCULL,0x3333333ULL,0x3999999ULL,0x4000000ULL,0x4CCCCCCULL,0x5999999ULL,0x6666666ULL,0x7333333ULL,0x8000000ULL,0x9999999ULL,0xB333333ULL,0xCCCCCCCULL,0xE666666ULL,0x10000000ULL,0x13333333ULL,0x16666666ULL,0x19999999ULL,0x1CCCCCCCULL,0x20000000ULL,0x26666666ULL,0x2CCCCCCCULL,0x33333333ULL,0x39999999ULL,0x40000000ULL,0x4CCCCCCCULL,0x59999999ULL,0x66666666ULL,0x73333333ULL,0x80000000ULL,0x99999999ULL,0xB3333333ULL,0xCCCCCCCCULL,0xE6666666ULL,0x100000000ULL,0x133333333ULL,0x166666666ULL,0x199999999ULL,0x1CCCCCCCCULL,0x200000000ULL,0x266666666ULL,0x2CCCCCCCCULL,0x333333333ULL,0x399999999ULL,0x400000000ULL,0x4CCCCCCCCULL,0x599999999ULL,0x666666666ULL,0x733333333ULL,0x800000000ULL,0x999999999ULL,0xB33333333ULL,0xCCCCCCCCCULL,0xE66666666ULL,0x1000000000ULL,0x1333333333ULL,0x1666666666ULL,0x1999999999ULL,0x1CCCCCCCCCULL,0x2000000000ULL,0x2666666666ULL,0x2CCCCCCCCCULL,0x3333333333ULL,0x3999999999ULL,0x4000000000ULL,0x4CCCCCCCCCULL,0x5999999999ULL,0x6666666666ULL,0x7333333333ULL,0x8000000000ULL,0x9999999999ULL,0xB333333333ULL,0xCCCCCCCCCCULL,0xE666666666ULL,0x10000000000ULL,0x13333333333ULL,0x16666666666ULL,0x19999999999ULL,0x1CCCCCCCCCCULL,0x20000000000ULL,0x26666666666ULL,0x2CCCCCCCCCCULL,0x33333333333ULL,0x39999999999ULL,0x40000000000ULL,0x4CCCCCCCCCCULL,0x59999999999ULL,0x66666666666ULL,0x73333333333ULL,0x80000000000ULL,0x99999999999ULL,0xB3333333333ULL,0xCCCCCCCCCCCULL,0xE6666666666ULL,0x100000000000ULL,0x133333333333ULL,0x166666666666ULL,0x199999999999ULL,0x1CCCCCCCCCCCULL,0x200000000000ULL};
#endif

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

// TEM QUE SER USED, PARA QUE NUNCA TENTEMOS ACESSÁ-LO NA HORA DE JOIN LEFT/RIGHT
// O MENOR TAMANHO POSSÍVEL; SE TENTARMOS ACESSAR ELE, VAMOS DESCOBRIR COM O ASSERT SIZE >= MIN
#define BUFFER_LR_VALUE 2ULL // ass

typedef u64 ChunkSize;

static inline u64 UNKN_SIZE_ISFREE_DECODE (const ChunkSize s) {
    return s & 1ULL;
}

// NAO SEI QUAL É, NAO PODE USAR ASSERT DE FLAG
static inline u64 UNKN_SIZE_DECODE (ChunkSize s) {
    s &= ~1ULL;
    ASSERT ( CHUNK_SIZE_MIN <= s || s == BUFFER_LR_VALUE ); //  BUFFER_LR_VALUE sem a flag de used
    ASSERT ( CHUNK_SIZE_MAX >= s );
    ASSERT ( s % 8 == 0 || s == BUFFER_LR_VALUE );
    return s;
}

static inline u64 FREE_SIZE_DECODE (ChunkSize s) {
    ASSERT ( (s & 1ULL) == 1ULL );
    s &= ~1ULL;
    ASSERT ( CHUNK_SIZE_MIN <= s );
    ASSERT ( CHUNK_SIZE_MAX >  s );
    ASSERT ( s % 8 == 0 );
    return s;
}

static inline ChunkSize FREE_SIZE_ENCODE (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s >= CHUNK_SIZE_MIN );
    ASSERT ( s <= CHUNK_SIZE_MAX );
    ASSERT ( s % 8 == 0 );
    return s | 1ULL;
}

static inline u64 USED_SIZE_DECODE (const ChunkSize s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s % 8 == 0 && CHUNK_SIZE_MIN <= s && s < CHUNK_SIZE_MAX );
    return s;
}

static inline ChunkSize USED_SIZE_ENCODE (const u64 s) {
    ASSERT ( (s & 1ULL) == 0ULL );
    ASSERT ( s % 8 == 0 && CHUNK_SIZE_MIN <= s && s < CHUNK_SIZE_MAX );
    return s;
}

#define UNKN_SIZE_DECODE(s) UNKN_SIZE_DECODE(FORCE_U64(s))
#define FREE_SIZE_DECODE(s) FREE_SIZE_DECODE(FORCE_U64(s))
#define FREE_SIZE_ENCODE(s) FREE_SIZE_ENCODE(FORCE_U64(s))
#define USED_SIZE_DECODE(s) USED_SIZE_DECODE(FORCE_U64(s))
#define USED_SIZE_ENCODE(s) USED_SIZE_ENCODE(FORCE_U64(s))

#define __CHUNK_PTR(chunk)  ((void**)((chunk) + _CHUNK_PTR_OFFSET))
#define __CHUNK_NEXT(chunk) ((void**)((chunk) + _CHUNK_NEXT_OFFSET))

#define _CHUNK_SIZE(chunk) (*(ChunkSize*)(chunk))
#define _CHUNK_SIZE2(chunk, size) (*(ChunkSize*)((chunk) + (size) - _CHUNK_SIZE2_SIZE))
#define _CHUNK_PTR(chunk)  (*__CHUNK_PTR (chunk))
#define _CHUNK_NEXT(chunk) (*__CHUNK_NEXT(chunk))

static inline int    CHUNK_UNKN_IS_FREE  (const void* const chunk                  ) { return UNKN_SIZE_ISFREE_DECODE(_CHUNK_SIZE(chunk)); }
static inline u64    CHUNK_UNKN_LD_SIZE  (const void* const chunk                  ) { return UNKN_SIZE_DECODE(_CHUNK_SIZE(chunk)); }
static inline u64    CHUNK_UNKN_LD_SIZE2 (      void* const chunk, const u64 size  ) { return UNKN_SIZE_DECODE(_CHUNK_SIZE2(chunk, size)); }
static inline void   CHUNK_USED_ST_SIZE  (      void* const chunk, const u64 size  ) { _CHUNK_SIZE(chunk) = USED_SIZE_ENCODE(size); }
static inline u64    CHUNK_USED_LD_SIZE  (const void* const chunk                  ) { return USED_SIZE_DECODE(_CHUNK_SIZE(chunk)); };
static inline void*  CHUNK_USED_FROM_DATA(      void* const data                   ) { return data - _CHUNK_HDR_SIZE_USED; }
static inline void*  CHUNK_USED_DATA     (      void* const chunk                  ) { return chunk + _CHUNK_HDR_SIZE_USED; }
static inline u64    CHUNK_USED_DATA_SIZE(                         const u64 size  ) { return size - _CHUNK_HDR_SIZE_USED - _CHUNK_SIZE2_SIZE; } // DADO UM CHUNK USED DE TAL TAMANHO, CALCULA O TAMANHO DOS DADOS
static inline void   CHUNK_USED_ST_SIZE2 (      void* const chunk, const u64 size  ) { _CHUNK_SIZE2(chunk, size) = USED_SIZE_ENCODE(size); }
static inline u64    CHUNK_USED_LD_SIZE2 (const void* const chunk, const u64 size  ) { return USED_SIZE_DECODE(_CHUNK_SIZE2(chunk, size)); }
static inline void   CHUNK_FREE_ST_SIZE  (      void* const chunk, const u64 size  ) {        _CHUNK_SIZE(chunk) = FREE_SIZE_ENCODE(size); }
static inline u64    CHUNK_FREE_LD_SIZE  (const void* const chunk                  ) { return FREE_SIZE_DECODE(_CHUNK_SIZE(chunk)); }
static inline void   CHUNK_FREE_ST_PTR   (      void* const chunk, void** const ptr) {         _CHUNK_PTR(chunk) = ptr; }
static inline void** CHUNK_FREE_LD_PTR   (const void* const chunk                  ) { return  _CHUNK_PTR(chunk); }
static inline void   CHUNK_FREE_ST_NEXT  (      void* const chunk, void* const next) {         _CHUNK_NEXT(chunk) = next; }
static inline void*  CHUNK_FREE_LD_NEXT  (const void* const chunk                  ) { return  _CHUNK_NEXT(chunk); }
static inline void** CHUNK_FREE_LD_NEXT_ (const void* const chunk                  ) { return __CHUNK_NEXT(chunk); }
static inline void   CHUNK_FREE_ST_SIZE2 (      void* const chunk, const u64 size  ) {         _CHUNK_SIZE2(chunk, size) = FREE_SIZE_ENCODE(size); }
static inline u64    CHUNK_FREE_LD_SIZE2 (const void* const chunk, const u64 size  ) { return FREE_SIZE_DECODE(_CHUNK_SIZE2(chunk, size)); }

static inline void* CHUNK_LEFT  (void* const chunk          ) { return chunk - UNKN_SIZE_DECODE(*(ChunkSize*)(chunk - _CHUNK_SIZE2_SIZE)); }
static inline void* CHUNK_RIGHT (void* const chunk, u64 size) { return chunk + size; }

// BASEADO NO TAMANHO TOTAL DO BUFFER
#define BUFFER_CHUNK0_SIZE(size) ((size) - sizeof(BufferInfo) - ROOTS_N*sizeof(void*) - sizeof(u64) - sizeof(u64))

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
// SOMENTE A LIB USA ISSO, ENTAO NAO PRECISA DE TANTAS CHEGAGENS?
static inline void** _CHUNK_PTR_ROOT_PUT (const u64 size) {

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

static inline void** _CHUNK_PTR_ROOT_GET (const u64 size) {

    uint n = N_START; // TODO: FIXME: ambos devem ser uint mesmo?
    uint x = 0;

    // CONTINUA ANDANDO ENQUANTO O PROMETIDO NÃO SATISFAZER O PEDIDO
    while ((((1ULL << n) + ((1ULL << n) * x)/X_DIVISOR)) < size)
        n += (x = (x + X_SALT) % X_LAST) == 0;

    // voltasGrandes*(voltasMiniN) + voltasMini
    return &BUFFER_ROOTS[(n - N_START)*(X_LAST/X_SALT) + x/X_SALT];
}

static inline void** CHUNK_PTR_ROOT_GET (const u64 size) {

    // JÁ TEM QUE TER TRATADO
    ASSERT ( size >= CHUNK_SIZE_MIN );
    ASSERT ( size <= CHUNK_SIZE_MAX );
    ASSERT ( size % 8 == 0 );

    void **ptr = _CHUNK_PTR_ROOT_GET(size);

    ASSERT (ptr >= BUFFER_ROOTS );
    ASSERT (ptr <= BUFFER_ROOTS_LST );

    // O ULTIMO DEVE SER PRESERVADO COMO NULL
    ASSERT (*BUFFER_ROOTS_LST == NULL);

    return ptr;
}

static inline void** CHUNK_PTR_ROOT_PUT (const u64 size) {

    // JÁ TEM QUE TER TRATADO
    ASSERT ( size >= CHUNK_SIZE_MIN );
    ASSERT ( size <= CHUNK_SIZE_MAX );
    ASSERT ( size % 8 == 0 );

    void **ptr = _CHUNK_PTR_ROOT_PUT(size);

    ASSERT (ptr >= BUFFER_ROOTS );
    ASSERT (ptr <= BUFFER_ROOTS_LST);

    // O ULTIMO DEVE SER PRESERVADO COMO NULL
    ASSERT (*BUFFER_ROOTS_LST == NULL);

    return ptr;
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
    DBGPRINTF3("UNLINK FREE CHUNK BX%llX SIZE %llu PTR BX%llX NEXT BX%llX", BOFFSET(chunk), (uintll)CHUNK_FREE_LD_SIZE(chunk), BOFFSET(CHUNK_FREE_LD_PTR(chunk)), BOFFSET(CHUNK_FREE_LD_NEXT(chunk)));
    ASSERT (CHUNK_UNKN_IS_FREE(chunk)); // TEM QUE SER UM FREE
    if (CHUNK_FREE_LD_NEXT(chunk)) { // SE TEM UM NEXT
        ASSERT (CHUNK_UNKN_IS_FREE(CHUNK_FREE_LD_NEXT(chunk))); // ELE TAMBÉM TEM QUE SER UM FREE
        CHUNK_FREE_ST_PTR(CHUNK_FREE_LD_NEXT(chunk), CHUNK_FREE_LD_PTR(chunk)); // ELE ASSUME O NOSSO PTR
    } // O NOSSO PTR ASSUME O NOSSO NEXT
    *CHUNK_FREE_LD_PTR(chunk) = CHUNK_FREE_LD_NEXT(chunk);
    CLEAR_CHUNK(chunk, CHUNK_FREE_LD_SIZE(chunk));
    DBGPRINTF3("UNLINK FREE CHUNK BX%llX - DONE", BOFFSET(chunk));
}

// MUST HAVE SAME ALIGNMENTS AS MALLOC! :/ @_@
static void ms_verify (void) {
#if VERIFY
    // LEFT/RIGHT
    ASSERT ( BUFFER_L                    == BUFFER_LR_VALUE );
    ASSERT ( BUFFER_R(BUFFER_INFO->size) == BUFFER_LR_VALUE );

    ASSERT( (BUFFER + BUFFER_INFO->size) == BUFFER_LMT );

    u64 totalFree = 0;
    u64 totalUsed = 0;

    void* chunk = BUFFER_CHUNK0;

    while (chunk != &BUFFER_R(BUFFER_INFO->size)) {
        if (CHUNK_UNKN_IS_FREE(chunk)) {
            DBGPRINTF3("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX PTR BX%llX NEXT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(CHUNK_LEFT(chunk)), BOFFSET(CHUNK_RIGHT(chunk, CHUNK_UNKN_LD_SIZE(chunk))), BOFFSET(CHUNK_FREE_LD_PTR(chunk)), BOFFSET(CHUNK_FREE_LD_NEXT(chunk)), (uintll)CHUNK_UNKN_LD_SIZE(chunk), !!CHUNK_UNKN_IS_FREE(chunk));
            ASSERT(*CHUNK_FREE_LD_PTR (chunk) == chunk);
            if (CHUNK_FREE_LD_NEXT(chunk)) {
                ASSERT(CHUNK_UNKN_IS_FREE(chunk));
                ASSERT(CHUNK_FREE_LD_PTR(CHUNK_FREE_LD_NEXT(chunk)) == CHUNK_FREE_LD_NEXT_(chunk));
            }
            ASSERT_ADDR_IN_BUFFER(CHUNK_FREE_LD_PTR(chunk));
            totalFree += CHUNK_FREE_LD_SIZE(chunk);
        } else {
            DBGPRINTF3("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(CHUNK_LEFT(chunk)), BOFFSET(CHUNK_RIGHT(chunk, CHUNK_UNKN_LD_SIZE(chunk))), (uintll)CHUNK_UNKN_LD_SIZE(chunk), !!CHUNK_UNKN_IS_FREE(chunk));
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

    DBGPRINTF3("-- TOTAL %llu ------", (uintll)total);

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

        DBGPRINTF3("FREE ROOT #%d CHUNK BX%llX", idx, BOFFSET(chunk));

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
#endif
}

// TODO: FIXME: WE WILL NEED A SIGNAL HANDLER

void ms_init (void) {

    DBGPRINTF("INITIALIZING");

    uintll start_ = 0;
    uintll size_ = 0;
    uintll started = 0;
    uintll code = 0;

    //
    if (sscanf(getenv("SLAVEPARAMS"), "%016llX" "%016llX" "%016llX" "%016llX", &start_, &size_, &started, &code) != 4 || start_ == 0 || size_ == 0)
        fatal("FAILED TO GET SLAVE PARAMS FROM ENVIRONMENT");

    // TODO: FIXME: mmap(2)
    const size_t size = size_;
    const off_t start = start_;

    ASSERT (size  == size_);
    ASSERT (start == start_);

    if (BUFFER != mmap(BUFFER_INFO, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_LOCKED | MAP_POPULATE, BUFFER_FD, start))
        fatal("MMAP FAILED");

#define DBG(fmt, ...) DBGPRINTF("SLAVE [%u] " fmt, BUFFER_INFO->id, ##__VA_ARGS__)

    if (BUFFER_INFO->cpu != sched_getcpu())
        fatal("CPU MISMATCH");

    if (BUFFER_INFO->started != started)
        fatal("STARTED MISMATCH");

    if (BUFFER_INFO->code != code)
        fatal("CODE MISMATCH");

    if (BUFFER_INFO->start != start)
        fatal("START MISMATCH");

    if (BUFFER_INFO->size != size)
        fatal("SIZE MISMATCH");

    if (BUFFER_INFO->pid == getpid()) {
        // ALREADY INITIALIZED

        char name[256];

        if (snprintf(name, sizeof(name), PROGNAME "#%u", (uint)BUFFER_INFO->id) < 2 || prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0))
            fatal("FAILED TO SET PROCESS NAME");

        // LEFT AND RIGHT
        BUFFER_L                    = BUFFER_LR_VALUE;
        BUFFER_R(BUFFER_INFO->size) = BUFFER_LR_VALUE;

        // THE INITIAL CHUNK
        // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O CHUNK_SIZE_MAX E ROOTS_SIZES_LST SÃO MAIORES DO QUE ELE
        CHUNK_FREE_FILL_AND_REGISTER(BUFFER_CHUNK0, BUFFER_CHUNK0_SIZE(BUFFER_INFO->size));

        BUFFER_INFO->initialized = 1;

    } elif (BUFFER_INFO->initialized != 1)
        fatal("NOT INITIALIZED");

    // TODO: FIXME: tentar dar malloc() e realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // CHUNK_SIZE_MAX
    // ROOTS_SIZES_LST
    // ROOTS_SIZES_LMT

    DBG("ROOTS_N %llu", (uintll)ROOTS_N);
    DBG("ROOTS_SIZES_FST %llu", (uintll)ROOTS_SIZES_FST);
    DBG("ROOTS_SIZES_LST %llu", (uintll)ROOTS_SIZES_LST);
    DBG("ROOTS_SIZES_LMT %llu", (uintll)ROOTS_SIZES_LMT);

    DBG("CHUNK_SIZE_MIN %llu", (uintll)CHUNK_SIZE_MIN);
    DBG("CHUNK_SIZE_MAX %llu", (uintll)CHUNK_SIZE_MAX);

    ASSERT (CHUNK_SIZE_MIN == ROOTS_SIZES_FST);
    ASSERT (CHUNK_SIZE_MAX <= ROOTS_SIZES_LST);
    ASSERT (CHUNK_SIZE_MAX <  ROOTS_SIZES_LMT);

    ASSERT (ROOTS_SIZES_FST < ROOTS_SIZES_LST);
    ASSERT (ROOTS_SIZES_LST < ROOTS_SIZES_LMT);

    ASSERT (CHUNK_PTR_ROOT_GET(CHUNK_SIZE_MIN) == (BUFFER_ROOTS));
    ASSERT (CHUNK_PTR_ROOT_GET(CHUNK_SIZE_MAX) == (BUFFER_ROOTS + (ROOTS_N - 1)));

    ASSERT (CHUNK_PTR_ROOT_PUT(CHUNK_SIZE_MIN) == (BUFFER_ROOTS));
    ASSERT (CHUNK_PTR_ROOT_PUT(CHUNK_SIZE_MAX) <= (BUFFER_ROOTS + (ROOTS_N - 1))); // COMO VAI ARREDONDAR PARA BAIXO, PEDIR O MÁXIMO PODE CAIR LOGO ANTES DO ÚLTIMO SLOT

    ASSERT (BUFFER_ROOTS_LMT == (BUFFER_ROOTS + ROOTS_N));

    DBG("BUFFER_INFO BX%llX",      BOFFSET(BUFFER_INFO));
    DBG("BUFFER_INFO->id %llu",    (uintll)BUFFER_INFO->id);
    DBG("BUFFER_INFO->pid %llu",   (uintll)BUFFER_INFO->pid);
    DBG("BUFFER_INFO->cpu %llu",   (uintll)BUFFER_INFO->cpu);
    DBG("BUFFER_INFO->start %llu", (uintll)BUFFER_INFO->start);
    DBG("BUFFER_INFO->size %llu",  (uintll)BUFFER_INFO->size);
    DBG("BUFFER_INFO->code %llu",  (uintll)BUFFER_INFO->code);

    DBG("BUFFER_ROOTS BX%llX",     BOFFSET(BUFFER_ROOTS));
    DBG("BUFFER_ROOTS_LMT BX%llX", BOFFSET(BUFFER_ROOTS_LMT));

    DBG(" BUFFER_L %llu",    (uintll)BUFFER_L);
    DBG("&BUFFER_L BX%llX", BOFFSET(&BUFFER_L));

    DBG(" BUFFER_CHUNK0 BX%llX", BOFFSET(BUFFER_CHUNK0));
    DBG(" BUFFER_CHUNK0 SIZE %llu",   (uintll)(CHUNK_FREE_LD_SIZE(BUFFER_CHUNK0)));
    DBG(" BUFFER_CHUNK0 PTR BX%llX",  BOFFSET( CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)));
    DBG("*BUFFER_CHUNK0 PTR BX%llX",  BOFFSET(*CHUNK_FREE_LD_PTR (BUFFER_CHUNK0)));
    DBG(" BUFFER_CHUNK0 NEXT BX%llX", BOFFSET( CHUNK_FREE_LD_NEXT(BUFFER_CHUNK0)));

    DBG(" BUFFER_R %llu",    (uintll)BUFFER_R(BUFFER_INFO->size));
    DBG("&BUFFER_R BX%llX", BOFFSET(&BUFFER_R(BUFFER_INFO->size)));

    DBG("BUFFER_LMT BX%llX", BOFFSET(BUFFER_LMT));

    DBG("---");

    ASSERT_ADDR_IN_BUFFER(BUFFER_CHUNK0);
    ASSERT_ADDR_IN_CHUNKS(BUFFER_CHUNK0);

    ASSERT ( sizeof(u64) == sizeof(off_t) );
    ASSERT ( sizeof(u64) == sizeof(size_t) );
    ASSERT ( sizeof(u64) == 8 );
    ASSERT ( sizeof(void*) == 8 );

    ms_verify();

    // TODO: FIXME: o malloc também tem que ser inicializado no master
}

void free (void* const data) {

    DBGPRINTF2("=== FREE(BX%llX) ========================================================================", BOFFSET(data));

    ms_verify();

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        void* chunk = CHUNK_USED_FROM_DATA(data);

        ASSERT(CHUNK_UNKN_IS_FREE(chunk) == 0);
        ASSERT(CHUNK_UNKN_LD_SIZE(chunk) <= CHUNK_SIZE_MAX);

        u64 size = CHUNK_USED_LD_SIZE(chunk);

        // JOIN WITH THE LEFT CHUNK
        void* const left = CHUNK_LEFT(chunk);

        if (CHUNK_UNKN_IS_FREE(left)) {
            size += CHUNK_FREE_LD_SIZE(left);
            chunk = left;
            CHUNK_FREE_REMOVE(chunk);
        }

        // JOIN WITH THE RIGHT CHUNK
        void* const right = CHUNK_RIGHT(chunk, size);

        if (CHUNK_UNKN_IS_FREE(right)) {
            size += CHUNK_FREE_LD_SIZE(right);
            CHUNK_FREE_REMOVE(right);
        }

        //
        CHUNK_FREE_FILL_AND_REGISTER(chunk, size);
    }
}

void* malloc (size_t size_) {

    //if (__builtin_expect_with_probability(initialized, 1, 0.99) == 0)
        //initialize();

    DBGPRINTF2("=== MALLOC(%llu) ========================================================================", (uintll)size_);

    ms_verify();

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 size = CHUNK_SIZE_FROM_DATA_SIZE(size_);

    //
    if (size > CHUNK_SIZE_MAX)
        return NULL;

    // PEGA UM LIVRE A SER USADO
    void* used;

    // ENCONTRA A PRIMEIRA LISTA LIVRE
    void** ptr = CHUNK_PTR_ROOT_GET(size);

    // LOGO APÓS O HEADS, HÁ O LEFT CHUNK, COM UM SIZE FAKE 1, PORTANTO ELE É NÃO-NULL, E VAI PARAR SE NAO TIVER MAIS CHUNKS LIVRES
    while ((used = *ptr) == NULL)
        ptr++;

    // SE
    if (ptr == BUFFER_ROOTS_LMT) {
        ASSERT ( used == (void*)BUFFER_LR_VALUE ); // TERÁ LIDO DESTA FORMA
        return NULL;
    }

    ASSERT(BUFFER_ROOTS <= ptr && ptr < BUFFER_ROOTS_LMT);
    ASSERT(CHUNK_UNKN_IS_FREE(used));
    //ASSERT(CHUNK_FREE(used)->next == NULL || CHUNK_FREE((CHUNK_FREE(used)->next))->ptr == &CHUNK_FREE(used)->next);
    ASSERT(CHUNK_SIZE_MIN <= CHUNK_FREE_LD_SIZE(used) && CHUNK_FREE_LD_SIZE(used) <= CHUNK_SIZE_MAX && (CHUNK_FREE_LD_SIZE(used) % 8) == 0);

    //ASSERT_ADDR_IN_CHUNKS(CHUNK_FREE(used)->next);

    // TAMANHO DESTE CHUNK FREE
    u64 usedSize = CHUNK_FREE_LD_SIZE(used);

    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = usedSize - size;

    // REMOVE ELE DE SUA LISTA, MESMO QUE SÓ PEGUEMOS UM PEDAÇO, VAI TER REALOCÁ-LO NA TREE
    CHUNK_FREE_REMOVE(used);

    // SE DER, CONSOME SÓ UMA PARTE, NO FINAL DELE
    if (CHUNK_SIZE_MIN <= freeSizeNew) {
        CHUNK_FREE_FILL_AND_REGISTER(used, freeSizeNew);
        used += freeSizeNew;
        usedSize = size;
    }

    CLEAR_CHUNK(used, usedSize);

    CHUNK_USED_ST_SIZE (used, usedSize);
    CHUNK_USED_ST_SIZE2(used, usedSize);

    return CHUNK_USED_DATA(used);
}

// If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
// If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.
void* calloc (size_t n, size_t size_) {

    DBGPRINTF2("MALLOC - CALLOC");

    const u64 size = (u64)n * (u64)size_;

    void* const data = malloc(size);

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
void* realloc (void* const data_, const size_t dataSizeNew_) {

    DBGPRINTF2("=== REALLOC(BX%llX, %llu) ========================================================================", BOFFSET(data_), (uintll)dataSizeNew_);

    ms_verify();

    if (data_ == NULL)
        return malloc(dataSizeNew_);

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 sizeNew = CHUNK_SIZE_FROM_DATA_SIZE(dataSizeNew_);

    //
    if (sizeNew > CHUNK_SIZE_MAX)
        return NULL;

    // FOI NOS PASSADO O DATA; VAI PARA O CHUNK
    void* const chunk = CHUNK_USED_FROM_DATA(data_);

    ASSERT(CHUNK_UNKN_IS_FREE(chunk) == 0);

    //
    u64 size = CHUNK_USED_LD_SIZE(chunk);

    DBGPRINTF2("WILL REALLOC CHUNK BX%llX SIZE %llu -> %llu", BOFFSET(chunk), (uintll)size, (uintll)sizeNew);

    if (size >= sizeNew) {
        // ELE SE AUTOFORNECE
        if ((size - sizeNew) < 64)
            // MAS NÃO VALE A PENA DIMINUIR
            return data_;
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return data_;
    }

    void* right = CHUNK_RIGHT(chunk, size);

    // SÓ SE FOR FREE E SUFICIENTE
    if (CHUNK_UNKN_IS_FREE(right)) {

        DBGPRINTF2("RIGHT CHUNK BX%llX IS FREE", BOFFSET(right));

        const u64 rightSize = CHUNK_FREE_LD_SIZE(right);
        // O QUANTO VAMOS TENTAR RETIRAR DA DIREITA
        const u64 sizeNeeded = sizeNew - size;

        if (rightSize >= sizeNeeded) {
            // DÁ SIM; VAMOS ALOCAR ARRANCANDO DA DIREITA

            // O TAMANHO NOVO DA DIREITA
            const u64 rightSizeNew = rightSize - sizeNeeded;
            DBGPRINTF2("UA UAU UAU");

            // REMOVE ELE DE SUA LISTA
            CHUNK_FREE_REMOVE(right);

            if (rightSizeNew >= CHUNK_SIZE_MIN) {
                // PEGA ESTE PEDAÇO DELE, PELA ESQUERDA
                size += sizeNeeded; // size -> sizeNew
                // MOVE O COMEÇO PARA A DIREITA
                right += sizeNeeded;
                // REESCREVE
                CHUNK_FREE_FILL_AND_REGISTER(right, rightSizeNew);
                DBGPRINTF2("RIGHT CONSUMED PARTIALLY; RIGHT CHUNK BX%llX SIZE %llu PTR BX%llX NEXT BX%llX", BOFFSET(right), (uintll)CHUNK_FREE_LD_SIZE(right), BOFFSET(CHUNK_FREE_LD_PTR(right)), BOFFSET(CHUNK_FREE_LD_NEXT(right)));
            } else { // CONSOME ELE POR INTEIRO
                size += rightSize; // size -> o que ja era + o free chunk
                DBGPRINTF2("RIGHT CONSUMED ENTIRELY");
            }

            // ESCREVE ELE
            CHUNK_USED_ST_SIZE (chunk, size);
            CHUNK_USED_ST_SIZE2(chunk, size);

            DBGPRINTF2("RETURNING CHUNK BX%llX SIZE %llu DATA BX%llX", BOFFSET(chunk), (uintll)CHUNK_USED_LD_SIZE(chunk), BOFFSET(CHUNK_USED_DATA(chunk)));

            return CHUNK_USED_DATA(chunk);
        }
    }

    DBGPRINTF2("RIGHT CHUNK BX%llX NOT USED; ALLOCATING A NEW ONE", BOFFSET(right));

    // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
    void* const data = malloc(dataSizeNew_);

    if (data) {
        // CONSEGUIU
        // COPIA DO CHUNK ORIGINAL
        DBGPRINTF2("COPYING DATA SIZE %llu BX%llX TO BX%llX", (uintll)CHUNK_USED_DATA_SIZE(size), BOFFSET(data_), BOFFSET(data));
        memcpy(data, data_, CHUNK_USED_DATA_SIZE(size));
        // LIBERA O CHUNK ORIGINAL
        free(CHUNK_USED_DATA(chunk));

        DBGPRINTF2("RETURNING CHUNK BX%llX SIZE %llu DATA BX%llX", BOFFSET(CHUNK_USED_FROM_DATA(data)), (uintll)CHUNK_USED_LD_SIZE(CHUNK_USED_FROM_DATA(data)), BOFFSET(CHUNK_USED_DATA(CHUNK_USED_FROM_DATA(data))));

        ASSERT(CHUNK_USED_DATA(CHUNK_USED_FROM_DATA(data)) == data);
    }

    DBGPRINTF2("RETURNING DATA BX%llX", BOFFSET(data));

    return data;
}

void* reallocarray (void *ptr, size_t nmemb, size_t size) {

    (void)ptr;
    (void)nmemb;
    (void)size;

    fatal("MALLOC - REALLOCARRAY");
}

void sync (void) {

}

int syncfs (int fd) {

    (void)fd;

    return 0;
}

//  começa depois disso
//   usar = sizeof(Buffer) + 8 + BUFF->mallocSize + 8;
// le todos os seus de /proc/self/maps
//      if ((usar + ORIGINALSIZE) > BUFFER_LMT)
//            abort();
//      pwrite(BUFFER_FD, ORIGINALADDR, ORIGINALSIZE, usar);
//   agora remapeia
//      mmap(ORIGINALADDR, ORIGINALSIZE, prot, flags, BUFFER_FD, usar);
//      usar += ORIGINALSIZE;
