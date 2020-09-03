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

#define BUFF_ROOTS   ((chunk_s**)    (buff ))
#define BUFF_L       ((chunk_size_t*)(buff + ROOTS_N*sizeof(void*) ))
#define BUFF_CHUNKS  ((chunk_s*     )(buff + ROOTS_N*sizeof(void*) + sizeof(chunk_size_t)))
#define BUFF_R       ((chunk_size_t*)(buff + buffSize - sizeof(chunk_size_t) ))
#define BUFF_LMT                     (buff + buffSize )

#define BUFF_ROOTS_SIZE (ROOTS_N*sizeof(void*))
#define BUFF_L_SIZE sizeof(u64)
#define BUFF_CHUNKS_SIZE (buffSize - BUFF_ROOTS_SIZE - BUFF_L_SIZE - BUFF_R_SIZE) // É TODO O BUFFER RETIRANDO O RESTANTE
#define BUFF_R_SIZE sizeof(u64)

// FOR DEBUGGING
#define BOFFSET(x) ((x) ?  ((uintll)((void*)(x) - (void*)BUFF_ADDR)) : 0ULL)

typedef u64 chunk_size_t;

typedef struct chunk_s chunk_s;

struct chunk_s {
    chunk_size_t size;
    chunk_s** ptr; // SÓ TEM NO USED
    chunk_s* next; // SÓ TEM NO USED
};

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

static inline void c_clear(chunk_s* const c, const u64 s) {
#if 0
    memset(c, 0, s);
#endif
}

#define CHUNK_ALIGNMENT 8

// TODO: FIXME: dar assert para que fique alinhado a isso no used
#define DATA_ALIGNMENT 8

//
#define C_USED_SIZE__(dataSize) (_C_HDR_SIZE_USED + ((((u64)(dataSize) + 7ULL) & ~0b111ULL)) + sizeof(chunk_size_t))

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define C_SIZE_FROM_DATA_SIZE(dataSize) (C_USED_SIZE__(dataSize) > C_SIZE_MIN ? C_USED_SIZE__(dataSize) : C_SIZE_MIN)

#include "python-dedipy-defs.h"

//  C_SIZE_MIN / C_SIZE_MAX
// FREE: SIZE + PTR + NEXT + ... + SIZE2
// USED: SIZE + DATA...          + SIZE2
// ----- NOTE: o máximo deve ser <= último
// ----- NOTE: estes limites tem que considerar o alinhamento
// ----- NOTE: cuidado com valores grandes, pois ao serem somados com os endereços, haverão overflows
//              REGRA GERAL: (buff + 4*ROOTS_SIZES_LMT) < (1 << 63)   <--- testar no python, pois se ao verificar tiver overflow, não adiantará nada =]

// TEM DE SER NÃO 0, PARA QUE SEJA LIDO COMO NÃO NULL SE ULTRAPASSAR O OS ROOTS
// MAS TAMBÉM NÃO PODE SER EQUIVALENTE A UM PONTEIRO PARA UM CHUNK
// TEM QUE SER USED, PARA QUE NUNCA TENTEMOS ACESSÁ-LO NA HORA DE JOIN LEFT/RIGHT
// O MENOR TAMANHO POSSÍVEL; SE TENTARMOS ACESSAR ELE, VAMOS DESCOBRIR COM O assert SIZE >= MIN
#define BUFF_LR_VALUE 2ULL // ass

#define C_SIZE2(c, s) (*(chunk_size_t*)((void*)(c) + (s) - sizeof(chunk_size_t)))

static inline u64 c_size_decode_is_free (const chunk_size_t s) {
    return s & 1ULL;
}

// NAO SEI QUAL É, NAO PODE USAR assert DE FLAG
static inline u64 c_size_decode (chunk_size_t s) {
    s &= ~1ULL;
    assert ( C_SIZE_MIN <= s || s == BUFF_LR_VALUE ); //  BUFF_LR_VALUE sem a flag de used
    assert ( C_SIZE_MAX >= s );
    assert ( s % 8 == 0 || s == BUFF_LR_VALUE );
    return s;
}

static inline u64 c_size_decode_free (chunk_size_t s) {
    assert ( (s & 1ULL) == 1ULL );
    s &= ~1ULL;
    assert ( C_SIZE_MIN <= s );
    assert ( C_SIZE_MAX >  s );
    assert ( s % 8 == 0 );
    return s;
}

static inline u64 c_size_decode_used (const chunk_size_t s) {
    assert ( (s & 1ULL) == 0ULL );
    assert ( s % 8 == 0 );
    assert ( s >= C_SIZE_MIN );
    assert ( s <= C_SIZE_MAX );
    return s;
}

static inline chunk_size_t c_size_encode_free (const u64 s) {
    assert ( (s & 1ULL) == 0ULL );
    assert ( s >= C_SIZE_MIN );
    assert ( s <= C_SIZE_MAX );
    assert ( s % 8 == 0 );
    return s | 1ULL;
}

static inline chunk_size_t c_size_encode_used (const u64 s) {
    assert ( (s & 1ULL) == 0ULL );
    assert ( s % 8 == 0 );
    assert ( s >= C_SIZE_MIN );
    assert ( s <= C_SIZE_MAX );
    return s;
}

static inline chunk_s* c_left (const chunk_s* const chunk) {
    return (chunk_s*)((void*)chunk - c_size_decode(*(chunk_size_t*)((void*)chunk - sizeof(chunk_size_t))));
}

static inline chunk_s* c_right (const chunk_s* const chunk, u64 size) {
    return (chunk_s*)((void*)chunk + size);
}

static inline void assert_in_buff (const void* const addr, const u64 size) {
    assert ( in_mem(addr, size, buff, buffSize) );
}

static inline void assert_in_chunks (const void* const addr, const u64 size) {
    assert ( in_mem(addr, size, BUFF_CHUNKS, BUFF_CHUNKS_SIZE) );
}

static inline void assert_c_size (const u64 s) {
    assert ( is_from_to(C_SIZE_MIN, s, C_SIZE_MAX) );
    assert_aligned ( (void*)s , CHUNK_ALIGNMENT );
}

// OS DOIS DEVEM SER SETADOS JUNTOS
static inline void assert_c_sizes (const chunk_s* const c) {
    assert ( is_from_to(C_SIZE_MIN, c_size_decode(c->size), C_SIZE_MAX) );
    assert ( c->size == C_SIZE2(c, c_size_decode(c->size)) );
}

static inline void assert_c_ptr (const chunk_s* const c) {
    assert_in_buff ( c->ptr, sizeof(*c->ptr) );
    assert (  c->ptr );
    assert ( *c->ptr == c );
}

static inline void assert_c_next (const chunk_s* const c) {
    if (c->next) {
        assert_aligned ( c->next, 8 );
        assert_in_chunks ( c->next, c->next->size );
        assert ( &c->next == c->next->ptr );
    }
}

static inline void assert_root (const chunk_s* const root) {
    assert_is_element_of_array (root, BUFF_ROOTS, ROOTS_N) ;
}

#define assert_c_is_free(c) assert ( c_is_free(c) )
#define assert_c_is_used(c) assert ( c_is_used(c) )
#define assert_c_data(c, d) assert ( c_data(c) == d )

#define assert_c(c) (c_is_free(c) ? assert_c_free(c) : assert_c_used(c))
#define assert_c_used(c) (assert_in_chunks(c, c_size_decode_used(c->size)) , assert_c_is_used(c) , assert_c_size(c_size_decode_used(c->size)) , assert_c_sizes(c))
#define assert_c_free(c) (assert_in_chunks(c, c_size_decode_free(c->size)) , assert_c_is_free(c) , assert_c_size(c_size_decode_free(c->size)) , assert_c_sizes(c) , assert_c_ptr(c) , assert_c_next(c) )

//assert(C_FREE(used)->next == NULL || C_FREE((C_FREE(used)->next))->ptr == &C_FREE(used)->next);
//assert ( C_SIZE_MIN <= f_get_size(used) && f_get_size(used) <= C_SIZE_MAX && (f_get_size(used) % 8) == 0 );

static inline bool c_is_free (const chunk_s* const c)
    { return c_size_decode_is_free(c->size); }

static inline bool c_is_used (const chunk_s* const c)
    { return ! c_size_decode_is_free(c->size); }

static inline void* c_data (chunk_s* const c) {
    void* const data = (void*)c + sizeof(chunk_size_t);
    assert_c_used ( c );
    assert_c_data ( c, data );
    return data;
}

static inline chunk_s* c_from_data (void* const data) {
    chunk_s* c = data - sizeof(chunk_size_t);
    assert_c_used ( c );
    assert_c_data ( c, data );
    return c;
}

static inline u64 u_data_size (const u64 size) {
    assert_c_size(size);
    return size - sizeof(chunk_s) - sizeof(chunk_size_t);
}

// TODO: FIXME: outra verificação, completa
// assert_c_free(c)
// assert_c_used(c)
// assert_c(c) o chunk é válido, levando em consideração qual tipo é

// ESCOLHE O PRIMEIRO PTR
// BASEADO NESTE SIZE, SELECINAR UM PTR
// A PARTIR DESTE PTR É GARANTIDO QUE TODOS OS CHUNKS TENHAM ESTE TAMANHO

// PARA DEIXAR MAIS SIMPLES/MENOS INSTRUCOES
// - O LAST TEM QUE SER UM TAMANHO TAO GRANDE QUE JAMAIS SOLICITAREMOS
// f_ptr_root_get() na hora de pegar, usar o ANTEPENULTIMO como limite????
//  e o que mais????

// SE SIZE < ROOTS_SIZES_0, ENTÃO NÃO HAVERIA ONDE SER COLOCADO
//      NOTE: C_SIZE_MIN >= ROOTS_SIZES_0, ENTÃO size >= ROOTS_SIZES_0
// É RESPONSABILIDADE DO CALLER TER CERTEZA DE QUE size <= C_SIZE_MAX
//      NOTE: C_SIZE_MAX <= ROOTS_SIZES_N, ENTÃO size <= ROOTS_SIZES_N
//      NOTE: size <= LAST, PARA QUE HAJA UM INDEX QUE GARANTA

// QUEREMOS COLOCAR UM FREE
// SOMENTE A LIB USA ISSO, ENTAO NAO PRECISA DE TANTAS CHEGAGENS?
static inline chunk_s** root_put_ptr (u64 size) {

    assert_c_size(size);

    uint idx = 0;
    uint e = ROOT_EXP;
    uint X = ROOT_X;
    uint x = 0;

    size -= ROOT_BASE;

    // (1 << e) --> (2^e)
    // CONTINUA ANDANDO ENQUANTO PROVIDENCIARMOS TANTO
    // TODO: FIXME: fazer de um jeito que va subtraindo, ao inves de recomputar esse expoente toda hora
    while ((((1ULL << e) + ((1ULL << e) * x)/X)) <= size) {
        u64 foi = (x = (x + 1) % X) == 0;
        e += foi;
        foi *= X;
        X += foi/ROOT_X_ACCEL;
        X += foi/ROOT_X_ACCEL2;
        X += foi/ROOT_X_ACCEL3;
        idx++;
    }

    // O ATUAL NÃO PROVIDENCIAMOS, ENTÃO RETIRA 1
    idx--;

    assert(idx < ROOTS_N);

    return BUFF_ROOTS + idx;
}

static inline chunk_s** root_get_ptr (u64 size) {

    assert_c_size(size);

    uint idx = 0;
    uint e = ROOT_EXP;
    uint X = ROOT_X;
    uint x = 0;

    size -= ROOT_BASE;

    // CONTINUA ANDANDO ENQUANTO O PROMETIDO NÃO SATISFAZER O PEDIDO
    while ((((1ULL << e) + ((1ULL << e) * x)/X)) < size) {
        u64 foi = (x = (x + 1) % X) == 0;
        e += foi;
        foi *= X;
        X += foi/ROOT_X_ACCEL;
        X += foi/ROOT_X_ACCEL2;
        X += foi/ROOT_X_ACCEL3;
        idx++;
    }

    assert(idx < ROOTS_N);

    return BUFF_ROOTS + idx;
}

static inline void c_free_fill_and_register (chunk_s* const c, const u64 size) {

    assert_c_size(size);

    // FILL
    C_SIZE2(c, size) = c->size = c_size_encode_free(size);

    // REGISTER
    if ((c->next = *(c->ptr = root_put_ptr(size))))
        c->next->ptr = &c->next;

    assert_c_free(c);
}

// NOTE: VAI DEIXAR O PTR E O NEXT INVÁLIDOS
static inline void c_unlink (const chunk_s* const c) {

    assert_c_free(c);

    if ((*c->ptr = c->next)) {
        (*c->ptr)->ptr = c->ptr;
        assert_c_free(c->next);
    }
}

// MUST HAVE SAME ALIGNMENTS AS MALLOC! :/ @_@
static void VERIFY (void) {

    // ROOTS
    assert_in_buff ( BUFF_ROOTS, BUFF_ROOTS_SIZE );

    // CHUNKS
    assert_in_buff   ( BUFF_CHUNKS, c_size_decode_free(BUFF_CHUNKS->size) );
    assert_in_chunks ( BUFF_CHUNKS, c_size_decode_free(BUFF_CHUNKS->size) );

    // LEFT/RIGHT
    assert ( *BUFF_L == BUFF_LR_VALUE );
    assert ( *BUFF_R == BUFF_LR_VALUE );

    assert_in_buff (BUFF_L, sizeof(chunk_size_t) );
    assert_in_buff (BUFF_R, sizeof(chunk_size_t) );

    assert ( (buff + buffSize) == BUFF_LMT );

    assert ( (void*)(BUFF_ROOTS + ROOTS_N) == (void*)BUFF_L );

    assert (*BUFF_L == BUFF_LR_VALUE);
    assert (*BUFF_R == BUFF_LR_VALUE);
    assert (*BUFF_L == *BUFF_R);

    // O LEFT TEM QUE SER INTERPRETADO COMO NÃO NULL
    assert (BUFF_ROOTS[ROOTS_N]);

#if DEDIPY_VERIFY
    u64 totalFree = 0;
    u64 totalUsed = 0;

    const chunk_s* chunk = BUFF_CHUNKS;

    while (chunk != (chunk_s*)BUFF_R) { const u64 size = size;
        if (c_is_free(chunk)) {
            //dbg3("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX PTR BX%llX NEXT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(c_left(chunk)), BOFFSET(c_right(chunk, c_get_size(chunk))), BOFFSET(f_get_ptr(chunk)), BOFFSET(f_get_next(chunk)), (uintll)c_get_size(chunk), !!c_is_free(chunk));
            assert(*chunk->ptr == chunk);
            if (chunk->next) {
                assert_c_is_free ( chunk );
                assert ( chunk->next->ptr == &chunk->next );
            }
            assert_in_buff ( chunk->ptr, sizeof(chunk_s*) );
            totalFree += c_size_decode_free(chunk->size);
        } else {
            //dbg3("- CHUNK BX%llX LEFT BX%llX RIGHT BX%llX SIZE %llu ISFREE %d", BOFFSET(chunk), BOFFSET(c_left(chunk)), BOFFSET(c_right(chunk, c_get_size(chunk))), (uintll)c_get_size(chunk), !!c_is_free(chunk));
            totalUsed += c_size_decode_used(chunk->size);
        }
        //
        assert ( size == c_size_decode(C_SIZE2(chunk, size)) );
        assert ( c_right(chunk, size) == ((void*)chunk + size) );
        assert_in_chunks ( chunk, size );
        //
        chunk = c_right(chunk, size);
    }

    const u64 total = totalFree + totalUsed;

    dbg3("-- TOTAL %llu ------", (uintll)total);

    assert ( total == BUFF_CHUNKS_SIZE );

    // VERIFICA OS FREES
    uint idx = 0; chunk_s** ptrRoot = BUFF_ROOTS;

    do {
        const u64 fst = rootSizes[idx]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR NESTE ROOT
        const u64 lmt = rootSizes[idx + 1]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR SÓ NOS ROOTS DA FRENTE

        assert (fst < lmt);
        assert (fst >= C_SIZE_MIN);
        //assert (lmt <= C_SIZE_MAX || lmt == ROOTS_SIZES_LMT && idx == (ROOTS_N - 1)));

        chunk_s* const* ptr = ptrRoot;
        const chunk_s* chunk = *ptrRoot;

        dbg3("FREE ROOT #%d CHUNK BX%llX", idx, BOFFSET(chunk));

        while (chunk) { const u64 size = c_size_decode_free(chunk->size);
            assert_in_chunks ( chunk, size );
            assert_c_is_free ( chunk );
            assert ( size >= fst );
            assert ( size <  lmt );
            assert ( size == c_size_decode_free(chunk->size) );
            assert ( chunk->ptr == ptr );
            assert_c ( chunk );
            //assert ( f_ptr_root_get(f_get_size(chunk)) == ptrRoot );
            //assert ( root_put_ptr(f_get_size(chunk)) == ptrRoot ); QUAL DOS DOIS? :S e um <=/>= em umdeles
            //
            totalFree -= size;
            // PRÓXIMO
            chunk = *(ptr = &chunk->next);
        }

        ptrRoot++;

    } while (++idx != ROOTS_N);

    assert (ptrRoot == (BUFF_ROOTS + ROOTS_N));

    // CONFIRMA QUE VIU TODOS OS FREES VISTOS AO ANDAR TODOS OS CHUNKS
    assert (totalFree == 0);
#endif
}

void dedipy_free (void* const data) {

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        chunk_s* c = c_from_data(data);

        assert_c_used ( c );

        u64 size = c_size_decode_used(c->size);

        // JOIN WITH THE LEFT CHUNK
        void* const l = c_left(c);

        if (c_is_free(l)) {
            size += c_size_decode_free(l->size);
            c_unlink((c = l));
        }

        // JOIN WITH THE r CHUNK
        void* const r = c_right(c, size);

        if (c_is_free(r)) {
            size += c_size_decode_free(r->size);
            c_unlink(r);
        }

        //
        c_free_fill_and_register(c, size);

        assert_c_free ( c );
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
    *BUFF_L = BUFF_LR_VALUE;
    *BUFF_R = BUFF_LR_VALUE;

    // THE INITIAL CHUNK
    // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O C_SIZE_MAX E ROOTS_SIZES_N SÃO MAIORES DO QUE ELE
    c_free_fill_and_register(BUFF_CHUNKS, BUFF_CHUNKS_SIZE(buffSize));

    // TODO: FIXME: tentar dar malloc() e realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // C_SIZE_MAX
    // ROOTS_SIZES_N

    dbg("ROOTS_N %llu", (uintll)ROOTS_N);
    dbg("ROOTS_SIZES_0 %llu", (uintll)ROOTS_SIZES_0);
    dbg("ROOTS_SIZES_N %llu", (uintll)ROOTS_SIZES_N);

    dbg("C_SIZE_MIN %llu", (uintll)C_SIZE_MIN);
    dbg("C_SIZE_MAX %llu", (uintll)C_SIZE_MAX);

    assert (C_SIZE_MIN == ROOTS_SIZES_0);
    assert (C_SIZE_MAX <= ROOTS_SIZES_N);

    assert (ROOTS_SIZES_0 < ROOTS_SIZES_N);

    assert (f_ptr_root_get(C_SIZE_MIN) == (BUFF_ROOTS));
    assert (f_ptr_root_get(C_SIZE_MAX) == (BUFF_ROOTS + (ROOTS_N - 1)));

    assert (root_put_ptr(C_SIZE_MIN) == (BUFF_ROOTS));
    assert (root_put_ptr(C_SIZE_MAX) <= (BUFF_ROOTS + (ROOTS_N - 1))); // COMO VAI ARREDONDAR PARA BAIXO, PEDIR O MÁXIMO PODE CAIR LOGO ANTES DO ÚLTIMO SLOT

    dbg("CPU %u",                  (uint)cpu);
    dbg("PID %llu",                (uintll)pid);
    dbg("ID %u",                   (uint)id);
    dbg("CODE %llu",               (uintll)code);

    dbg("BUFF ADDR 0x%016llX",      (uintll)BUFF_ADDR);
    dbg("BUFF TOTAL SIZE %llu",     (uintll)buffTotal);
    dbg("BUFF START %llu",          (uintll)buffStart_);
    dbg("BUFF 0x%016llX",           BOFFSET(buff));
    dbg("BUFF_ROOTS BX%llX",        BOFFSET(BUFF_ROOTS));
    dbg("BUFF_L BX%llX",            BOFFSET(BUFF_L));
    dbg(" BUFF_CHUNKS BX%llX",      BOFFSET(BUFF_CHUNKS));
    dbg(" BUFF_CHUNKS SIZE %llu",   (uintll)(f_get_size(BUFF_CHUNKS)));
    dbg(" BUFF_CHUNKS PTR BX%llX",  BOFFSET( f_get_ptr (BUFF_CHUNKS)));
    dbg("*BUFF_CHUNKS PTR BX%llX",  BOFFSET(*f_get_ptr (BUFF_CHUNKS)));
    dbg(" BUFF_CHUNKS NEXT BX%llX", BOFFSET( f_get_next(BUFF_CHUNKS)));
    dbg("BUFF_R BX%llX",            BOFFSET(BUFF_R));
    dbg("BUFF_LMT BX%llX",          BOFFSET(BUFF_LMT));
    dbg("BUFF SIZE %llu",          (uintll)buffSize);

    dbg("*BUFF_L %llu",    (uintll)*BUFF_L);
    dbg("*BUFF_R %llu",    (uintll)*BUFF_R);

    assert ( sizeof(u64) == sizeof(off_t) );
    assert ( sizeof(u64) == sizeof(size_t) );
    assert ( sizeof(u64) == 8 );
    assert ( sizeof(void*) == 8 );

    VERIFY();

    dbg("OKAY!");
}

void* dedipy_malloc (const size_t size_) {

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 size = C_SIZE_FROM_DATA_SIZE(size_);

    assert_c_size(size);

    // SÓ O QUE PODE GARANTIR
    if (size > ROOTS_SIZES_N)
        return NULL;

    void* used; // PEGA UM LIVRE A SER USADO
    void** ptr = f_ptr_root_get(size); // ENCONTRA A PRIMEIRA LISTA LIVRE

    assert_root(ptr);

    // LOGO APÓS O HEADS, HÁ O LEFT CHUNK, COM UM SIZE FAKE 1, PORTANTO ELE É NÃO-NULL, E VAI PARAR SE NAO TIVER MAIS CHUNKS LIVRES
    while ((used = *ptr) == NULL)
        ptr++;

    if (used == BUFF_LR_VALUE)
        return NULL; // SAIU DOS ROOTS E NÃO ENCONTROU NENHUM

    assert_c_free ( used );

    u64 usedSize = f_get_size(used);
    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = usedSize - size;

    // REMOVE ELE DE SUA LISTA, MESMO QUE SÓ PEGUEMOS UM PEDAÇO, VAI TER REALOCÁ-LO NA TREE
    c_unlink(used);

    // SE DER, CONSOME SÓ UMA PARTE, NO FINAL DELE
    if (C_SIZE_MIN <= freeSizeNew) {
        c_free_fill_and_register(used, freeSizeNew);
        used += freeSizeNew;
        usedSize = size;
    }

    assert_c_size ( usedSize );

    C_SIZE2(used, usedSize) = used->size = c_size_encode_used(usedSize);

    assert_c_used ( used );

    return c_data(used);
}

// If nmemb or size is 0, then calloc() returns either NULL, or a unique pointer value that can later be successfully passed to free().
// If the multiplication of nmemb and size would result in integer overflow, then calloc() returns an error.
void* dedipy_calloc (size_t n, size_t size_) {

    const u64 size = (u64)n * (u64)size_;

    void* const data = dedipy_malloc(size);

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

    if (data_ == NULL)
        return dedipy_malloc(dataSizeNew_);

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 sizeNew = C_SIZE_FROM_DATA_SIZE(dataSizeNew_);

    //
    if (sizeNew > C_SIZE_MAX)
        return NULL;

    // FOI NOS PASSADO O DATA; VAI PARA O CHUNK
    chunk_s* const chunk = c_from_data(data_);

    assert_c_used(chunk);

    u64 size = c_size_decode_used(chunk->size);

    if (size >= sizeNew) { // ELE SE AUTOFORNECE
        if ((size - sizeNew) < C_SIZE_MIN)
            return data_; // MAS NÃO VALE A PENA DIMINUIR
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return data_;
    }

    chunk_s* const right = c_right(chunk, size);

    if (c_is_free(right)) { // É FREE E SUFICIENTE

        const u64 rightSize = c_size_decode_free(right->size);
        // O QUANTO VAMOS TENTAR RETIRAR DA DIREITA
        const u64 sizeNeeded = sizeNew - size;

        if (rightSize >= sizeNeeded) {
            // DÁ SIM; VAMOS ALOCAR ARRANCANDO DA DIREITA

            // O TAMANHO NOVO DA DIREITA
            const u64 rightSizeNew = rightSize - sizeNeeded;

            c_unlink(right);

            if (rightSizeNew >= C_SIZE_MIN) {
                // PEGA ESTE PEDAÇO DELE, PELA ESQUERDA
                size += sizeNeeded; // size -> sizeNew
                // MOVE O COMEÇO PARA A DIREITA E O REGISTRA
                c_free_fill_and_register(right + sizeNeeded, rightSizeNew);
            } else // CONSOME ELE POR INTEIRO
                size += rightSize; // size -> o que ja era + o free chunk

            C_SIZE2(chunk, size) = chunk->size = c_size_encode_used(size);

            assert_c_used(size);

            return c_data(chunk);
        }
    }

    // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
    chunk_s* const data = dedipy_malloc(dataSizeNew_);

    if (data) { // CONSEGUIU
        // COPIA DO CHUNK ORIGINAL
        memcpy(data, data_, u_data_size(size));
        // LIBERA O CHUNK ORIGINAL
        dedipy_free(c_data(chunk));

        assert_c_used(c_from_data(data))
        assert_c_data(c_from_data(data), data);
    }

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







// ver onde dá par diminuir acessos a ponteiros
//      usar buffLMT em alguns casos? :S
// Só precisamos do buff, pois ele é o root. as demais coisas sós ao acessadas para inicializar e verificar.
// O restante é acessado diretamente, pelos ponteiros que o usuário possui.
