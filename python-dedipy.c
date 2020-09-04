/*
    TODO: FIXME: TESTAR COM VÁRIAS THREADS/FORKS: WORKER[workerID:subProcessID]

 buff                                buffLMT
 |___________________________________|
 |    ROOTS  | L |    CHUNKS     | R |

  TODO: FIXME: reduzir o PTR e o NEXT para um u32, múltiplo de 16
  TODO: FIXME: interceptar signal(), etc. quem captura eles somos nós. e quando possível, executamos a função deles
  TODO: FIXME: PRECISA DO MSYNC?
  TODO: FIXME: INTERCEPTAR sched_*()
  TODO: FIXME: INTERCEPTAR exec*()
  TODO: FIXME: INTERCEPTAR system()
  TODO: FIXME: INTERCEPTAR fork()
  TODO: FIXME: INTERCEPTAR clone()
  TODO: FIXME: INTERCEPTAR POSIX ALIGNED MEMORY FUNCTIONS

    cp -v ${HOME}/dedipy/{util.h,dedipy.h,python-dedipy.h,python-dedipy.c} .

    TODO: FIXME: definir dedipy_malloc como (buff->malloc)
        no main do processo a gente
        todo o codigo vai somente no main
        todos os modulos, libs etc do processo atual a partir dai poderao usar o as definicoes do python-dedipy.h

        BUFFER_MALLOC malloc_f_t malloc;

    NOTE: não vamos ter um C_DATA_MAX... pois nao vamos alocar nada próximo de 2^64 mesmo :/

    // O ALGORITIMO PODE USAR AS FLAGS PARA COISAS MAIS INTELIGENTES :/
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
#error
#endif

#if _FILE_OFFSET_BITS != 64
#error
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

#include "python-dedipy-defs.h"

#define BUFF_ROOTS   ((chunk_s**)       (buff ))
#define BUFF_L       ((chunk_size_enc_t*)(buff + ROOTS_N*sizeof(void*) ))
#define BUFF_CHUNKS  ((chunk_s*        )(buff + ROOTS_N*sizeof(void*) + sizeof(chunk_size_enc_t)))
#define BUFF_R       ((chunk_size_enc_t*)(buff + buffSize - sizeof(chunk_size_enc_t) ))
#define BUFF_LMT                        (buff + buffSize )

#define BUFF_ROOTS_SIZE (ROOTS_N*sizeof(void*))
#define BUFF_L_SIZE sizeof(chunk_size_enc_t)
#define BUFF_CHUNKS_SIZE (buffSize - BUFF_ROOTS_SIZE - BUFF_L_SIZE - BUFF_R_SIZE) // É TODO O BUFFER RETIRANDO O RESTANTE
#define BUFF_R_SIZE sizeof(chunk_size_enc_t)

// FOR DEBUGGING
#define BOFFSET(x) ((uintll)((void*)(x) - (void*)BUFF_ADDR))

typedef u64 data_size_t;

typedef void* data_p;

typedef u64 chunk_size_t;
typedef u64 chunk_size_enc_t;

typedef struct chunk_s chunk_s;

struct chunk_s {
    chunk_size_enc_t size;
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

// TODO: FIXME: dar assert para que DATA_ALIGNMENT fique alinhado a isso no used

// TAMANHO DE UM CHUNK COM TAL DATA SIZE, ALINHADO
// NOTE: QUEREMOS TANTO OS DADOS QUANTO O CHUNK ALINHADOS; ALINHA AO QUE FOR MAIOR
#define C_USED_SIZE__(ds) ((sizeof(chunk_size_enc_t) + (ds) + (DATA_ALIGNMENT - 1) + sizeof(chunk_size_enc_t)) & ~(DATA_ALIGNMENT - 1))

// O TAMANHO DO CHUNK TEM QUE CABER ELE QUANDO ESTIVER LIVRE
#define c_size_from_data_size(_ds) ((chunk_size_t)({ const chunk_size_t ds = _FORCE_TYPE(data_size_t, _ds); (C_USED_SIZE__(ds) > C_SIZE_MIN ? C_USED_SIZE__(ds) : C_SIZE_MIN); }))

//  C_SIZE_MIN
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
#define BUFF_LR_VALUE ((chunk_size_enc_t)C_FLAG_L) // ass

#define C_FLAGS     0b1111ULL
#define C_FLAG_FREE 0b0001ULL
#define C_FLAG_USED 0b0010ULL
#define C_FLAG_L    0b0100ULL
#define C_FLAG_R    0b1000ULL

#define C_SIZE(s)          ({ const chunk_size_t     _s = _FORCE_TYPE(chunk_size_t,     s); assert(((_s & C_FLAGS) ==        0ULL)  && ((_s & ~C_FLAGS) % CHUNK_ALIGNMENT == 0) && ((_s & ~C_FLAGS) >= C_SIZE_MIN)) ; _s; })
#define C_SIZE_ENCODED(s)  ({ const chunk_size_enc_t _s = _FORCE_TYPE(chunk_size_enc_t, s); assert(((_s & C_FLAGS)               )  && ((_s & ~C_FLAGS) % CHUNK_ALIGNMENT == 0) && ((_s & ~C_FLAGS) >= C_SIZE_MIN)) ; _s; })
#define C_SIZE_FREE(s)     ({ const chunk_size_enc_t _s = _FORCE_TYPE(chunk_size_enc_t, s); assert(((_s & C_FLAGS) == C_FLAG_FREE)  && ((_s & ~C_FLAGS) % CHUNK_ALIGNMENT == 0) && ((_s & ~C_FLAGS) >= C_SIZE_MIN)) ; _s; })
#define C_SIZE_USED(s)     ({ const chunk_size_enc_t _s = _FORCE_TYPE(chunk_size_enc_t, s); assert(((_s & C_FLAGS) == C_FLAG_USED)  && ((_s & ~C_FLAGS) % CHUNK_ALIGNMENT == 0) && ((_s & ~C_FLAGS) >= C_SIZE_MIN)) ; _s; })

//_FORCE_TYPE(chunk_s*, c)
#define C_CHUNK(c) ({ const chunk_s* const _c = (c); assert(c_size_is_free(_c->size) ? C_FREE(_c) : C_USED(_c)); _c; })
// _FORCE_TYPE(chunk_s*, c)
#define C_USED(c)  ({ typeof(c) _c = (c); assert(in_chunks(_c, C_SIZE_MIN) && C_SIZE_USED(_c->size) && in_chunks(_c, c_size_decode_used(_c->size)) && C_SIZE(c_size_decode_used(_c->size))); assert_c_sizes(_c); _c; })
#define C_FREE(c)  ({ typeof(c) _c = (c); assert(in_chunks(_c, C_SIZE_MIN) && C_SIZE_FREE(_c->size) && in_chunks(_c, c_size_decode_free(_c->size)) && C_SIZE(c_size_decode_free(_c->size))); assert_c_sizes(_c); assert_c_ptr(_c); assert_c_next(_c); _c; })

#define c_size2(c, s) (*(chunk_size_enc_t*)((void*)(c) + C_SIZE(s) - sizeof(chunk_size_enc_t)))

#define c_size_is_free(s) ((int)((C_SIZE_ENCODED(s) & C_FLAGS) == C_FLAG_FREE))
#define c_size_is_used(s) ((int)((C_SIZE_ENCODED(s) & C_FLAGS) == C_FLAG_USED))

#define c_size_encode_free(s) ((chunk_size_enc_t)(C_SIZE(s) | C_FLAG_FREE))
#define c_size_encode_used(s) ((chunk_size_enc_t)(C_SIZE(s) | C_FLAG_USED))

// NAO SEI QUAL É, NAO PODE USAR assert DE FLAG
// -> CUIDADO COM O LR
#define c_size_decode(s)      ((chunk_size_t)(C_SIZE_ENCODED(s) & ~C_FLAGS))
#define c_size_decode_free(s) ((chunk_size_t)((C_SIZE_FREE(s)) & ~C_FLAG_FREE))
#define c_size_decode_used(s) ((chunk_size_t)((C_SIZE_USED(s)) & ~C_FLAG_USED))

#define c_left_size(c) (*(chunk_size_enc_t*)((void*)(c) - sizeof(chunk_size_enc_t)))

#define c_left(c) ((chunk_s*)({ const chunk_s* const _c = (c); (void*)_c - c_size_decode(*(chunk_size_enc_t*)((void*)_c - sizeof(chunk_size_enc_t))); }))
#define c_right(c, s) ((chunk_s*)((void*)(c) + C_SIZE(s)))

#define in_buff(a, s)   in_mem((a), (s), buff, buffSize)
#define in_chunks(a, s) in_mem((a), (s), BUFF_CHUNKS, BUFF_CHUNKS_SIZE)

// OS DOIS DEVEM SER LIDADOS DE FORMA SÍNCRONA
#define assert_c_sizes(c) ({ const chunk_s* const _c = (c) ; assert(c_size_is_free(_c->size) == !!c_size_is_free(_c->size)) ; assert(C_SIZE_MIN <= c_size_decode(_c->size)) ; assert(_c->size == c_size2(_c, c_size_decode(_c->size))); })
#define assert_c_ptr(c)   ({ const chunk_s* const _c = (c) ; assert(in_buff(_c->ptr, sizeof(*_c->ptr))) ; assert(_c->ptr) ; assert(*_c->ptr == _c); })
#define assert_c_next(c)  ({ const chunk_s* const _c = (c) ; if (_c->next) { assert_aligned(_c->next, 8); assert_in_chunks(_c->next, _c->next->size); assert(&_c->next == _c->next->ptr); } })
#define assert_c_data(c, d) assert ( c_data(c) == (d) )

#if 0
#define c_clear(c, s) ((void)memset(c, 0, C_SIZE(s)))
#else
#define c_clear(c, s) ()
#endif

    //assert_aligned ( c_data(c) , DATA_ALIGNMENT );

            //assert ( *chunk->ptr == chunk );
            //if (chunk->next) {
                //C_SIZE_FREE ( chunk->size );
                //assert ( chunk->next->ptr == &chunk->next );
            //}
            //assert_in_buff ( chunk->ptr, sizeof(chunk_s*) );
        //assert ( size == c_size_decode(c_size2(chunk, size)) );
        //assert ( c_right(chunk, size) == ((void*)chunk + size) );
        //assert_in_chunks ( chunk, size );

//assert(C_FREE(used)->next == NULL || C_FREE((C_FREE(used)->next))->ptr == &C_FREE(used)->next);
//assert ( C_SIZE_MIN <= f_get_size(used) && f_get_size(used) <= C_SIZE_MAX && (f_get_size(used) % 8) == 0 );

#define c_data(_c) ((void*)(_c) + sizeof(chunk_size_enc_t))

//_FORCE_TYPE(data_p, _d)
#define c_from_data(_d) ((chunk_s*)((void*)(_d) - sizeof(chunk_size_enc_t)))

// DATA SIZE FROM THE CHUNK SIZE
#define c_data_size(_s) ((data_size_t)(C_SIZE(_s) - sizeof(chunk_s) - sizeof(chunk_size_enc_t)))

#define c_set_sizes(_c, _s, _se) ({ chunk_s* const c = (_c); const u64 x = _FORCE_TYPE(u64,_s); const chunk_size_enc_t se = _FORCE_TYPE(chunk_size_enc_t, _se); *(chunk_size_enc_t*)((void*)(c) + C_SIZE(x) - sizeof(chunk_size_enc_t)) = c->size = se;  })

#define c_set_sizes_free(_c, _s) c_set_sizes(_c, _s, c_size_encode_free(_s))
#define c_set_sizes_used(_c, _s) c_set_sizes(_c, _s, c_size_encode_used(_s))

// TODO: FIXME: outra verificação, completa
// C_CHUNK(c) o chunk é válido, levando em consideração qual tipo é

// ESCOLHE O PRIMEIRO PTR
// BASEADO NESTE SIZE, SELECINAR UM PTR
// A PARTIR DESTE PTR É GARANTIDO QUE TODOS OS CHUNKS TENHAM ESTE TAMANHO

// PARA DEIXAR MAIS SIMPLES/MENOS INSTRUCOES
// - O LAST TEM QUE SER UM TAMANHO TAO GRANDE QUE JAMAIS SOLICITAREMOS
// f_ptr_root_get() na hora de pegar, usar o ANTEPENULTIMO como limite????
//  e o que mais????

// SE SIZE < ROOTS_SIZES_0, ENTÃO NÃO HAVERIA ONDE SER COLOCADO
//      NOTE: C_SIZE_MIN >= ROOTS_SIZES_0, ENTÃO size >= ROOTS_SIZES_0

// QUEREMOS COLOCAR UM FREE
// SOMENTE A LIB USA ISSO, ENTAO NAO PRECISA DE TANTAS CHEGAGENS?
static inline uint root_put_ptr_idx (chunk_size_t size) {

    uint e = ROOT_EXP;
    uint X = ROOT_X;
    uint x = 0;

    uint idx = 0;

    size = (C_SIZE(size) - ROOT_BASE) / ROOT_MULT;

    // (1 << e) --> (2^e)
    // CONTINUA ANDANDO ENQUANTO PROVIDENCIARMOS TANTO
    // TODO: FIXME: fazer de um jeito que va subtraindo, ao inves de recomputar esse expoente toda hora
    while ((((1ULL << e) + ((1ULL << e) * x)/X)) <= size) {
        u64 f = (x = (x + 1) % X) == 0;
        e += f;
        f *= X;
        X += f/ROOT_X_ACCEL;
        X += f/ROOT_X_ACCEL2;
        X += f/ROOT_X_ACCEL3;
        idx++;
    } idx--; // VOLTA PARA O ANTERIOR, QUE FOI O ÚLTIMO VISTO QUE ESTE GARANTE

    if (idx > (ROOTS_N - 1))
        idx = (ROOTS_N - 1);

    return idx;
}

static inline chunk_s** root_put_ptr (chunk_size_t size) {

#if 0
    // vainos masks
    const u64* mask = masks + (idx / ((sizeof(u64) * 8)));

    //if ((ID = nzeroesright(( *mask >> (idx % ((sizeof(u64) * 8))) ))))  {
    if (nzeroesright(mask) > (idx % ((sizeof(u64) * 8)))) { // ??
        // ACHOU UM QUE SATISFAZ
    } else {
        // NENHUM SATISFAZ
        // procura em todos os masks daqui em diante entao
        while (){ mask++;

            // MAS LEMBRA A MASK ATUAL

        } // DEIXA UM DUMMY DEPOIS DOS MASKS PARA FORÇAR A QUEBRA DESTE LOOP
        if (mask == MASKS_LMT) {
            // NAO ENCONTROU NENHUM CHUNK LIVRE
        }
    }

    retira os % do idx,  e isso +
    (mask - MASKS) ;

    if (idx == (ROOTS_N - 1)) {
        // ENCONTRA O MENOR CHUNK POSSÍVEL QUE SATISFAÇA
        // .... ou o igual =]
        // no PUT, poe logo no começo mesmo
    }
#endif

    return BUFF_ROOTS + idx;
}

static inline uint root_get_ptr_index (chunk_size_t size) {

    uint e = ROOT_EXP;
    uint X = ROOT_X;
    uint x = 0;

    uint idx = 0;

    // TODO: FIXME: ter certeza de que isso aqui vai tornar o size impossivel de causar loop infinito ali
    size = (C_SIZE(size) - ROOT_BASE) / ROOT_MULT;

    // CONTINUA ANDANDO ENQUANTO O PROMETIDO NÃO SATISFAZER O PEDIDO
    while (((1ULL << e) + ((1ULL << e) * x)/X) < size) {
        u64 f = (x = (x + 1) % X) == 0;
        e += f;
        f *= X;
        X += f/ROOT_X_ACCEL;
        X += f/ROOT_X_ACCEL2;
        X += f/ROOT_X_ACCEL3;
        idx++;
    }

    if (idx > (ROOTS_N - 1))
        idx = (ROOTS_N - 1);

    return idx;
}

static inline chunk_s** root_get_ptr (chunk_size_t size) {

    uint idx = root_get_ptr_index(C_SIZE(size));

    return BUFF_ROOTS + idx;
}

static inline chunk_s* c_free_fill_and_register (chunk_s* const c, const u64 s) {

    c_set_sizes_free(c, C_SIZE(s));

    if ((c->next = *(c->ptr = root_put_ptr(s))))
        c->next->ptr = &c->next;
    *c->ptr = c;

    return C_FREE(c);
}

// NOTE: VAI DEIXAR O PTR E O NEXT INVÁLIDOS
static inline void c_unlink (const chunk_s* const c) {

    C_FREE(c);

    if ((*c->ptr = c->next)) {
        (*c->ptr)->ptr = c->ptr;
        C_FREE(c->next);
    }
}

// MUST HAVE SAME ALIGNMENTS AS MALLOC! :/ @_@
static void dedipy_verify (void) {

    // ROOTS
    assert(in_buff(BUFF_ROOTS, BUFF_ROOTS_SIZE));

    // CHUNKS
    assert(in_buff   (BUFF_CHUNKS, c_size_decode_free(BUFF_CHUNKS->size)));
    assert(in_chunks (BUFF_CHUNKS, c_size_decode_free(BUFF_CHUNKS->size)));

    // LEFT/RIGHT
    assert(*BUFF_L == BUFF_LR_VALUE);
    assert(*BUFF_R == BUFF_LR_VALUE);

    assert_in_buff(BUFF_L, sizeof(chunk_size_enc_t));
    assert_in_buff(BUFF_R, sizeof(chunk_size_enc_t));

    assert((buff + buffSize) == BUFF_LMT);

    assert((void*)(BUFF_ROOTS + ROOTS_N) == (void*)BUFF_L);

    assert(*BUFF_L == BUFF_LR_VALUE);
    assert(*BUFF_R == BUFF_LR_VALUE);
    assert(*BUFF_L == *BUFF_R);

    // O LEFT TEM QUE SER INTERPRETADO COMO NÃO NULL
    assert(BUFF_ROOTS[ROOTS_N]);

#if DEDIPY_VERIFY || 1
    chunk_size_t totalFree = 0;
    chunk_size_t totalUsed = 0;

    { const chunk_s* c = BUFF_CHUNKS;

        while (c != (chunk_s*)BUFF_R) {
            if (c_size_is_free(C_CHUNK(c)->size))
                totalFree += c_size_decode_free(C_FREE(c)->size);
            else
                totalUsed += c_size_decode_used(C_USED(c)->size);
            c = c_right(c, c_size_decode(c->size));
        }
    }

    const chunk_size_t total = totalFree + totalUsed;

    dbg3("-- TOTAL %llu ------", (uintll)total);

    assert ( total == BUFF_CHUNKS_SIZE );

    // VERIFICA OS FREES
    uint idx = 0; chunk_s** ptrRoot = BUFF_ROOTS;

    do {
        const chunk_size_t fst = rootSizes[idx]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR NESTE ROOT
        const chunk_size_t lmt = rootSizes[idx + 1]; // TAMANHOS DESTE EM DIANTE DEVEM FICAR SÓ NOS ROOTS DA FRENTE

        assert (fst < lmt);
        assert (fst >= C_SIZE_MIN);

        chunk_s* const* ptr = ptrRoot;
        const chunk_s* chunk = *ptrRoot;

        dbg3("FREE ROOT #%d CHUNK BX%llX", idx, BOFFSET(chunk));

        while (chunk) { const chunk_size_t size = c_size_decode_free(C_FREE(chunk)->size);
            assert ( size >= fst );
            assert ( size <  lmt );
            assert ( chunk->ptr == ptr );
            //assert ( f_ptr_root_get(f_get_size(chunk)) == ptrRoot );
            //assert ( root_put_ptr(f_get_size(chunk)) == ptrRoot ); QUAL DOS DOIS? :S e um <=/>= em umdeles
            //
            totalFree -= C_SIZE(size);
            // PRÓXIMO
            chunk = *(ptr = &C_FREE(chunk->next));
        }

        ptrRoot++;

    } while (++idx != ROOTS_N);

    assert(ptrRoot == (BUFF_ROOTS + ROOTS_N));

    // CONFIRMA QUE VIU TODOS OS FREES VISTOS AO ANDAR TODOS OS CHUNKS
    assert(totalFree == 0);
#endif
}

void* dedipy_malloc (const size_t size_) {

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 size = c_size_from_data_size(size_);

    // SÓ O QUE PODE GARANTIR
    if (size > ROOTS_SIZES_N)
        return NULL;

    chunk_s* used; // PEGA UM LIVRE A SER USADO
    chunk_s** ptr = root_get_ptr(C_SIZE(size)); // ENCONTRA A PRIMEIRA LISTA LIVRE

    // LOGO APÓS O HEADS, HÁ O LEFT CHUNK, COM UM SIZE FAKE 1, PORTANTO ELE É NÃO-NULL, E VAI PARAR SE NAO TIVER MAIS CHUNKS LIVRES
    while ((used = *ptr) == NULL)
        ptr++;

    if (used == (chunk_s*)BUFF_LR_VALUE)
        return NULL; // SAIU DOS ROOTS E NÃO ENCONTROU NENHUM

    C_FREE(used);

    u64 usedSize = c_size_decode_free(used->size);
    // TAMANHO QUE ELE FICARIA AO RETIRAR O CHUNK
    const u64 freeSizeNew = usedSize - size;

    // REMOVE ELE DE SUA LISTA, MESMO QUE SÓ PEGUEMOS UM PEDAÇO, VAI TER REALOCÁ-LO NA TREE
    c_unlink(used);

    // SE DER, CONSOME SÓ UMA PARTE, NO FINAL DELE
    if (C_SIZE_MIN <= freeSizeNew) {
        dbg("CONSUMINDO O DA DIREITA CHUNK BX%llX SIZE %llu --> NEW SIZE %llu", D(used), D(usedSize), D(freeSizeNew));
        used = c_right(c_free_fill_and_register(used, freeSizeNew), freeSizeNew);
        usedSize = size;
        dbg("CHUNK RETIRADO: CHUNK BX%llX SIZE %llu", D(used), D(usedSize));
    }

    c_set_sizes_used(used, usedSize);

    C_USED(used);

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

void dedipy_free (void* const data) {

    if (data) {
        // VAI PARA O COMEÇO DO CHUNK
        chunk_s* c = C_USED(c_from_data(data));

        u64 size = c_size_decode_used(c->size);

        u64 ss;

        // JOIN WITH THE LEFT CHUNK
        if ((ss = c_size_decode_free(c_left_size(c)))) {
            size += ss;
            c_unlink((c = (void*)c - ss));
        }

        // JOIN WITH THE RIGHT CHUNK
        if ((ss = c_size_decode_free(c_right(c, size)->size))) {
            size += ss;
            c_unlink((void*)c + ss);
        }

        C_FREE(c_free_fill_and_register(c, size));
    }
}

// The  realloc()  function returns a pointer to the newly allocated memory, which is suitably aligned for any built-in type, or NULL if the request failed.
// The returned pointer may be the same as ptr if the allocation was not moved (e.g., there was room to expand the allocation in-place),
//    or different from ptr if the allocation was moved to a new address.
// If size was equal to 0, either NULL or a pointer suitable to be passed to free() is returned.
// If realloc() fails, the original block is left untouched; it is not freed or moved.
void* dedipy_realloc (void* const data_, const size_t dataSizeNew_) {

    if (data_ == NULL)
        return dedipy_malloc((data_size_t)dataSizeNew_);

    // CONSIDERA O CHUNK INTEIRO, E O ALINHA
    u64 sizeNew = c_size_from_data_size((data_size_t)dataSizeNew_);

    // FOI NOS PASSADO O DATA; VAI PARA O CHUNK
    chunk_s* const chunk = C_USED(c_from_data(data_));

    u64 size = c_size_decode_used(chunk->size);

    if (size >= sizeNew) { // ELE SE AUTOFORNECE
        if ((size - sizeNew) < C_SIZE_MIN)
            return data_; // MAS NÃO VALE A PENA DIMINUIR
        // TODO: FIXME: SE FOR PARA DIMINUIR, DIMINUI!!!
        return data_;
    }

    chunk_s* const right = c_right(chunk, size);

    const u64 rightSize = c_size_decode_free(right->size);

    if (rightSize) {

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

            c_set_sizes_used(chunk, size);

            return c_data(C_USED(chunk));
        }
    }

    // NAO TEM ESPAÇO NA DIREITA; ALOCA UM NOVO
    chunk_s* const data = dedipy_malloc(dataSizeNew_);

    if (data) { // CONSEGUIU
        // COPIA DO CHUNK ORIGINAL
        memcpy(data, data_, c_data_size(size));
        // LIBERA O CHUNK ORIGINAL
        dedipy_free(c_data(chunk));

        C_USED(c_from_data(data));
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

#if DEDIPY_TEST
void dedipy_test (void) {

    assert ( ROOTS_SIZES_0 <= C_SIZE(c_size_from_data_size((data_size_t)1)) );

    assert ( dedipy_malloc(0) == NULL );
    assert ( dedipy_realloc(BUFF_CHUNKS, 0) == NULL );
    assert ( dedipy_realloc(NULL, 0) == NULL );

    // NÃO TEM COMO DAR ASSERT LOL
    dedipy_free(NULL);

    dedipy_free(c_data(c_from_data(dedipy_realloc(NULL, 65536))));

    //
    assert ( dedipy_realloc(dedipy_malloc(65536), 0) == NULL );

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
}
#endif

void dedipy_main (void) {

    // SUPPORT CALLING FROM MULTIPLE PLACES =]
    static int initialized = 0;

    if (initialized)
        return;

    assert ( is_from_to(0, 0, 2) );
    assert ( is_from_to(0, 1, 2) );
    assert ( is_from_to(0, 2, 2) );
    assert ( is_from_to(0, 0, 0) );
    assert ( is_from_to(1, 1, 1) );

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
        dbg("RUNNING IN FALL BACK MODE");
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

    dbg("INICIALIZANDO AINDA...");

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

    //
    assert (C_SIZE_MIN == ROOTS_SIZES_0);

    assert (ROOTS_SIZES_0 < ROOTS_SIZES_N);

    assert( c_size_decode_free(c_size_encode_free((data_size_t)1ULL)) == 1ULL );
    assert( c_size_decode_free(c_size_encode_free((data_size_t)2ULL)) == 2ULL );
    assert( c_size_decode_free(c_size_encode_free((data_size_t)3ULL)) == 3ULL );
    assert( c_size_decode_free(c_size_encode_free((data_size_t)4ULL)) == 4ULL );
    assert( c_size_decode_free(c_size_encode_free((data_size_t)65536ULL)) == 65536ULL );

    assert( c_size_decode_used(c_size_encode_used((data_size_t)1ULL)) == 1ULL );
    assert( c_size_decode_used(c_size_encode_used((data_size_t)2ULL)) == 2ULL );
    assert( c_size_decode_used(c_size_encode_used((data_size_t)3ULL)) == 3ULL );
    assert( c_size_decode_used(c_size_encode_used((data_size_t)4ULL)) == 4ULL );
    assert( c_size_decode_used(c_size_encode_used((data_size_t)65536ULL)) == 65536ULL );

    assert ( root_get_ptr(C_SIZE_MIN) == BUFF_ROOTS );
    assert ( root_put_ptr(C_SIZE_MIN) == BUFF_ROOTS );

    assert ( root_get_ptr(16ULL*1024*1024*1024*1024) == (BUFF_ROOTS + (ROOTS_N - 1)) );
    assert ( root_put_ptr(16ULL*1024*1024*1024*1024) <= (BUFF_ROOTS + (ROOTS_N - 1)) ); // COMO VAI ARREDONDAR PARA BAIXO, PEDIR O MÁXIMO PODE CAIR LOGO ANTES DO ÚLTIMO SLOT

    assert ( c_size_decode_used(c_size_encode_used((data_size_t)65536)) == (data_size_t)65536 );
    assert ( c_size_decode_free(c_size_encode_free((data_size_t)65536)) == (data_size_t)65536 );

    assert ( c_size_decode_used(c_size_encode_used((data_size_t)C_SIZE_MIN)) == (data_size_t)C_SIZE_MIN );
    assert ( c_size_decode_free(c_size_encode_free((data_size_t)C_SIZE_MIN)) == (data_size_t)C_SIZE_MIN );

    // LEFT AND RIGHT
    *BUFF_L = BUFF_LR_VALUE;
    *BUFF_R = BUFF_LR_VALUE;

    dbg("CRIANDO CHUNK 0 %llu", (uintll)BUFF_CHUNKS_SIZE);

    // THE INITIAL CHUNK
    // É O MAIOR CHUNK QUE PODERÁ SER CRIADO; ENTÃO AQUI CONFIRMA QUE O C_SIZE_MAX E ROOTS_SIZES_N SÃO MAIORES DO QUE ELE
    c_free_fill_and_register(BUFF_CHUNKS, BUFF_CHUNKS_SIZE);

    dbg("CHUNK 0 CRIADO");

    // TODO: FIXME: tentar dar dedipy_malloc() e dedipy_realloc() com valores bem grandes, acima desses limies, e confirmar que deu NULL
    // ROOTS_SIZES_N

    dbg("CPU %u",                  (uint)cpu);
    dbg("PID %llu",                (uintll)pid);
    dbg("ID %u",                   (uint)id);
    dbg("CODE %llu",               (uintll)code);

    dbg("ROOTS_N %llu", (uintll)ROOTS_N);
    dbg("ROOTS_SIZES_0 %llu", (uintll)ROOTS_SIZES_0);
    dbg("ROOTS_SIZES_N %llu", (uintll)ROOTS_SIZES_N);

    dbg("C_SIZE_MIN %llu", (uintll)C_SIZE_MIN);

    dbg("BUFF ADDR 0x%016llX",      (uintll)BUFF_ADDR);
    dbg("BUFF TOTAL SIZE %llu",     (uintll)buffTotal);
    dbg("BUFF START %llu",          (uintll)buffStart_);
    dbg("BUFF 0x%016llX",           BOFFSET(buff));
    dbg("BUFF_ROOTS BX%llX",        BOFFSET(BUFF_ROOTS));
    dbg("BUFF_L BX%llX",            BOFFSET(BUFF_L));
    dbg(" BUFF_CHUNKS BX%llX",      BOFFSET(BUFF_CHUNKS));
    //dbg(" BUFF_CHUNKS SIZE %llu",   (uintll)(c_size_decode_free(BUFF_CHUNKS->size)));
    dbg(" BUFF_CHUNKS PTR BX%llX",  BOFFSET( BUFF_CHUNKS->ptr));
    dbg("*BUFF_CHUNKS PTR BX%llX",  BOFFSET(*BUFF_CHUNKS->ptr));
    dbg(" BUFF_CHUNKS NEXT BX%llX", BOFFSET( BUFF_CHUNKS->next));
    dbg("BUFF_R BX%llX",            BOFFSET(BUFF_R));
    dbg("BUFF_LMT BX%llX",          BOFFSET(BUFF_LMT));
    dbg("BUFF SIZE %llu",          (uintll)buffSize);

    dbg("*BUFF_L %llu",    (uintll)*BUFF_L);
    dbg("*BUFF_R %llu",    (uintll)*BUFF_R);

    assert ( sizeof(u64) == sizeof(off_t) );
    assert ( sizeof(u64) == sizeof(size_t) );
    assert ( sizeof(u64) == 8 );
    assert ( sizeof(void*) == 8 );

    void* a = dedipy_malloc((data_size_t)(2*1024*1024));

    if (dedipy_malloc((data_size_t)(1)))
        (void)0;

    dedipy_free(a);

    dedipy_free(dedipy_malloc((data_size_t)(      65536)));
    dedipy_free(dedipy_malloc((data_size_t)(2*1024*1024)));
    dedipy_free(dedipy_malloc((data_size_t)(3*1024*1024)));
    dedipy_free(dedipy_malloc((data_size_t)(4*1024*1024)));

    dedipy_verify();

    dbg("OKAY!");
}

//int posix_memalign(void **memptr, size_t alignment, size_t size)
//void *aligned_alloc(size_t alignment, size_t size)
//void *valloc(size_t size)
//void *memalign(size_t alignment, size_t size)
//void *pvalloc(size_t size)
//int brk(void *addr) {
//void *sbrk(intptr_t increment) ;
//void sync (void)
//int syncfs (int fd)

// ver onde dá par diminuir acessos a ponteiros
//      usar buffLMT em alguns casos? :S
// Só precisamos do buff, pois ele é o root. as demais coisas sós ao acessadas para inicializar e verificar.
// O restante é acessado diretamente, pelos ponteiros que o usuário possui.

// warn_unused_result
// -Werror=unused-result

// COLOCAR o BUFF_LR_VALUE

USOS DO c_size_Xcode_X()



testar com cada rootSizes[idx] + ( -2, -1, 0, +1, +2 )
