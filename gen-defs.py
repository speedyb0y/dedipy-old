#!/usr/bin/python

# (1 << 40)/(1024*1024*1024*1024) = 1tb
# ENTÃO OBJETIVO É QUE N CHEGUE A 40

### O HUGEPAGE é PROCESSES[(offset, size),] |PROCESS0|PROCESS2|
####  TODO: FIXME: ja executar ele rodando como root, scheduling e cpu

ROOTS_N = 4096

ROOT_EXP = 5

ROOT_BASE = -28

# PARA FICAR UNIFORME TEM QUE SER X_DIVISOR = X_COUNT
ROOT_X = 32 # (ISSO - 1) -> QUANTOS FICAM NO MEIO ENTRE A ... 2*A
ROOT_X_ACCEL = 7
ROOT_X_A = 64
ROOT_X_B = 230

def sequence():

    idx = 0

    X = ROOT_X # QUANTAS QUEBRAS TEMOS INICIALMENTE

    e = ROOT_EXP # e é a potencia

    while True:
        x = 0
        # X é o quanto ele vai ficar quebrando uma potência
        while x != X:
            yield ROOT_BASE + (1 << e) + ((x * (1 << e)) // X)
            idx += 1
            if idx == ROOTS_N:
                return
            x += 1
        X += X//ROOT_X_ACCEL + X//ROOT_X_A + X//ROOT_X_B # a velocidade aumenta a cada ROOT_X_ACCEL vezes
        e += 1

ROOTS_SIZES_0, *_, ROOTS_SIZES_N = SEQUENCE = tuple(sequence())

assert len(SEQUENCE) == ROOTS_N

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

static const u64 rootSizes[] = {ROOTS_SIZES};

#define ROOT_EXP {ROOT_EXP}
#define ROOT_BASE {ROOT_BASE}
#define ROOT_X {ROOT_X}
#define ROOT_X_ACCEL {ROOT_X_ACCEL}
#define ROOT_X_A {ROOT_X_A}
#define ROOT_X_B {ROOT_X_B}

#define C_SIZE_MIN 0x{C_SIZE_MIN_}{SUFFIX} // {C_SIZE_MIN}
#define C_SIZE_MAX 0x{C_SIZE_MAX_}{SUFFIX} // {C_SIZE_MAX}
''')

### SOMENTE PARA VISUALIZAR

UNITS=8

# É DESPERDÍCIO PROVIDENCIAR TAMANHOS MENORES DO QUE O QUE VAI SER ALOCADO
# O MÁXIMO TEM DE SER GARANTIDO

print(' '.join((((f'{x//(1024*1024*1024)}g' if x > 1024*1024*1024 else f'{x//(1024*1024)}m') if x > 1024*1024 else f'{x//1024}k') if x > 1024 else f'{x}') for x in (x*UNITS for x in SEQUENCE)))

# roots pequenos representam o proximo livre

# os pequenos representam

# usar prefetches
# pode usar simd etc
assert C_SIZE_MIN <= ROOTS_SIZES_0 < C_SIZE_MAX <= ROOTS_SIZES_N <= (1 << 64)


# SE ALOCA MUITO, NAO TEM PROBLEMA POLUIR O CACHE
#       -> desde que bem utilizado e que continue la
# SE ALOCA POUCO, NAO TEM PROBLEMA DEMORAR UM POUCO MAIS
#
