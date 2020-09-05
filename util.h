/*

*/

#undef assert

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef typeof
#define typeof __typeof__
#endif

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

#ifndef DBG_PREPEND
#define DBG_PREPEND
#endif

#ifdef DBG_PREPEND_ARGS
#define _DBG_PREPEND_ARGS , DBG_PREPEND_ARGS
#else
#define DBG_PREPEND_ARGS
#define _DBG_PREPEND_ARGS
#endif

// TODO: FIXME: name fmt \n, nameargs, ##__VA_ARGS__
#if DEBUG
#define DBGPRINT(str) write(STDOUT_FILENO, __FILE__ ":- " str "\n", sizeof(__FILE__ ":- " str))
#define dbg(fmt, ...) ({ char b[8192]; write(STDERR_FILENO, b, (size_t)snprintf(b, sizeof(b), "%20s %-30s %5d " DBG_PREPEND fmt "\n", __FILE__, __func__, __LINE__ _DBG_PREPEND_ARGS, ##__VA_ARGS__)); })
#else
#define DBGPRINT(str) ({ })
#define dbg(fmt, ...) ({ })
#endif

#if DEBUG >= 2
#define dbg2 dbg
#else
#define dbg2(fmt, ...) ({ })
#endif

#if DEBUG >= 3
#define dbg3 dbg
#else
#define dbg3(fmt, ...) ({ })
#endif

static inline u64 rdtsc(void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

// TODO: FIXME: usando o builtin choose expression, se for constante expression, verificar em compile time =]
#define assert(condition) ({ if (!(condition)) { write(STDERR_FILENO, "\n" __FILE__ ":" LINE(__LINE__) " ASSERT FAILED: " #condition "\n", sizeof("\n" __FILE__ ":" LINE(__LINE__) " assert FAILED: " #condition "\n")); abort(); } })

// TODO: FIXME: :S
#define abort_group() ({ kill(0, SIGABRT); kill(0, SIGKILL); abort(); })

#define fatal(reason)       ({ write(2, "FATAL: " reason "\n", sizeof("FATAL: " reason)); abort(); })
#define fatal_group(reason) ({ write(2, "FATAL: " reason "\n", sizeof("FATAL: " reason)); abort_group(); })

static inline int in_mem (const void* const mem, const u64 memSize, const void* const mem2, const u64 memSize2)
    { return mem2 <= mem && (mem + memSize) <= (mem2 + memSize2); }

// E É UM ELEMENTO DA ARRAY A, DE N ELEMENTOS DE TAMANHO S
static inline int is_element_of_array (const void* const e, const void* const a, const u64 n, const u64 s)
    { return (a <= e) && (e < (a + n*s)) && ((uintptr_t)e % sizeof(s)) == 0; }

// PROTEGE A FUNCAO
//#define is_element_of_array(e, a, n) __builtin_choose_expr ( __builtin_types_compatible_p(typeof(e), typeof(*a)), is_element_of_array((e), (a), (n), sizeof(*a)), (void)0 )
#define is_element_of_array(e, a, n) is_element_of_array(e, a, n, n*sizeof(*a))

// ASSIM COMO UMA FUNCAO, EXECUTA OS ARGUMENTOS ANTES DE EXECUTAR!!!
#define is_from_to(min, value, max)      ({ typeof(value) __value;  ((min) <= (__value = (value)) && __value <= (max)); })
#define is_from_between(min, value, max) ({ typeof(value) __value;  ((min) <  (__value = (value)) && __value <  (max)); })

typedef int bool;

#define is_aligned(addr, alignment) (!!((addr) % (alignment)))

#define is_aligned_ptr(addr, alignment) (!!((uintptr_t)(addr) % (alignment)))
