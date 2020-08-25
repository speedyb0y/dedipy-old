#!/usr/bin/python

HEADS_N = 128

N_START = 5

# PARA FICAR UNIFORME TEM QUE SER X_DIVISOR = (X_COUNT*X_SALT)
X_SALT = 1
X_DIVISOR = 5
X_LAST = 5

FIRST, SECOND, *_, LAST, LMT = SEQUENCE = [(1 << n) + ((x * (1 << n)) // X_DIVISOR) for n in range(N_START, 1024) for x in range(X_LAST)][:HEADS_N + 1]

print('//', ' '.join(map(str, SEQUENCE)))
print('')
print('#define HEADS_N %d' % HEADS_N)
print('')
print('#define N_START %d' % N_START)
print('#define X_DIVISOR %d' % X_DIVISOR)
print('#define X_SALT %d' % X_SALT)
print('#define X_LAST %d' % X_LAST)
print('')
print('#define FIRST_SIZE  0x%016XULL' % FIRST)
print('#define SECOND_SIZE 0x%016XULL' % SECOND)
print('#define LAST_SIZE   0x%016XULL' % LAST)
print('#define LMT_SIZE    0x%016XULL' % LMT)
print('')
