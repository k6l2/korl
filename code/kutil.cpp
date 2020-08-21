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
internal u32 netPackCommon(const u8* dataBytes, size_t dataSize, 
                           u8** ppPacketBuffer, u8* dataBuffer, 
                           u32 dataBufferSize)
{
	/* first, ensure that we aren't about to go out-of-bounds of the 
		dataBuffer */
	const u32 currentDataIndex = 
		kmath::safeTruncateU32(*ppPacketBuffer - dataBuffer);
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
	return kmath::safeTruncateU32(dataSize);
}
u32 kutil::netPack(u64 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(u32 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(u16 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(u8 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(i64 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(i32 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(i16 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(i8 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(f32 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netPack(f64 data, u8** ppPacketBuffer, u8* dataBuffer, 
                   u32 dataBufferSize)
{
	return netPackCommon(reinterpret_cast<u8*>(&data), sizeof(data), 
	                     ppPacketBuffer, dataBuffer, dataBufferSize);
}
internal u32 netUnpackCommon(u8* resultBytes, size_t resultSize, 
                             const u8** ppPacketBuffer, const u8* dataBuffer, 
                             u32 dataBufferSize)
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
	return kmath::safeTruncateU32(resultSize);
}
u32 kutil::netUnpack(u64* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(u32* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(u16* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(u8* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(i64* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(i32* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(i16* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(i8* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(f32* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}
u32 kutil::netUnpack(f64* o_data, const u8** ppPacketBuffer, 
                     const u8* dataBuffer, u32 dataBufferSize)
{
	return netUnpackCommon(reinterpret_cast<u8*>(o_data), sizeof(*o_data), 
		                   ppPacketBuffer, dataBuffer, dataBufferSize);
}