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

#define _LINE(x) #x
#define LINE(x) _LINE(x)

// TODO: FIXME: name fmt \n, nameargs, ##__VA_ARGS__
#if DEBUG
#define DBGPRINT(str) write(STDOUT_FILENO, __FILE__ ":- " str "\n", sizeof(__FILE__ ":- " str))
#define DBGPRINTF(x, ...) ({ char b[4096]; write(STDERR_FILENO, b, snprintf(b, sizeof(b), "%20s %-30s %5d " x "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)); })
#else
#define DBGPRINT(str) ({ })
#define DBGPRINTF(fmt, ...) ({ })
#endif

#if DEBUG >= 2
#define DBGPRINTF2 DBGPRINTF
#else
#define DBGPRINTF2(fmt, ...) ({ })
#endif

#if DEBUG >= 3
#define DBGPRINTF3 DBGPRINTF
#else
#define DBGPRINTF3(fmt, ...) ({ })
#endif

static inline u64 rdtsc(void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

// TODO: FIXME: builtion choose expression -> int 0 / 1

#define ASSERT(condition) ({ if (!(condition)) { write(STDERR_FILENO, "\n" __FILE__ ":" LINE(__LINE__) " ASSERT FAILED: " #condition "\n", sizeof("\n" __FILE__ ":" LINE(__LINE__) " ASSERT FAILED: " #condition "\n")); abort(); } })

#define fatal(reason) ({ write(2, reason "\n", sizeof(reason)); abort(); })

/*
 TODO: FIXME: forçar compilacao a falhar :/
#define _CHUNK(t, c) \
    __builtin_choose_expr(( \
        __builtin_types_compatible_p(typeof(c), Chunk*) || \
        __builtin_types_compatible_p(typeof(c), ChunkUnkn*) || \
        __builtin_types_compatible_p(typeof(c), ChunkFree*) || \
        __builtin_types_compatible_p(typeof(c), ChunkUsed*) \
        ), (t*)(c), (long double)1 )
*/

#define _FORCE(t, c) __builtin_choose_expr(__builtin_types_compatible_p(t, typeof(c)), (c), (void)0)

#define FORCE_U64(c) _FORCE(u64, c)
