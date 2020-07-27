#pragma once
#define internal        static
#define local_persist   static
#define global_variable static
#define class_namespace static
#define CARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
// Obtain the file name excluding the path //
//	Source: https://stackoverflow.com/a/8488201
#include <cstring>
#define __FILENAME__ ( strrchr(__FILE__, '\\') \
	? (strrchr(__FILE__, '\\') + 1) \
	: ( (strrchr(__FILE__, '/' ) \
		? strrchr(__FILE__, '/' ) + 1 \
		: __FILE__) ) )
// Defer statement for C++11   //
//	Source: https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
template <typename F>
struct privDefer {
	F f;
	privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};
template <typename F>
privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}
#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})
// custom assert support //
///TODO: improve this to allow platform-specific behavior like trigger debugger?
#if INTERNAL_BUILD || SLOW_BUILD
	#define kassert(expression) do \
	{\
		if(!(expression))\
		{\
			*(int*)0 = 0;\
		}\
	}while(0)
#else
	#define kassert(expression) do {}while(0)
#endif
// These must be re-defined in the appropriate compilation units //
#define KLOG(platformLogCategory, formattedString, ...)
#include <stdint.h>
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using SoundSample = i16;