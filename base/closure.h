#ifndef CLOSURE_H
#define CLOSURE_H

// author: chuxiaofeng
// gcc扩展资料 http://www.csheeet.com/en/latest/notes/c_gnuext.html
// clang block资料 http://thirdcog.eu/pwcblocks/
// clang 需要将修改的upvalue加上__block修饰符
// clang 需要显示Block_release, 因为将block编译成struct, 堆内存

// 测试通过 gcc version 4.4.7 
// 测试通过 Apple LLVM version 8.0.0 (clang-800.0.42.1)
// 使用方法参见 closure_test.c


#ifdef __clang__
#define closure(name, ret_type, ...) ret_type (^name) __VA_ARGS__
#define lambda(ret_type, ...) (^ret_type __VA_ARGS__)
#include <Block.h>
#define closure_copy Block_copy
#define closure_release Block_release
#define closure_var __block
#else
#define closure(name, ret_type, ...) ret_type (*name) __VA_ARGS__
#define lambda(ret_type, ...) __extension__({ ret_type __fn__ __VA_ARGS__ __fn__; })
#define closure_copy
#define closure_release
#define closure_var
#endif


#endif