#!/usr/bin/python

### O HUGEPAGE é PROCESSES[(offset, size),] |PROCESS0|PROCESS2|
####  TODO: FIXME: ja executar ele rodando como root, scheduling e cpu

ROOTS_N = 256

N_START = 5

# PARA FICAR UNIFORME TEM QUE SER X_DIVISOR = (X_COUNT*X_SALT)
X_SALT = 1
X_DIVISOR = 8 # (ISSO - 1) -> QUANTOS FICAM NO MEIO ENTRE A ... 2*A
X_LAST = 8

# TODO: FIXME: IMPLEMENTAR NO CÓDIGO; PODE SER POSITIVO OU NEGATIVO
INITIAL = 0

FST, *_, LST, LMT = SEQUENCE = [(INITIAL + (1 << n) + ((x * (1 << n)) // X_DIVISOR)) for n in range(N_START, 1024) for x in range(X_LAST)][:ROOTS_N + 1]

C_SIZE_MIN = FST
# É O LST, ARREDONDADO PARA BAIXO
C_SIZE_MAX = LST - (LST % 8)

C_SIZE_MIN_ = '%016X' % C_SIZE_MIN
C_SIZE_MAX_ = '%016X' % C_SIZE_MAX

FST_ = '%016X' % FST
LST_ = '%016X' % LST
LMT_ = '%016X' % LMT

OPEN, CLOSE = '{', '}'

ROOTS_SIZES = '{' + ','.join(('0x%XULL' % s for s in SEQUENCE)) + '}'

SUFFIX = 'ULL'

print(f'''
#define N_START {N_START}
#define X_DIVISOR {X_DIVISOR}
#define X_SALT {X_SALT}
#define X_LAST {X_LAST}

#define ROOTS_N {ROOTS_N}

#define ROOTS_SIZES_FST 0x{FST_}{SUFFIX} // {FST}
#define ROOTS_SIZES_LST 0x{LST_}{SUFFIX} // {LST}
#define ROOTS_SIZES_LMT 0x{LMT_}{SUFFIX} // {LMT}

static const u64 ROOTS_SIZES[] = {ROOTS_SIZES};

#define C_SIZE_MIN 0x{C_SIZE_MIN_}{SUFFIX} // {C_SIZE_MIN}
#define C_SIZE_MAX 0x{C_SIZE_MAX_}{SUFFIX} // {C_SIZE_MAX}
''')

assert LMT <= (1 << 63)

### SOMENTE PARA VISUALIZAR
print(' '.join((((f'{x//(1024*1024*1024)}g' if x > 1024*1024 else f'{x//(1024*1024)}m') if x > 1024*1024 else f'{x//1024}k') if x > 1024 else f'{x}') for x in SEQUENCE))
