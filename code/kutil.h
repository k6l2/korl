#pragma once
/* Disambiguation of C++ static keyword.  Good idea, Casey! */
#define internal        static
#define local_persist   static
#define global_variable static
#define class_namespace static
/* Utility macro for getting the size of a C array */
#define CARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))
/* Defer statement for C++11
	Source: https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/ */
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
#define CONCAT_(x,y)    x##y
#define CONCAT(x,y)     CONCAT_(x,y)
#define DEFER_UNIQUE(x) CONCAT(x,__COUNTER__)
#define defer(code)     auto DEFER_UNIQUE(_defer_) = defer_func([&](){code;})
/* This is a really stupid hack to force the export of struct symbols when there 
	is no declared instances of the struct in the project.  If I can figure out 
	a way to force the linker to export struct symbols for structs that have no 
	variables defined (outside of internal code or implicitly via reinterpret 
	casting of memory), then this should be deleted probably.  Another option to 
	achieve this would be to use `__declspec( dllexport )` on MSVC for example, 
	but I'm hoping this method is fully portable.
	Example usage:
		struct TestStruct
		{
			// ... some data declared in here
		} FORCE_SYMBOL_EXPORT; */
#define FORCE_SYMBOL_EXPORT \
	CONCAT(hack_force_symbol_export_DO_NOT_USE_,__COUNTER__)
/* Custom assert support. platform implementation required! */
#define kassert(expression) do {\
	platformAssert(static_cast<bool>(expression));\
}while(0)
/* Logging support.  platform implementation required! */
#define KLOG(platformLogCategory, formattedString, ...) platformLog(\
             __FILE__, __LINE__, PlatformLogCategory::K_##platformLogCategory, \
             formattedString, ##__VA_ARGS__)
/* Universal primitive definitions with reasonable keystroke counts. */
#include <stdint.h>
using u8  = uint8_t;  static_assert(sizeof(u8)  == 1);
using u16 = uint16_t; static_assert(sizeof(u16) == 2);
using u32 = uint32_t; static_assert(sizeof(u32) == 4);
using u64 = uint64_t; static_assert(sizeof(u64) == 8);
using i8  = int8_t;   static_assert(sizeof(i8)  == 1);
using i16 = int16_t;  static_assert(sizeof(i16) == 2);
using i32 = int32_t;  static_assert(sizeof(i32) == 4);
using i64 = int64_t;  static_assert(sizeof(i64) == 8);
using f32 = float;    static_assert(sizeof(f32) == 4);
using f64 = double;   static_assert(sizeof(f64) == 8);
/* @TODO: allow the game to define the SoundSample size in the build script? */
using SoundSample = i16;
namespace kutil
{
	/**
	 * @return a string representing everything in `filePath` after the last 
	 *         directory separation character ('/' or '\\').  If no directory 
	 *         characters exist in the string, the return value is `filePath`.
	*/
	const char* fileName(const char* filePath);
	/**
	 * upon execution, `data` will be stored in an array at the position 
	 * currently pointed to by `*bufferCursor`, and `*bufferCursor` is 
	 * incremented by `sizeof(data)`
	 * @param data the variable to pack
	 * @param bufferCursor pointer to the address of the moving current byte 
	 *                     within an array
	 * @param bufferEnd pointer to the address immediately following the last 
	 *                  byte within the buffer which `*bufferCursor` is pointing 
	 *                  to.  `*bufferEnd` should NEVER be modified, and netPack 
	 *                  will fail if the call is about to violate this axiom.
	 * @return the # of bytes packed into `*bufferCursor`
	 */
	u32 netPack(u64 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(u32 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(u16 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(u8 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(i64 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(i32 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(i16 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(i8 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(f32 data, u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netPack(f64 data, u8** bufferCursor, const u8* bufferEnd);
	/* this template prevents calls to netPack which don't precisely match the 
		data parameter.  Source: https://stackoverflow.com/a/12877589 */
	template<class T>
	u32 netPack(T data, u8** bufferCursor, const u8* bufferEnd) = delete;
	/**
	 * upon execution, a value is extracted from an array defined by the 
	 * position currently pointed to by `*bufferCursor`, and `*bufferCursor` 
	 * is incremented by `sizeof(*o_data)`
	 * @param o_data the return value which receives the unpacked data
	 * @param bufferCursor pointer to the address of the moving current byte 
	 *                       within `dataBuffer`
	 * @param bufferEnd pointer to the address immediately following the last 
	 *                  byte within the buffer which `*bufferCursor` is pointing 
	 *                  to.  `*bufferEnd` should NEVER be read, and netUnpack 
	 *                  will fail if the call is about to violate this axiom.
	 * @return the # of bytes unpacked from `*bufferCursor`
	 */
	u32 netUnpack(u64* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(u32* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(u16* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(u8* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(i64* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(i32* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(i16* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(i8* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(f32* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/** See above for documentation */
	u32 netUnpack(f64* o_data, const u8** bufferCursor, const u8* bufferEnd);
	/* this template prevents calls to netUnpack which don't precisely match the 
		data parameter.  Source: https://stackoverflow.com/a/12877589 */
	template<class T>
	u32 netUnpack(T data, const u8** bufferCursor, const u8* bufferEnd) 
		= delete;
}