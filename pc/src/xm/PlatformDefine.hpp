#pragma once
#if defined(_MSC_VER)
    #define XM_COMPILER_MSVC
    #define XM_COMPILER_NAME "msvc"
#elif defined(__GNUC__)
    #define XM_COMPILER_GCC
    #define XM_COMPILER_NAME "gcc"
#elif defined(__clang__)
    #define XM_COMPILER_CLANG
    #define XM_COMPILER_NAME "clang"
#else
    #error "unknown compiler"
#endif

#if defined(_WIN32)
    #define XM_OS_WINDOWS
    #define XM_OS_NAME "windows"
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define XM_OS_IOS
        #define XM_OS_NAME "ios"
    #elif TARGET_OS_MAC
        #define XM_OS_MAC
        #define XM_OS_NAME "mac"
    #else
        #error "unknown apple os"
    #endif
#elif defined(__ANDROID__)
    #define XM_OS_ANDROID
    #define XM_OS_NAME "android"
#elif defined(__linux__)
    #define XM_OS_LINUX
    #define XM_OS_NAME "linux"
#else
    #error "unknown os"
#endif

#if defined(XM_COMPILER_CLANG) || defined(XM_COMPILER_GCC)
    #define XM_PRINTF_FORMAT_CHECK(I0, I1) __attribute__((format(printf, I0, I1)))
#else
    #define XM_PRINTF_FORMAT_CHECK(...)
#endif

#if defined(XM_COMPILER_MSVC)
    #define XM_FORCE_INLINE __forceinline
#elif defined(XM_COMPILER_GCC) || defined(XM_COMPILER_CLANG)
    #define XM_FORCE_INLINE inline __attribute__((always_inline))
#else
    #define XM_FORCE_INLINE inline
#endif
