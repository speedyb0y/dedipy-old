#!/usr/bin/python

### O HUGEPAGE é PROCESSES[(offset, size),] |PROCESS0|PROCESS2|
####  TODO: FIXME: ja executar ele rodando como root, scheduling e cpu

HEADS_N = 128

N_START = 5

# PARA FICAR UNIFORME TEM QUE SER X_DIVISOR = (X_COUNT*X_SALT)
X_SALT = 1
X_DIVISOR = 5
X_LAST = 5

FIRST, *_, LAST = SEQUENCE = [(1 << n) + ((x * (1 << n)) // X_DIVISOR) for n in range(N_START, 1024) for x in range(X_LAST)][:HEADS_N]

print(f'''
// {" ".join(map(str, SEQUENCE))}

#define HEADS_N {HEADS_N}

#define N_START {N_START}
#define X_DIVISOR {X_DIVISOR}
#define X_SALT {X_SALT}
#define X_LAST {X_LAST}

#define FIRST_SIZE {FIRST}
#define LAST_SIZE {LAST}
''')
