#pragma once
#include "global-defines.h"
/*
	Z85 converts binary data to ascii data,
		in a format which can be saved as a 
		const char* const variable in source code.

	Formal specification of Z85 encode/decode:
		https://rfc.zeromq.org/spec:32/Z85/
*/
namespace z85
{
	internal bool isValidZ85AsciiByteCount(u64 asciiByteCount);
	internal u64 encodedFileSizeBytes(u64 rawFileByteCount);
	/** @return 0 if the asciiByteCount is invalid. */
	internal u64 decodedFileSizeBytes(u64 asciiByteCount);
	/** 
	 * @param output must point to an array whose size matches the output of a
	 *               call to encodedFileSizeBytes(inputByteSize)
	 * */
	internal void encode(const i8* input, u64 inputByteSize, i8* output);
	/**
	 * @param input Must be a NULL-terminated array with a valid z85 size, 
	 *              containing only valid z85 ASCII characters.
	 * @param output Must point to an array whose size matches the output of a
	 *               call to decodedFileSizeBytes(SIZE_OF(input)), where SIZE_OF
	 *               computes the size of the input array.
	 * @return false if an invalid z85 character is found, true otherwise.
	 * */
	internal bool decode(const i8* input, i8* output);
};
