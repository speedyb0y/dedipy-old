#!/usr/bin/python

# (1 << 40)/(1024*1024*1024*1024) = 1tb
# ENTÃO OBJETIVO É QUE N CHEGUE A 40

### O HUGEPAGE é PROCESSES[(offset, size),] |PROCESS0|PROCESS2|
####  TODO: FIXME: ja executar ele rodando como root, scheduling e cpu

ROOTS_N = 4096

N_START = 5

INITIAL = -28

# PARA FICAR UNIFORME TEM QUE SER X_DIVISOR = (X_COUNT*X_SALT)
X_INICIAL = 32 # (ISSO - 1) -> QUANTOS FICAM NO MEIO ENTRE A ... 2*A
X_SALT = 1
X_ACELERACAO = 7
# 32 40 48 56 64 72 80 88 96 104 112 120 128 136 144 152 160 168 176 184 192 200 208 216 224 232 240 248 256 264 272 280 288 296 312 320 336 352 360 376 384 400 416 424 440 448 464 480 488 504 512 528 544 552 568 576 592 608 616 632 640 656 672 680 696 704 720 736 744 760 768 784 800 816 840 856 880 896 920 936 960 984 1000 1024

def sequence():

    idx = 0

    n = N_START # n é a potencia

    X_DIVISOR = X_INICIAL # QUANTAS QUEBRAS TEMOS INICIALMENTE

    while True:
        x = 0
        # X_DIVISOR é o quanto ele vai ficar quebrando uma potência
        while x != X_DIVISOR:
            yield INITIAL + (1 << n) + ((x * (1 << n)) // X_DIVISOR)
            # yield X_DIVISOR
            # yield n
            idx += 1
            if idx == ROOTS_N:
                return
            x += X_SALT

        X_DIVISOR += X_DIVISOR//X_ACELERACAO + X_DIVISOR//64 + X_DIVISOR//230 # a velocidade aumenta a cada X_ACELERACAO vezes

        n += 1
X_DIVISOR = X_INICIAL
SEQUENCE = tuple(sequence())

assert len(SEQUENCE) == ROOTS_N

ROOTS_SIZES_0, *_, ROOTS_SIZES_N = SEQUENCE

C_SIZE_MIN = ROOTS_SIZES_0
# É O ROOTS_SIZES_N, ARREDONDADO PARA BAIXO
C_SIZE_MAX = ROOTS_SIZES_N - (ROOTS_SIZES_N % 8)

C_SIZE_MIN_ = '%016X' % C_SIZE_MIN
C_SIZE_MAX_ = '%016X' % C_SIZE_MAX

FST_ = '%016X' % ROOTS_SIZES_0
LST_ = '%016X' % ROOTS_SIZES_N

OPEN, CLOSE = '{', '}'

ROOTS_SIZES = '{' + ','.join(('0x%XULL' % s for s in SEQUENCE)) + '}'

SUFFIX = 'ULL'

print(f'''
#define ROOTS_N {ROOTS_N}

#define ROOTS_SIZES_0 0x{FST_}{SUFFIX} // {ROOTS_SIZES_0}
#define ROOTS_SIZES_N 0x{LST_}{SUFFIX} // {ROOTS_SIZES_N}

static const u64 ROOTS_SIZES[] = {ROOTS_SIZES};

#define N_START {N_START}

#define X_DIVISOR {X_DIVISOR}
#define X_SALT {X_SALT}
#define X_DIVISOR {X_DIVISOR}

#define C_SIZE_MIN 0x{C_SIZE_MIN_}{SUFFIX} // {C_SIZE_MIN}
#define C_SIZE_MAX 0x{C_SIZE_MAX_}{SUFFIX} // {C_SIZE_MAX}
''')

### SOMENTE PARA VISUALIZAR

UNITS=8

# É DESPERDÍCIO PROVIDENCIAR TAMANHOS MENORES DO QUE O QUE VAI SER ALOCADO
# O MÁXIMO TEM DE SER GARANTIDO


print(' '.join((((f'{x//(1024*1024*1024)}g' if x > 1024*1024*1024 else f'{x//(1024*1024)}m') if x > 1024*1024 else f'{x//1024}k') if x > 1024 else f'{x}') for x in (x*UNITS for x in SEQUENCE)))
# print([x*8 for x in SEQUENCE])
# roots pequenos representam o proximo livre

# os pequenos representam

# usar prefetches
# pode usar simd etc
assert C_SIZE_MIN <= ROOTS_SIZES_0 < C_SIZE_MAX <= ROOTS_SIZES_N <= (1 << 64)




# SE ALOCA MUITO, NAO TEM PROBLEMA POLUIR O CACHE
#       -> desde que bem utilizado e que continue la
# SE ALOCA POUCO, NAO TEM PROBLEMA DEMORAR UM POUCO MAIS
#
