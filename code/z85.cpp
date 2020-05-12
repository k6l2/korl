#include "z85.h"
namespace z85
{
	internal const i8 ENCODE_LOOKUP_TABLE[] = "0123456789abcdefghijklmnopqrstuv"
		"wxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";
	internal const u8 INVALID_ASCII = 0xFF;
	internal const u8 DECODE_LOOKUP_TABLE[128] = {
		// NULL(0) -> UnitSeparator(31)
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		INVALID_ASCII, INVALID_ASCII, INVALID_ASCII, INVALID_ASCII,
		// SPACE(32) -> '/'(47)
		INVALID_ASCII, 68, INVALID_ASCII, 84, 83, 82, 72, INVALID_ASCII, 75, 76,
		70, 65, INVALID_ASCII, 63, 62, 69,
		// '0'(48) -> '@'(64)
		0,1,2,3,4,5,6,7,8,9, 64, INVALID_ASCII, 73, 66, 74, 71, 81,
		// 'A'(65) -> '`'(96)
		36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,
		60,61, 77, INVALID_ASCII, 78, 67, INVALID_ASCII, INVALID_ASCII,
		// 'a'(97) -> DELETE(127)
		10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,
		34,35, 79, INVALID_ASCII, 80, INVALID_ASCII, INVALID_ASCII };
	internal bool isValidZ85AsciiByteCount(u64 asciiByteCount)
	{
		return asciiByteCount % 5 == 0;
	}
	internal u64 encodedFileSizeBytes(u64 rawFileByteCount)
	{
		u64 binaryFrameCount = rawFileByteCount / 4;
		if (rawFileByteCount % 4)
		{
			// add an extra frame if we can't perfectly divide the raw file into 
			//	groups of 4 bytes per frame
			++binaryFrameCount;
		}
		// 5 ascii characters per frame!
		return binaryFrameCount * 5;
	}
	internal u64 decodedFileSizeBytes(u64 asciiByteCount)
	{
		/// ASSUMPTION: all encoded files % 5 == 0
		if(!isValidZ85AsciiByteCount(asciiByteCount))
			return 0;
		return (asciiByteCount / 5) * 4;
	}
	internal void encode(const i8* input, u64 inputByteSize, i8* output)
	{
		for (u64 b = 0; b < inputByteSize; b += 4)
		{
			// build the next unsigned 32-bit # from the input
			u32 intInput = 0;
			for (size_t c = 0; c < 4; c++)
			{
				intInput <<= 8;
				intInput |= static_cast<u8>(input[b + c]);
			}
			// find the 5 base-85 remainders, and write them to the output
			for (int c = 4; c >= 0; c--)
			{
				const u32 remainder = intInput % 85;
				intInput /= 85;
				output[c] = ENCODE_LOOKUP_TABLE[remainder];
			}
			// increment the output pointers, we assume that output array is 
			//	properly sized //
			output += 5;
		}
	}
	internal bool decode(const i8* input, i8* output)
	{
		while (*input)
		{
			// first, we need to translate from ascii char => base-85 # //
			// then, build the 32-bit ouput # from the base-85 digits //
			unsigned long intOutput = 0;
			for (size_t c = 0; c < 5; c++)
			{
				const u32 base85Digit = DECODE_LOOKUP_TABLE[input[c]];
				if(base85Digit == INVALID_ASCII)
				{
					return false;
				}
				const size_t base85Place = 4 - c;
				intOutput += base85Digit * 
					static_cast<u32>(pow(85, static_cast<double>(base85Place)));
			}
			// output each digit byte to output array //
			for (int c = 3; c >= 0; c--)
			{
				output[c] = static_cast<i8>(intOutput & 0xFF);
				intOutput >>= 8;
			}
			// increment input & output //
			// 5 bytes of input == 4 bytes of output data
			input += 5;
			output += 4;
		}
		return true;
	}
}