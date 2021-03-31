#include "kutil.h"
#include <cstring>
const char* kutil::fileName(const char* filePath)
{
	const char* result = strrchr(filePath, '\\');
	if(result)
	{
		result++;
	}
	else
	{
		result = strrchr(filePath, '/');
		if(result)
			result++;
		else
			result = filePath;
	}
	return result;
}
internal inline bool isBigEndian()
{
	const i32 i = 1;
	const u8*const iBytes = reinterpret_cast<const u8*>(&i);
	return iBytes[0] == 0;
}
internal u32 dataPackCommon(
	const u8* dataBytes, size_t dataSize, u8** bufferCursor, 
	const u8* bufferEnd, bool littleEndian)
{
	/* first, ensure that we aren't about to go out-of-bounds of the 
		dataBuffer */
	korlAssert(*bufferCursor + dataSize <= bufferEnd);
	/* now that it's safe to do so, pack the bytes in big-endian order (network 
		byte order) */
	if(isBigEndian() == !littleEndian)
		for(u8 b = 0; b < dataSize; b++)
			(*bufferCursor)[b] = dataBytes[b];
	else
		for(u8 b = 0; b < dataSize; b++)
			(*bufferCursor)[b] = dataBytes[dataSize - 1 - b];
	*bufferCursor += dataSize;
	return kmath::safeTruncateU32(dataSize);
}
u32 kutil::dataPack(
	u64 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	u32 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	u16 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	u8 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	i64 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	i32 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	i16 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	i8 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	f32 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataPack(
	f64 data, u8** bufferCursor, const u8* bufferEnd, bool littleEndian)
{
	return dataPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                      bufferCursor, bufferEnd, littleEndian);
}
internal u32 dataUnpackCommon(
	u8* resultBytes, size_t resultSize, const u8** bufferCursor, 
	const u8* bufferEnd, bool littleEndian)
{
	/* first, ensure that we aren't about to go out-of-bounds of the 
		dataBuffer */
	korlAssert(*bufferCursor + resultSize <= bufferEnd);
	/* now that it's safe to do so, unpack the bytes from big-endian order 
		(network byte order) to our native byte order */
	if(isBigEndian() == !littleEndian)
		for(u8 b = 0; b < resultSize; b++)
			resultBytes[b] = (*bufferCursor)[b];
	else
		for(u8 b = 0; b < resultSize; b++)
			resultBytes[resultSize - 1 - b] = (*bufferCursor)[b];
	*bufferCursor += resultSize;
	return kmath::safeTruncateU32(resultSize);
}
u32 kutil::dataUnpack(
	u64* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	u32* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	u16* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	u8* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	i64* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	i32* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	i16* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	i8* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	f32* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
u32 kutil::dataUnpack(
	f64* o_data, const u8** bufferCursor, const u8* bufferEnd, 
	bool littleEndian)
{
	return dataUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
	                        bufferCursor, bufferEnd, littleEndian);
}
size_t kutil::extractNonWhitespaceToken(char** pCStr, size_t cStrSize)
{
	const char*const cStrEnd = (*pCStr) + cStrSize;
	/* advance the cursor until we find a non-whitespace char */
	while(*pCStr < cStrEnd && isspace(**pCStr))
	{
		(*pCStr)++;
	}
	if(*pCStr >= cStrEnd)
		return 0;
	/* locate the first whitespace character after the token and nullify it so 
	 * we can actually use `*pCStr` as the token */
	size_t sizeOfToken = kmath::safeTruncateU64(cStrEnd - *pCStr);
	for(char* spaceSearchChar = (*pCStr) + 1; 
		*spaceSearchChar && spaceSearchChar < cStrEnd; spaceSearchChar++)
	{
		if(isspace(*spaceSearchChar))
		{
			*spaceSearchChar = '\0';
			sizeOfToken = kmath::safeTruncateU64(spaceSearchChar - *pCStr);
			break;
		}
	}
	return sizeOfToken;
}
void kutil::cStrToLowercase(char* cStr, size_t cStrSize)
{
	for(size_t c = 0; c < cStrSize; c++)
	{
		cStr[c] = kmath::safeTruncateI8(tolower(cStr[c]));
	}
}
void kutil::cStrToUppercase(char* cStr, size_t cStrSize)
{
	for(size_t c = 0; c < cStrSize; c++)
	{
		cStr[c] = kmath::safeTruncateI8(toupper(cStr[c]));
	}
}
void kutil::cStrRaiseUnderscores(char* cStr, size_t cStrSize)
{
	for(size_t c = 0; c < cStrSize; c++)
	{
		if(cStr[c] != '_')
			continue;
		cStr[c] = '-';
	}
}
size_t kutil::cStrCompareArray(
	const char* cStr, const char*const* cStrArray, size_t cStrArraySize)
{
	for(size_t a = 0; a < cStrArraySize; a++)
		if(strcmp(cStr, cStrArray[a]) == 0)
			return a;
	return cStrArraySize;
}
void kutil::cStrMakeFilesystemSafe(char* cStr, size_t cStrSize)
{
	const char*const cStrEnd = cStr + cStrSize;
	for(; cStr < cStrEnd; cStr++)
	{
		switch(*cStr)
		{
		case '\\': *cStr = '['; break;
		case '/':  *cStr = ']'; break;
		case ':':  *cStr = ';'; break;
		case '*':  *cStr = '.'; break;
		case '?':  *cStr = '~'; break;
		case '<':  *cStr = '{'; break;
		case '>':  *cStr = '}'; break;
		case '|':  *cStr = '`'; break;
		case '\"': *cStr = ','; break;
		}
	}
}