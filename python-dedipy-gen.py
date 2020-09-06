#!/usr/bin/python
#
# GENERATE python-dedipy-gen.h
#

# COUNT RIGHT ZERO BITS
# sum(((1 << n) < ROOTS_GROUP_SIZE) for n in range(64))

C_SIZE_MIN = 32

ROOTS_GROUP_SIZE = 8192

ROOTS_MAX_0 =        64*1024
ROOTS_MAX_1 =       256*1024
ROOTS_MAX_2 =    8*1024*1024
ROOTS_MAX_3 =   16*1024*1024
ROOTS_MAX_4 =   32*1024*1024
ROOTS_MAX_5 =   64*1024*1024
ROOTS_MAX_6 =  128*1024*1024
ROOTS_MAX_7 = 1024*1024*1024

assert C_SIZE_MIN % 2 == 0
assert ROOTS_GROUP_SIZE % 2 == 0

assert ROOTS_MAX_0 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_1 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_2 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_3 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_4 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_5 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_6 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_7 % ROOTS_GROUP_SIZE == 0

assert C_SIZE_MIN < ROOTS_MAX_0 < ROOTS_MAX_1 < ROOTS_MAX_2 < ROOTS_MAX_3 < ROOTS_MAX_4 < ROOTS_MAX_5 < ROOTS_MAX_6 < ROOTS_MAX_7

ROOTS_GROUPS_REMAINING_0 = (ROOTS_MAX_0 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_1 = (ROOTS_MAX_1 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_2 = (ROOTS_MAX_2 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_3 = (ROOTS_MAX_3 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_4 = (ROOTS_MAX_4 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_5 = (ROOTS_MAX_5 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_6 = (ROOTS_MAX_6 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_7 = (ROOTS_MAX_7 // ROOTS_GROUP_SIZE) - 1

ROOTS_DIV_0 = sum(((1 << n) < (ROOTS_MAX_0 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_1 = sum(((1 << n) < (ROOTS_MAX_1 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_2 = sum(((1 << n) < (ROOTS_MAX_2 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_3 = sum(((1 << n) < (ROOTS_MAX_3 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_4 = sum(((1 << n) < (ROOTS_MAX_4 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_5 = sum(((1 << n) < (ROOTS_MAX_5 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_6 = sum(((1 << n) < (ROOTS_MAX_6 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_7 = sum(((1 << n) < (ROOTS_MAX_7 // ROOTS_GROUP_SIZE)) for n in range(64))

ROOTS_GROUPS_OFFSET_0  = 0 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_1  = 1 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_2  = 2 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_3  = 3 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_4  = 4 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_5  = 5 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_6  = 6 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_7  = 7 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_8  = 8 * ROOTS_GROUP_SIZE

g0 = ['%10d'%(idx*(ROOTS_MAX_0//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g1 = ['%10d'%(idx*(ROOTS_MAX_1//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g2 = ['%10d'%(idx*(ROOTS_MAX_2//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g3 = ['%10d'%(idx*(ROOTS_MAX_3//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g4 = ['%10d'%(idx*(ROOTS_MAX_4//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g5 = ['%10d'%(idx*(ROOTS_MAX_5//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g6 = ['%10d'%(idx*(ROOTS_MAX_6//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g7 = ['%10d'%(idx*(ROOTS_MAX_7//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]

g0_a, g0_b, g0_c, g0_d, g0_e, *_, g0_n2, g0_n1, g0_n = g0
g1_a, g1_b, g1_c, g1_d, g1_e, *_, g1_n2, g1_n1, g1_n = g1
g2_a, g2_b, g2_c, g2_d, g2_e, *_, g2_n2, g2_n1, g2_n = g2
g3_a, g3_b, g3_c, g3_d, g3_e, *_, g3_n2, g3_n1, g3_n = g3
g4_a, g4_b, g4_c, g4_d, g4_e, *_, g4_n2, g4_n1, g4_n = g4
g5_a, g5_b, g5_c, g5_d, g5_e, *_, g5_n2, g5_n5, g5_n = g5
g6_a, g6_b, g6_c, g6_d, g6_e, *_, g6_n6, g6_n1, g6_n = g6
g7_a, g7_b, g7_c, g7_d, g7_e, *_, g7_n2, g7_n1, g7_n = g7

GROUP0_SAMPLE = f'%8d {g0_a} {g0_b} {g0_c} {g0_d} {g0_e} ... {g0_n2} {g0_n1} {g0_n}' % (ROOTS_MAX_0//ROOTS_GROUP_SIZE)
GROUP1_SAMPLE = f'%8d {g1_a} {g1_b} {g1_c} {g1_d} {g1_e} ... {g1_n2} {g1_n1} {g1_n}' % (ROOTS_MAX_1//ROOTS_GROUP_SIZE)
GROUP2_SAMPLE = f'%8d {g2_a} {g2_b} {g2_c} {g2_d} {g2_e} ... {g2_n2} {g2_n1} {g2_n}' % (ROOTS_MAX_2//ROOTS_GROUP_SIZE)
GROUP3_SAMPLE = f'%8d {g3_a} {g3_b} {g3_c} {g3_d} {g3_e} ... {g3_n2} {g3_n1} {g3_n}' % (ROOTS_MAX_3//ROOTS_GROUP_SIZE)
GROUP4_SAMPLE = f'%8d {g4_a} {g4_b} {g4_c} {g4_d} {g4_e} ... {g4_n2} {g4_n1} {g4_n}' % (ROOTS_MAX_4//ROOTS_GROUP_SIZE)
GROUP5_SAMPLE = f'%8d {g5_a} {g5_b} {g5_c} {g5_d} {g5_e} ... {g5_n2} {g5_n5} {g5_n}' % (ROOTS_MAX_5//ROOTS_GROUP_SIZE)
GROUP6_SAMPLE = f'%8d {g6_a} {g6_b} {g6_c} {g6_d} {g6_e} ... {g6_n6} {g6_n1} {g6_n}' % (ROOTS_MAX_6//ROOTS_GROUP_SIZE)
GROUP7_SAMPLE = f'%8d {g7_a} {g7_b} {g7_c} {g7_d} {g7_e} ... {g7_n2} {g7_n1} {g7_n}' % (ROOTS_MAX_7//ROOTS_GROUP_SIZE)

def put_idx(size):

    if (size <= (ROOTS_MAX_0 - C_SIZE_MIN)) : size -= C_SIZE_MIN               ; return (size >> ROOTS_DIV_0) + ROOTS_GROUPS_OFFSET_0
    if (size <= (ROOTS_MAX_1 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_0 ; return (size >> ROOTS_DIV_1) + ROOTS_GROUPS_OFFSET_1
    if (size <= (ROOTS_MAX_2 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_1 ; return (size >> ROOTS_DIV_2) + ROOTS_GROUPS_OFFSET_2
    if (size <= (ROOTS_MAX_3 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_2 ; return (size >> ROOTS_DIV_3) + ROOTS_GROUPS_OFFSET_3
    if (size <= (ROOTS_MAX_4 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_3 ; return (size >> ROOTS_DIV_4) + ROOTS_GROUPS_OFFSET_4
    if (size <= (ROOTS_MAX_5 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_4 ; return (size >> ROOTS_DIV_5) + ROOTS_GROUPS_OFFSET_5
    if (size <= (ROOTS_MAX_6 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_5 ; return (size >> ROOTS_DIV_6) + ROOTS_GROUPS_OFFSET_6
    if (size <= (ROOTS_MAX_7 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_6 ; return (size >> ROOTS_DIV_7) + ROOTS_GROUPS_OFFSET_7

    return ROOTS_GROUPS_OFFSET_8 - 1

def get_idx(size):

    # (size // (ROOTS_MAX_3//ROOTS_GROUP_SIZE)) + (size % (ROOTS_MAX_3//ROOTS_GROUP_SIZE) != 0) + ROOTS_GROUPS_OFFSET_0
    if (size <= (ROOTS_MAX_0 - C_SIZE_MIN)) : size -= C_SIZE_MIN               ; return (size >> ROOTS_DIV_0) + (not not (size & ROOTS_GROUPS_REMAINING_0)) + ROOTS_GROUPS_OFFSET_0
    if (size <= (ROOTS_MAX_1 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_0 ; return (size >> ROOTS_DIV_1) + (not not (size & ROOTS_GROUPS_REMAINING_1)) + ROOTS_GROUPS_OFFSET_1
    if (size <= (ROOTS_MAX_2 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_1 ; return (size >> ROOTS_DIV_2) + (not not (size & ROOTS_GROUPS_REMAINING_2)) + ROOTS_GROUPS_OFFSET_2
    if (size <= (ROOTS_MAX_3 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_2 ; return (size >> ROOTS_DIV_3) + (not not (size & ROOTS_GROUPS_REMAINING_3)) + ROOTS_GROUPS_OFFSET_3
    if (size <= (ROOTS_MAX_4 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_3 ; return (size >> ROOTS_DIV_4) + (not not (size & ROOTS_GROUPS_REMAINING_4)) + ROOTS_GROUPS_OFFSET_4
    if (size <= (ROOTS_MAX_5 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_4 ; return (size >> ROOTS_DIV_5) + (not not (size & ROOTS_GROUPS_REMAINING_5)) + ROOTS_GROUPS_OFFSET_5
    if (size <= (ROOTS_MAX_6 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_5 ; return (size >> ROOTS_DIV_6) + (not not (size & ROOTS_GROUPS_REMAINING_6)) + ROOTS_GROUPS_OFFSET_6
    if (size <= (ROOTS_MAX_7 - C_SIZE_MIN)) : size -= C_SIZE_MIN + ROOTS_MAX_6 ; return (size >> ROOTS_DIV_7) + (not not (size & ROOTS_GROUPS_REMAINING_7)) + ROOTS_GROUPS_OFFSET_7

    return ROOTS_GROUPS_OFFSET_8 - 1

TESTS = range(C_SIZE_MIN, C_SIZE_MIN+70)

print('/*')
print('  SIZE ' + ''.join('%4d' % x           for x in TESTS))
print('  PUT  ' + ''.join('%4d' % put_idx(x)  for x in TESTS))
print('  GET  ' + ''.join('%4d' % get_idx(x)  for x in TESTS))

print(f'''
    BSIZE               SIZES
    {GROUP0_SAMPLE}
    {GROUP1_SAMPLE}
    {GROUP2_SAMPLE}
    {GROUP3_SAMPLE}
    {GROUP4_SAMPLE}
    {GROUP5_SAMPLE}
    {GROUP6_SAMPLE}
    {GROUP7_SAMPLE}
*/

#define ROOTS_GROUP_SIZE {ROOTS_GROUP_SIZE}

#define ROOTS_GROUPS_REMAINING_0 {ROOTS_GROUPS_REMAINING_0}ULL
#define ROOTS_GROUPS_REMAINING_1 {ROOTS_GROUPS_REMAINING_1}ULL
#define ROOTS_GROUPS_REMAINING_2 {ROOTS_GROUPS_REMAINING_2}ULL
#define ROOTS_GROUPS_REMAINING_3 {ROOTS_GROUPS_REMAINING_3}ULL
#define ROOTS_GROUPS_REMAINING_4 {ROOTS_GROUPS_REMAINING_4}ULL
#define ROOTS_GROUPS_REMAINING_5 {ROOTS_GROUPS_REMAINING_5}ULL
#define ROOTS_GROUPS_REMAINING_6 {ROOTS_GROUPS_REMAINING_6}ULL
#define ROOTS_GROUPS_REMAINING_7 {ROOTS_GROUPS_REMAINING_7}ULL

#define ROOTS_MAX_0 {ROOTS_MAX_0}ULL
#define ROOTS_MAX_1 {ROOTS_MAX_1}ULL
#define ROOTS_MAX_2 {ROOTS_MAX_2}ULL
#define ROOTS_MAX_3 {ROOTS_MAX_3}ULL
#define ROOTS_MAX_4 {ROOTS_MAX_4}ULL
#define ROOTS_MAX_5 {ROOTS_MAX_5}ULL
#define ROOTS_MAX_6 {ROOTS_MAX_6}ULL
#define ROOTS_MAX_7 {ROOTS_MAX_7}ULL

#define ROOTS_DIV_0 {ROOTS_DIV_0}
#define ROOTS_DIV_1 {ROOTS_DIV_1}
#define ROOTS_DIV_2 {ROOTS_DIV_2}
#define ROOTS_DIV_3 {ROOTS_DIV_3}
#define ROOTS_DIV_4 {ROOTS_DIV_4}
#define ROOTS_DIV_5 {ROOTS_DIV_5}
#define ROOTS_DIV_6 {ROOTS_DIV_6}
#define ROOTS_DIV_7 {ROOTS_DIV_7}

#define ROOTS_GROUPS_OFFSET_0 {ROOTS_GROUPS_OFFSET_0}ULL
#define ROOTS_GROUPS_OFFSET_1 {ROOTS_GROUPS_OFFSET_1}ULL
#define ROOTS_GROUPS_OFFSET_2 {ROOTS_GROUPS_OFFSET_2}ULL
#define ROOTS_GROUPS_OFFSET_3 {ROOTS_GROUPS_OFFSET_3}ULL
#define ROOTS_GROUPS_OFFSET_4 {ROOTS_GROUPS_OFFSET_4}ULL
#define ROOTS_GROUPS_OFFSET_5 {ROOTS_GROUPS_OFFSET_5}ULL
#define ROOTS_GROUPS_OFFSET_6 {ROOTS_GROUPS_OFFSET_6}ULL
#define ROOTS_GROUPS_OFFSET_7 {ROOTS_GROUPS_OFFSET_7}ULL
''')
