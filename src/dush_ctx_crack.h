#ifndef DUSH_CTX_CRACK_H
#define DUSH_CTX_CRACK_H

////////////////////////////////
//~ Context Crack: MSVC Extraction

#if defined(_MSC_VER)

# define COMPILER_MSVC 1

# if defined(_WIN32)
#  define OS_WINDOWS 1
# else
#  error _MSC_VER is defined, but _WIN32 is not. This setup is not supported.
# endif

////////////////////////////////
//~ Context Crack: Clang Extraction

#elif defined(__clang__)

# define COMPILER_CLANG 1

# if defined(__APPLE__) && defined(__MACH__)
#  define OS_MAC 1
# elif defined(__gnu_linux__)
#  define OS_LINUX 1
# else
#  error __clang__ is defined, but one of {__APPLE__, __gnu_linux__} is not. This setup is not supported.
# endif

////////////////////////////////
//~ Context Crack: GCC Extraction

#elif defined(__GNUC__) || defined(__GNUG__)

# define COMPILER_GCC 1

# if defined(__gnu_linux__)
#  define OS_LINUX 1
# else
#  error __GNUC__ or __GNUG__ is defined, but __gnu_linux__ is not. This setup is not supported.
# endif

#else
# error Compiler is not supported. _MSC_VER, __clang__, __GNUC__, or __GNUG__ must be defined.
#endif

////////////////////////////////
//~ Context Crack: Preprocessor

#if !defined(__has_include)
# define __has_include(x) 0
#endif

#define HAS_INCLUDE __has_include

////////////////////////////////
//~ rjf: Sanitizers

#if defined(__SANITIZE_ADDRESS__)
# define ASAN_ENABLED 1
#else
# define ASAN_ENABLED 0
#endif

////////////////////////////////
//~ Context Crack: Zero

#if !defined(COMPILER_MSVC)
# define COMPILER_MSVC 0
#endif
#if !defined(COMPILER_GCC)
# define COMPILER_GCC 0
#endif
#if !defined(COMPILER_CLANG)
# define COMPILER_CLANG 0
#endif
#if !defined(OS_WINDOWS)
# define OS_WINDOWS 0
#endif
#if !defined(OS_LINUX)
# define OS_LINUX 0
#endif
#if !defined(OS_MAC)
# define OS_MAC 0
#endif

////////////////////////////////
//~ Context Crack: Build params

#if !defined(AGGRESSIVE_ASSERTS)
# define AGGRESSIVE_ASSERTS 1
#endif

#if !defined(AGGRESSIVE_MEM_ZERO)
# define AGGRESSIVE_MEM_ZERO 1
#endif

#endif
