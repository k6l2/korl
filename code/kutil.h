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
#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})
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
	 * upon execution, `data` will be stored in the array `dataBuffer` at the 
	 * position currently pointed to by `*ppPacketBuffer`, and `*ppPacketBuffer` 
	 * is incremented by `sizeof(data)`
	 * @param data the variable to pack
	 * @param ppPacketBuffer pointer to the address of the moving current byte 
	 *                       within `dataBuffer`
	 * @param dataBuffer the first element of the array to which 
	 *                   `*ppPacketBuffer` is writing to
	 */
	void netPack(u64 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/** See above for documentation */
	void netPack(u32 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/** See above for documentation */
	void netPack(u16 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/** See above for documentation */
	void netPack(i64 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/** See above for documentation */
	void netPack(i32 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/** See above for documentation */
	void netPack(i16 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/** See above for documentation */
	void netPack(f32 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/** See above for documentation */
	void netPack(f64 data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize);
	/* this template prevents calls to netPack which don't precisely match the 
		data parameter.  Source: https://stackoverflow.com/a/12877589 */
	template<class T>
	void netPack(T data, u8** ppPacketBuffer, u8* dataBuffer, 
	             size_t dataBufferSize) = delete;
	/**
	 * upon execution, an `u64` variable is extracted from `dataBuffer` at the 
	 * position currently pointed to by `*ppPacketBuffer`, and `*ppPacketBuffer` 
	 * is incremented by `sizeof(u64)`
	 * @param ppPacketBuffer pointer to the address of the moving current byte 
	 *                       within `dataBuffer`
	 * @param dataBuffer the first element of the array to which 
	 *                   `*ppPacketBuffer` is reading from
	 */
	u64 netUnpackU64(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
	/** See above for documentation */
	u32 netUnpackU32(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
	/** See above for documentation */
	u16 netUnpackU16(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
	/** See above for documentation */
	i64 netUnpackI64(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
	/** See above for documentation */
	i32 netUnpackI32(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
	/** See above for documentation */
	i16 netUnpackI16(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
	/** See above for documentation */
	f32 netUnpackF32(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
	/** See above for documentation */
	f64 netUnpackF64(u8** ppPacketBuffer, u8* dataBuffer, 
	                 size_t dataBufferSize);
}