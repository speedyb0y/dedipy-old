#!/usr/bin/python

### O HUGEPAGE é PROCESSES[(offset, size),] |PROCESS0|PROCESS2|
####  TODO: FIXME: ja executar ele rodando como root, scheduling e cpu

ROOTS_N = 200

N_START = 5

# PARA FICAR UNIFORME TEM QUE SER X_DIVISOR = (X_COUNT*X_SALT)
X_SALT = 1
X_DIVISOR = 5
X_LAST = 5

FST, *_, LST, LMT = SEQUENCE = [(1 << n) + ((x * (1 << n)) // X_DIVISOR) for n in range(N_START, 1024) for x in range(X_LAST)][:ROOTS_N + 1]

# É O LST, ARREDONDADO PARA BAIXO
CHUNK_SIZE_MAX = LST - (LST % 8)

OPEN, CLOSE = '{', '}'

ROOTS_SIZES = '{' + ','.join(('0x%XULL' % s for s in SEQUENCE)) + '}'

print(f'''
#define N_START {N_START}
#define X_DIVISOR {X_DIVISOR}
#define X_SALT {X_SALT}
#define X_LAST {X_LAST}

#define ROOTS_N {ROOTS_N}

#define ROOTS_SIZES_FST {FST}ULL
#define ROOTS_SIZES_LST {LST}ULL
#define ROOTS_SIZES_LMT {LMT}ULL

static const u64 ROOTS_SIZES[] = {ROOTS_SIZES};

#define CHUNK_SIZE_MAX {CHUNK_SIZE_MAX}ULL
''')
