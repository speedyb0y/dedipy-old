
TESTS = (*range(0, 65536), 0xFFFF, 0xFFFF+1, 0xFFFF, 0xFFFFFFF, 0xFFFFFFF, 0xFFFFFFF+1, 0xFFFFFFF+2, 999655352606345931)

ROOTS_GROUP_SIZE = 8192

ROOTS_MAX_0 =        64*1024
ROOTS_MAX_1 =   64*1024*1024
ROOTS_MAX_2 =  128*1024*1024
ROOTS_MAX_3 = 1024*1024*1024

assert ROOTS_GROUP_SIZE % 2 == 0

assert ROOTS_MAX_0 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_1 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_2 % ROOTS_GROUP_SIZE == 0
assert ROOTS_MAX_3 % ROOTS_GROUP_SIZE == 0

assert ROOTS_MAX_0 < ROOTS_MAX_1 < ROOTS_MAX_2 < ROOTS_MAX_3

ROOTS_GROUPS_DIV = sum(((1 << n) < ROOTS_GROUP_SIZE) for n in range(64)) # count right zero bits

ROOTS_GROUPS_REMAINING_0 = (ROOTS_MAX_0 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_1 = (ROOTS_MAX_1 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_2 = (ROOTS_MAX_2 // ROOTS_GROUP_SIZE) - 1
ROOTS_GROUPS_REMAINING_3 = (ROOTS_MAX_3 // ROOTS_GROUP_SIZE) - 1

ROOTS_DIV_0 = sum(((1 << n) < (ROOTS_MAX_0 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_1 = sum(((1 << n) < (ROOTS_MAX_1 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_2 = sum(((1 << n) < (ROOTS_MAX_2 // ROOTS_GROUP_SIZE)) for n in range(64))
ROOTS_DIV_3 = sum(((1 << n) < (ROOTS_MAX_3 // ROOTS_GROUP_SIZE)) for n in range(64))

ROOTS_GROUPS_OFFSET_0  = 0 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_1  = 1 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_2  = 2 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_3  = 3 * ROOTS_GROUP_SIZE
ROOTS_GROUPS_OFFSET_4  = 4 * ROOTS_GROUP_SIZE

assert all((x >> 5) == (x//32) for x in TESTS)

assert all((x // (ROOTS_MAX_0 // ROOTS_GROUP_SIZE)) == (x >> ROOTS_DIV_0) for x in TESTS)
assert all((x // (ROOTS_MAX_1 // ROOTS_GROUP_SIZE)) == (x >> ROOTS_DIV_1) for x in TESTS)
assert all((x // (ROOTS_MAX_2 // ROOTS_GROUP_SIZE)) == (x >> ROOTS_DIV_2) for x in TESTS)
assert all((x // (ROOTS_MAX_3 // ROOTS_GROUP_SIZE)) == (x >> ROOTS_DIV_3) for x in TESTS)

g0 = [(idx*(ROOTS_MAX_0//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g1 = [(idx*(ROOTS_MAX_1//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g2 = [(idx*(ROOTS_MAX_2//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]
g3 = [(idx*(ROOTS_MAX_3//ROOTS_GROUP_SIZE)) for idx in range(ROOTS_GROUP_SIZE)]

g0_a, g0_b, g0_c, g0_d, g0_e, *_, g0_n2, g0_n1, g0_n = g0
g1_a, g1_b, g1_c, g1_d, g1_e, *_, g1_n2, g1_n1, g1_n = g1
g2_a, g2_b, g2_c, g2_d, g2_e, *_, g2_n2, g2_n1, g2_n = g2
g3_a, g3_b, g3_c, g3_d, g3_e, *_, g3_n2, g3_n1, g3_n = g3

GROUP0_SAMPLE = f'{g0_a} {g0_b} {g0_c} {g0_d} {g0_e} ... {g0_n2} {g0_n1} {g0_n}'
GROUP1_SAMPLE = f'{g1_a} {g1_b} {g1_c} {g1_d} {g1_e} ... {g1_n2} {g1_n1} {g1_n}'
GROUP2_SAMPLE = f'{g2_a} {g2_b} {g2_c} {g2_d} {g2_e} ... {g2_n2} {g2_n1} {g2_n}'
GROUP3_SAMPLE = f'{g3_a} {g3_b} {g3_c} {g3_d} {g3_e} ... {g3_n2} {g3_n1} {g3_n}'

print(f'''
#define ROOTS_GROUP_SIZE {ROOTS_GROUP_SIZE}

#define ROOTS_GROUPS_REMAINING_0 {ROOTS_GROUPS_REMAINING_0}ULL
#define ROOTS_GROUPS_REMAINING_1 {ROOTS_GROUPS_REMAINING_1}ULL
#define ROOTS_GROUPS_REMAINING_2 {ROOTS_GROUPS_REMAINING_2}ULL
#define ROOTS_GROUPS_REMAINING_3 {ROOTS_GROUPS_REMAINING_3}ULL


// GROUP_MAX/GROUP_SIZE = BLOCK_SIZE
// 0*BLOCK_SIZE, 1*BLOCK_SIZE, 2*BLOCK_SIZE ... N*BLOCK_SIZE
// {GROUP0_SAMPLE}
// {GROUP1_SAMPLE}
// {GROUP2_SAMPLE}
// {GROUP3_SAMPLE}

#define ROOTS_MAX_0 {ROOTS_MAX_0}ULL
#define ROOTS_MAX_1 {ROOTS_MAX_1}ULL
#define ROOTS_MAX_2 {ROOTS_MAX_2}ULL
#define ROOTS_MAX_3 {ROOTS_MAX_3}ULL

#define ROOTS_DIV_0 {ROOTS_DIV_0}
#define ROOTS_DIV_1 {ROOTS_DIV_1}
#define ROOTS_DIV_2 {ROOTS_DIV_2}
#define ROOTS_DIV_3 {ROOTS_DIV_3}

#define ROOTS_GROUPS_OFFSET_0 {ROOTS_GROUPS_OFFSET_0}ULL
#define ROOTS_GROUPS_OFFSET_1 {ROOTS_GROUPS_OFFSET_1}ULL
#define ROOTS_GROUPS_OFFSET_2 {ROOTS_GROUPS_OFFSET_2}ULL
#define ROOTS_GROUPS_OFFSET_3 {ROOTS_GROUPS_OFFSET_3}ULL
''')


C_SIZE_MIN = 32

def put_idx(size):

    size -= C_SIZE_MIN

    assert (size >> ROOTS_DIV_0) == (size // (ROOTS_MAX_0//ROOTS_GROUP_SIZE))
    assert (size >> ROOTS_DIV_1) == (size // (ROOTS_MAX_1//ROOTS_GROUP_SIZE))
    assert (size >> ROOTS_DIV_2) == (size // (ROOTS_MAX_2//ROOTS_GROUP_SIZE))
    assert (size >> ROOTS_DIV_3) == (size // (ROOTS_MAX_3//ROOTS_GROUP_SIZE))

    if (size <= ROOTS_MAX_0) : return (size >> ROOTS_DIV_0) + ROOTS_GROUPS_OFFSET_0
    if (size <= ROOTS_MAX_1) : return (size >> ROOTS_DIV_1) + ROOTS_GROUPS_OFFSET_1
    if (size <= ROOTS_MAX_2) : return (size >> ROOTS_DIV_2) + ROOTS_GROUPS_OFFSET_2
    if (size <= ROOTS_MAX_3) : return (size >> ROOTS_DIV_3) + ROOTS_GROUPS_OFFSET_3

    return ROOTS_GROUPS_OFFSET_4 - 1

def get_idx(size):

    size -= C_SIZE_MIN

    assert (size >> ROOTS_DIV_0) == (size // (ROOTS_MAX_0//ROOTS_GROUP_SIZE))
    assert (size >> ROOTS_DIV_1) == (size // (ROOTS_MAX_1//ROOTS_GROUP_SIZE))
    assert (size >> ROOTS_DIV_2) == (size // (ROOTS_MAX_2//ROOTS_GROUP_SIZE))
    assert (size >> ROOTS_DIV_3) == (size // (ROOTS_MAX_3//ROOTS_GROUP_SIZE))

    assert (not not (size & ROOTS_GROUPS_REMAINING_0)) == (size % (ROOTS_MAX_0//ROOTS_GROUP_SIZE) != 0)
    assert (not not (size & ROOTS_GROUPS_REMAINING_1)) == (size % (ROOTS_MAX_1//ROOTS_GROUP_SIZE) != 0)
    assert (not not (size & ROOTS_GROUPS_REMAINING_2)) == (size % (ROOTS_MAX_2//ROOTS_GROUP_SIZE) != 0)
    assert (not not (size & ROOTS_GROUPS_REMAINING_3)) == (size % (ROOTS_MAX_3//ROOTS_GROUP_SIZE) != 0)

    if (size <= ROOTS_MAX_0) : return (size >> ROOTS_DIV_0) + (not not (size & ROOTS_GROUPS_REMAINING_0)) + ROOTS_GROUPS_OFFSET_0
    if (size <= ROOTS_MAX_1) : return (size >> ROOTS_DIV_1) + (not not (size & ROOTS_GROUPS_REMAINING_1)) + ROOTS_GROUPS_OFFSET_1
    if (size <= ROOTS_MAX_2) : return (size >> ROOTS_DIV_2) + (not not (size & ROOTS_GROUPS_REMAINING_2)) + ROOTS_GROUPS_OFFSET_2
    if (size <= ROOTS_MAX_3) : return (size >> ROOTS_DIV_3) + (not not (size & ROOTS_GROUPS_REMAINING_3)) + ROOTS_GROUPS_OFFSET_3

    return ROOTS_GROUPS_OFFSET_4 - 1

TESTS = range(C_SIZE_MIN, C_SIZE_MIN+70)

print('/*')
print('SIZE ' + ''.join('%4d' % x           for x in TESTS))
print('PUT  ' + ''.join('%4d' % put_idx(x)  for x in TESTS))
print('GET  ' + ''.join('%4d' % get_idx(x)  for x in TESTS))
print('*/')

all(map(get_idx, range(0,
                       ROOTS_MAX_0 + 65536 + ROOTS_GROUP_SIZE + C_SIZE_MIN)))

all(map(get_idx, range(ROOTS_MAX_1 - 65536 - ROOTS_GROUP_SIZE - C_SIZE_MIN,
                       ROOTS_MAX_1 + 65536 + ROOTS_GROUP_SIZE + C_SIZE_MIN)))

all(map(get_idx, range(ROOTS_MAX_2 - 65536 - ROOTS_GROUP_SIZE - C_SIZE_MIN,
                       ROOTS_MAX_2 + 65536 + ROOTS_GROUP_SIZE + C_SIZE_MIN)))

all(map(get_idx, range(ROOTS_MAX_3 - 65536 - ROOTS_GROUP_SIZE - C_SIZE_MIN,
                       ROOTS_MAX_3 + 65536 + ROOTS_GROUP_SIZE + C_SIZE_MIN)))
