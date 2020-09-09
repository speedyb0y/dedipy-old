/*

*/

#ifndef typeof
#define typeof __typeof__
#endif

#define loop while (1)
#define elif else if

typedef unsigned int uint;

typedef long long int intll;
typedef unsigned long long int uintll;
typedef signed long long int sintll;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define _LINE(x) #x
#define LINE(x) _LINE(x)

#ifndef LOG_PREPEND
#define LOG_PREPEND
#endif

#ifdef LOG_PREPEND_ARGS
#define _LOG_PREPEND_ARGS , LOG_PREPEND_ARGS
#else
#define LOG_PREPEND_ARGS
#define _LOG_PREPEND_ARGS
#endif

#define log(fmt, ...) ({ char b[4096]; write(STDERR_FILENO, b, (size_t)snprintf(b, sizeof(b), LOG_PREPEND fmt "\n" _LOG_PREPEND_ARGS, ##__VA_ARGS__)); })

// TODO: FIXME: name fmt \n, nameargs, ##__VA_ARGS__
#define DBGPRINT(str) write(STDOUT_FILENO, __FILE__ ":- " str "\n", sizeof(__FILE__ ":- " str))
#define dbg(x, ...) ({ char b[4096]; write(STDERR_FILENO, b, (size_t)snprintf(b, sizeof(b), "%20s %-30s %5d " LOG_PREPEND x "\n", __FILE__, __func__, __LINE__ _LOG_PREPEND_ARGS, ##__VA_ARGS__)); })

static inline u64 rdtsc (void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

// TODO: FIXME: builtion choose expression -> int 0 / 1

#define assert(condition) ({ if (!(condition)) { write(STDERR_FILENO, "\n" __FILE__ ":" LINE(__LINE__) " ASSERT FAILED: " #condition "\n", sizeof("\n" __FILE__ ":" LINE(__LINE__) " ASSERT FAILED: " #condition "\n")); abort(); } })

// TODO: FIXME: :S
#define abort_group() ({ kill(0, SIGABRT); kill(0, SIGKILL); abort(); })

#define fatal(reason)       ({ write(2, "FATAL: " reason "\n", sizeof("FATAL: " reason)); abort(); })
#define fatal_group(reason) ({ write(2, "FATAL: " reason "\n", sizeof("FATAL: " reason)); abort_group(); })

#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif
#ifndef MAP_HUGE_1GB
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)
#endif

#define ctz_u64(v) ((u64)__builtin_ctzll(v))

#define popcount64(v) __builtin_popcountll((u64)v)

typedef atomic_int int_atomic;
