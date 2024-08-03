#ifndef __CORLIB_MACRO_H__
#define __CORLIB_MACRO_H__

#include <string.h>
#include <assert.h>
#include <iostream> // 添加对 iostream 的包含，以便使用 std::cerr

#if defined __GNUC__ || defined __llvm__
/// LIKELY 宏的封装, 告诉编译器优化, 条件大概率成立
#   define CORLIB_LIKELY(x)       __builtin_expect(!!(x), 1)
/// UNLIKELY 宏的封装, 告诉编译器优化, 条件大概率不成立
#   define CORLIB_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define CORLIB_LIKELY(x)       (x)
#   define CORLIB_UNLIKELY(x)     (x)
#endif

/// 断言宏封装
#define CORLIB_ASSERT(x) \
    if(CORLIB_UNLIKELY(!(x))) { \
        std::cerr << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << std::endl; \
        assert(x); \
    }

/// 断言宏封装, 带额外信息
#define CORLIB_ASSERT2(x, w) \
    if(CORLIB_UNLIKELY(!(x))) { \
        std::cerr << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << std::endl; \
        assert(x); \
    }

#endif
