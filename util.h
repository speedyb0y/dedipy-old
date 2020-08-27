/*

*/

#define loop while (1)
#define elif else if

typedef unsigned int uint;

typedef unsigned long long int uintll;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define DBGPRINT(str) write(STDOUT_FILENO, str "\n", sizeof(str))

static inline u64 rdtsc(void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}
