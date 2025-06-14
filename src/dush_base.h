#ifndef DUSH_BASE_H
#define DUSH_BASE_H

////////////////////////////////
//~ System Headers

#if OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#elif OS_LINUX
# include <sys/mman.h>
# include <sys/ioctl.h>
# include <sys/unistd.h>
# if HAS_INCLUDE(<libexplain/pathconf.h>)
#  include <libexplain/pathconf.h>
# endif
#else
# error Platform not supported.
#endif

////////////////////////////////
//~ Standard Headers

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

////////////////////////////////
//~ fsize

static size_t fsize(FILE *fp);

////////////////////////////////
//~ Core

//- Keywords

#define cast(t) (t)

#if ASAN_ENABLED && COMPILER_MSVC
# define no_asan __declspec(no_sanitize_address)
#elif ASAN_ENABLED && (COMPILER_CLANG || COMPILER_GCC)
# define no_asan __attribute__((no_sanitize("address")))
#endif
#if !defined(no_asan)
# define no_asan
#endif

#if COMPILER_MSVC
# define per_thread __declspec(thread)
#elif COMPILER_CLANG
# define per_thread __thread
#elif COMPILER_GCC
# define per_thread __thread
#endif

#if OS_WINDOWS
# pragma section(".roglob", read)
# define read_only no_asan __declspec(allocate(".roglob"))
#else
# define read_only
#endif

#define alignof(t) _Alignof(t)

//- Integer/pointer/array/type manipulations

#define array_count(a) (i64)(sizeof(a)/sizeof((a)[0]))

#define bytes(n)     (   1ULL * n)
#define kilobytes(n) (1024ULL * bytes(n))
#define megabytes(n) (1024ULL * kilobytes(n))
#define gigabytes(n) (1024ULL * megabytes(n))

//- Clamps, mins, maxes

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define clamp_top(a, b) min(a, b)
#define clamp_bot(a, b) max(a, b)

#define clamp(a, x, b) clamp_bot(a, clamp_top(x, b))

//- Assertions

#define allow_break() do { int __x__ = 0; (void)__x__; } while (0)
#define panic(...) assert(0)
#define unimplemented(...) assert(0)

#if OS_WINDOWS
#define force_break() __debugbreak()
#else
#define force_break() (*(volatile int *)0 = 0)
#endif

//- Linked list helpers

#define check_null(p) ((p)==0)
#define set_null(p) ((p)=0)

#define stack_push_n(f,n,next) (void)((n)->next=(f),(f)=(n))
#define stack_pop_nz(f,next,zchk) (void)(zchk(f)?0:((f)=(f)->next))

#define queue_push_nz(f,l,n,next,zchk,zset) (void)(zchk(f)?\
(((f)=(l)=(n)), zset((n)->next)):\
((l)->next=(n),(l)=(n),zset((n)->next)))
#define queue_push_front_nz(f,l,n,next,zchk,zset) (void)(zchk(f) ? (((f) = (l) = (n)), zset((n)->next)) :\
((n)->next = (f)), ((f) = (n)))
#define queue_pop_nz(f,l,next,zset) (void)((f)==(l)?\
(zset(f),zset(l)):\
((f)=(f)->next))

#define dll_insert_npz(f,l,p,n,next,prev,zchk,zset) \
(void)(zchk(f) ? (((f) = (l) = (n)), zset((n)->next), zset((n)->prev)) :\
zchk(p) ? (zset((n)->prev), (n)->next = (f), (zchk(f) ? (0) : ((f)->prev = (n))), (f) = (n)) :\
((zchk((p)->next) ? (0) : (((p)->next->prev) = (n))), (n)->next = (p)->next, (n)->prev = (p), (p)->next = (n),\
((p) == (l) ? (l) = (n) : (0))))
#define dll_push_back_npz(first,last,elem,next_ident,prev_ident,zero_check,zero_set) dll_insert_npz(first,last,last,elem,next_ident,prev_ident,zero_check,zero_set)
#define dll_remove_npz(f,l,n,next,prev,zchk,zset) (void)(((f)==(n))?\
((f)=(f)->next, (zchk(f) ? (zset(l)) : zset((f)->prev))):\
((l)==(n))?\
((l)=(l)->prev, (zchk(l) ? (zset(f)) : zset((l)->next))):\
((zchk((n)->next) ? (0) : ((n)->next->prev=(n)->prev)),\
(zchk((n)->prev) ? (0) : ((n)->prev->next=(n)->next))))

#define stack_push(f,n)           stack_push_n(f,n,next)
#define stack_pop(f)              stack_pop_nz(f,next,check_null)

#define queue_push(f,l,n)         queue_push_nz(f,l,n,next,check_null,set_null)
#define queue_push_front(f,l,n)   queue_push_front_nz(f,l,n,next,check_null,set_null)
#define queue_pop(f,l)            queue_pop_nz(f,l,next,set_null)

#define dll_push_back(f,l,n)      dll_push_back_npz(f,l,n,next,prev,check_null,set_null)
#define dll_push_front(f,l,n)     dll_push_back_npz(l,f,n,prev,next,check_null,set_null)
#define dll_insert(f,l,p,n)       dll_insert_npz(f,l,p,n,next,prev,check_null,set_null)
#define dll_remove(f,l,n)         dll_remove_npz(f,l,n,next,prev,check_null,set_null)

//- Basic types

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef   int8_t  i8;
typedef  int16_t i16;
typedef  int32_t i32;
typedef  int64_t i64;

//- String and slice types

typedef struct SliceU8 SliceU8;
struct SliceU8 {
	i64  len;
	u8  *data;
};

typedef struct String String;
struct String {
	i64  len;
	u8  *data;
};

//- Integer math

static bool is_power_of_two(u64 i);
static u64  align_forward(u64 ptr, u64 alignment);
static u64  round_up_to_multiple_of_u64(u64 n, u64 r);
static i64  round_up_to_multiple_of_i64(i64 n, i64 r);

////////////////////////////////
//~ Memory

//- Memory types

// Note: If generic allocators were implemented, a .MODE_NOT_IMPLEMENTED error would be useful
// for operations not supported on a specific allocator.

// As a rule, all allocation functions that return 0 (could be either NULL or false) *must* set
// the last error to something. Errors .INVALID_ARGUMENT and .INVALID_POINTER are introduced
// for this reason.
typedef enum Alloc_Error {
	Alloc_Error_NONE = 0,
	
	// Set either when a call to an Operating System's allocation function fails,
	// or when a custom allocator cannot satisfy an allocation request.
	Alloc_Error_OUT_OF_MEMORY,
	
	// Set when an argument to an allocation call is invalid, such as when the size is 0.
	Alloc_Error_INVALID_ARGUMENT,
	
	// Set when a pointer argument to an allocation call is an invalid pointer or a pointer
	// that is not at the beginning of a block.
	Alloc_Error_INVALID_POINTER,
	Alloc_Error_COUNT,
} Alloc_Error;

//- Memory global variables

per_thread Alloc_Error last_alloc_error;

//- Memory procedures

static void *mem_reserve(u64 size);
static void *mem_commit(void *ptr, u64 size);
static void *mem_reserve_and_commit(u64 size);
static bool  mem_decommit(void *ptr, u64 size);
static bool  mem_release(void *ptr, u64 size);

static String last_alloc_error_string(void);

////////////////////////////////
//~ Arena

//- Arena constants

#if !defined(ARENA_COMMIT_GRANULARITY)
#define ARENA_COMMIT_GRANULARITY kilobytes(4)
#endif

#if !defined(ARENA_DECOMMIT_THRESHOLD)
#define ARENA_DECOMMIT_THRESHOLD megabytes(64)
#endif

#if !defined(DEFAULT_ARENA_RESERVE_SIZE)
#define DEFAULT_ARENA_RESERVE_SIZE gigabytes(1)
#endif

//- Arena types

typedef struct Arena Arena;
struct Arena {
	u8  *ptr;
	u64  pos;
	u64  cap;
	u64  peak;
	u64  commit_pos;
};

typedef struct Arena_Restore_Point Arena_Restore_Point;
struct Arena_Restore_Point {
	Arena *arena;
	u64    pos;
};

typedef struct Arena_Init_Params Arena_Init_Params;
struct Arena_Init_Params {
	u64 reserve_size;
};

//- Arena procedures

static bool _arena_init(Arena *arena, Arena_Init_Params params);
#define arena_init(arena, ...) _arena_init(arena, (Arena_Init_Params){ .reserve_size = DEFAULT_ARENA_RESERVE_SIZE, __VA_ARGS__ })
static bool arena_fini(Arena *arena);
static void arena_reset(Arena *arena);

static u64  arena_cap(Arena arena);
static u64  arena_pos(Arena arena);
static u64  arena_space(Arena arena);

static void *push_nozero(Arena *arena, u64 size, u64 alignment);
static void *push_zero(Arena *arena, u64 size, u64 alignment);

static void pop_to(Arena *arena, u64 pos);
static void pop_amount(Arena *arena, u64 amount);

static Arena_Restore_Point arena_begin_temp_region(Arena *arena);
static void arena_end_temp_region(Arena_Restore_Point point);

////////////////////////////////
//~ Scratch memory

//- Scratch memory constants

#if !defined(SCRATCH_ARENA_COUNT)
#define SCRATCH_ARENA_COUNT 2
#endif

#if !defined(SCRATCH_ARENA_RESERVE_SIZE)
#define SCRATCH_ARENA_RESERVE_SIZE gigabytes(8)
#endif

//- Scratch memory types

typedef Arena_Restore_Point Scratch;

//- Scratch memory variables

#if SCRATCH_ARENA_COUNT > 0
per_thread Arena scratch_arenas[SCRATCH_ARENA_COUNT];
per_thread Alloc_Error scratch_arenas_init_errors[SCRATCH_ARENA_COUNT];
#endif

//- Scratch memory functions

static Scratch scratch_begin(Arena **conflicts, i64 conflict_count);
static void    scratch_end(Scratch scratch);

////////////////////////////////
//~ Strings and slices

//- Slice functions

static SliceU8 make_sliceu8(u8 *data, i64 len);
static SliceU8 push_sliceu8(Arena *arena, i64 len);
static SliceU8 sliceu8_from_string(String s);
static SliceU8 sliceu8_clone(Arena *arena, SliceU8 s);

//- String functions

#define string_lit_expand(s)     s, (sizeof(s)-1)
#define string_expand(s)         cast(int)(s).len, (s).data

#define string_from_lit(s)       string(cast(u8 *)s, sizeof(s)-1)
#define string_from_cstring(s)   string(cast(u8 *)s, strlen(s))
#define string_from_lit_const(s)       {sizeof(s)-1, cast(u8 *)s}

static String string(u8 *data, i64 len);
static String push_string(Arena *arena, i64 len);
static String push_stringf(Arena *arena, char *fmt, ...);
static String push_stringf_va_list(Arena *arena, char *fmt, va_list args);
static String string_from_sliceu8(SliceU8 s);
static String string_clone(Arena *arena, String s);
static String strings_concat(Arena *arena, String *strings, i64 string_count);
static char *cstring_from_string(Arena *arena, String s);

static bool string_starts_with(String a, String b);
static bool string_ends_with(String a, String b);
static bool string_equals(String a, String b);
static bool string_equals_case_insensitive(String a, String b);

static i64 string_find_first(String s, u8 c);
static i64 string_count_occurrences(String s, u8 c);
static i64 string_contains(String s, u8 c);

static String string_skip(String s, i64 amount);
static String string_chop(String s, i64 amount);
static String string_stop(String s, i64 index);
static String string_skip_chop_whitespace(String s);
static String string_chop_past_last_slash(String s);

////////////////////////////////
//~ String Builder

//- String builder types

typedef struct String_Builder String_Builder;
struct String_Builder {
	u8  *data;
	i64  len;
	i64  cap;
};

//- String builder functions

static void string_builder_init(String_Builder *builder, SliceU8 backing);
static i64  string_builder_append(String_Builder *builder, String s);

static String string_from_builder(String_Builder builder);

#endif
