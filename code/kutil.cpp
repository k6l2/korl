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
internal void netPackCommon(const u8* dataBytes, size_t dataSize, 
                            u8** ppPacketBuffer, u8* dataBuffer, 
                            size_t dataBufferSize)
{
	/* first, ensure that we aren't about to go out-of-bounds of the 
		dataBuffer */
	const size_t currentDataIndex = 
		kmath::safeTruncateU64(*ppPacketBuffer - dataBuffer);
	kassert(currentDataIndex <= dataBufferSize - dataSize);
	/* now that it's safe to do so, pack the bytes in big-endian order (network 
		byte order) */
	if(isBigEndian())
	{
		for(u8 b = 0; b < dataSize; b++)
			(*ppPacketBuffer)[b] = dataBytes[b];
	}
	else
	{
		for(u8 b = 0; b < dataSize; b++)
			(*ppPacketBuffer)[b] = dataBytes[dataSize - 1 - b];
	}
	*ppPacketBuffer += dataSize;
}
void kutil::netPack(u64 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
void kutil::netPack(u32 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
void kutil::netPack(u16 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
void kutil::netPack(i64 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
void kutil::netPack(i32 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
void kutil::netPack(i16 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
void kutil::netPack(f32 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
void kutil::netPack(f64 data, u8** ppPacketBuffer, u8* dataBuffer, 
                    size_t dataBufferSize)
{
	netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), ppPacketBuffer, 
	              dataBuffer, dataBufferSize);
}
internal void netUnpackCommon(u8* resultBytes, size_t resultSize, 
                              u8** ppPacketBuffer, u8* dataBuffer, 
                              size_t dataBufferSize)
{
	/* first, ensure that we aren't about to go out-of-bounds of the 
		dataBuffer */
	const size_t currentDataIndex = 
		kmath::safeTruncateU64(*ppPacketBuffer - dataBuffer);
	kassert(currentDataIndex <= dataBufferSize - resultSize);
	/* now that it's safe to do so, unpack the bytes from big-endian order 
		(network byte order) to our native byte order */
	if(isBigEndian())
	{
		for(u8 b = 0; b < resultSize; b++)
			resultBytes[b] = (*ppPacketBuffer)[b];
	}
	else
	{
		for(u8 b = 0; b < resultSize; b++)
			resultBytes[resultSize - 1 - b] = (*ppPacketBuffer)[b];
	}
	*ppPacketBuffer += resultSize;
}
u64 kutil::netUnpackU64(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	u64 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}
u32 kutil::netUnpackU32(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	u32 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}
u16 kutil::netUnpackU16(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	u16 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}
i64 kutil::netUnpackI64(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	i64 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}
i32 kutil::netUnpackI32(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	i32 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}
i16 kutil::netUnpackI16(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	i16 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}
f32 kutil::netUnpackF32(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	f32 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}
f64 kutil::netUnpackF64(u8** ppPacketBuffer, u8* dataBuffer, 
                        size_t dataBufferSize)
{
	f64 result = 0;
	netUnpackCommon(reinterpret_cast<u8*>(&result), sizeof(result), 
	                ppPacketBuffer, dataBuffer, dataBufferSize);
	return result;
}