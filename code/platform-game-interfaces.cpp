#include "platform-game-interfaces.h"
internal u8 
	korlRawImageGetRed(const RawImage& rawImg, u64 pixelIndex)
{
	u32 componentByteOffset = 0;
	u32 pixelByteStride = 4;
	switch(rawImg.pixelDataFormat)
	{
	case KorlPixelDataFormat::RGBA:
		componentByteOffset = 0;
		break;
	case KorlPixelDataFormat::BGR:
		pixelByteStride = 3;
		componentByteOffset = 2;
		break;
	}
	if(rawImg.rowByteStride)
	{
		const u32 row    = kmath::safeTruncateU32(pixelIndex / rawImg.sizeX);
		const u32 column = kmath::safeTruncateU32(pixelIndex % rawImg.sizeX);
		componentByteOffset += row*rawImg.rowByteStride;
		pixelIndex = column;
	}
	return rawImg.pixelData[pixelIndex*pixelByteStride + componentByteOffset];
}
internal u8 
	korlRawImageGetGreen(const RawImage& rawImg, u64 pixelIndex)
{
	u32 componentByteOffset = 0;
	u32 pixelByteStride = 4;
	switch(rawImg.pixelDataFormat)
	{
	case KorlPixelDataFormat::RGBA:
		componentByteOffset = 1;
		break;
	case KorlPixelDataFormat::BGR:
		pixelByteStride = 3;
		componentByteOffset = 1;
		break;
	}
	if(rawImg.rowByteStride)
	{
		const u32 row    = kmath::safeTruncateU32(pixelIndex / rawImg.sizeX);
		const u32 column = kmath::safeTruncateU32(pixelIndex % rawImg.sizeX);
		componentByteOffset += row*rawImg.rowByteStride;
		pixelIndex = column;
	}
	return rawImg.pixelData[pixelIndex*pixelByteStride + componentByteOffset];
}
internal u8 
	korlRawImageGetBlue(const RawImage& rawImg, u64 pixelIndex)
{
	u32 componentByteOffset = 0;
	u32 pixelByteStride = 4;
	switch(rawImg.pixelDataFormat)
	{
	case KorlPixelDataFormat::RGBA:
		componentByteOffset = 2;
		break;
	case KorlPixelDataFormat::BGR:
		pixelByteStride = 3;
		componentByteOffset = 0;
		break;
	}
	if(rawImg.rowByteStride)
	{
		const u32 row    = kmath::safeTruncateU32(pixelIndex / rawImg.sizeX);
		const u32 column = kmath::safeTruncateU32(pixelIndex % rawImg.sizeX);
		componentByteOffset += row*rawImg.rowByteStride;
		pixelIndex = column;
	}
	return rawImg.pixelData[pixelIndex*pixelByteStride + componentByteOffset];
}
internal u8 
	korlRawImageGetAlpha(const RawImage& rawImg, u64 pixelIndex)
{
	u32 componentByteOffset = 3;
	u32 pixelByteStride = 4;
	switch(rawImg.pixelDataFormat)
	{
	case KorlPixelDataFormat::RGBA:
		break;
	case KorlPixelDataFormat::BGR:
		return 0xFF;
		break;
	}
	if(rawImg.rowByteStride)
	{
		const u32 row    = kmath::safeTruncateU32(pixelIndex / rawImg.sizeX);
		const u32 column = kmath::safeTruncateU32(pixelIndex % rawImg.sizeX);
		componentByteOffset += row*rawImg.rowByteStride;
		pixelIndex = column;
	}
	return rawImg.pixelData[pixelIndex*pixelByteStride + componentByteOffset];
}
internal u8 
	korlRawImageGetRed(const RawImage& rawImg, const v2u32& coord)
{
	return korlRawImageGetRed(rawImg, coord.y * rawImg.sizeX + coord.x);
}
internal u8 
	korlRawImageGetGreen(const RawImage& rawImg, const v2u32& coord)
{
	return korlRawImageGetGreen(rawImg, coord.y * rawImg.sizeX + coord.x);
}
internal u8 
	korlRawImageGetBlue(const RawImage& rawImg, const v2u32& coord)
{
	return korlRawImageGetBlue(rawImg, coord.y * rawImg.sizeX + coord.x);
}
internal u8 
	korlRawImageGetAlpha(const RawImage& rawImg, const v2u32& coord)
{
	return korlRawImageGetAlpha(rawImg, coord.y * rawImg.sizeX + coord.x);
}
internal void 
	korlRawImageSetPixel(RawImage* rawImg, u64 pixelIndex, u8 r, u8 g, u8 b)
{
	u32 rowStrideByteOffset = 0;
	if(rawImg->rowByteStride)
	{
		const u32 row    = kmath::safeTruncateU32(pixelIndex / rawImg->sizeX);
		const u32 column = kmath::safeTruncateU32(pixelIndex % rawImg->sizeX);
		rowStrideByteOffset = row*rawImg->rowByteStride;
		pixelIndex = column;
	}
	switch(rawImg->pixelDataFormat)
	{
		case KorlPixelDataFormat::RGBA:
			rawImg->pixelData[rowStrideByteOffset + 4*pixelIndex + 0] = r;
			rawImg->pixelData[rowStrideByteOffset + 4*pixelIndex + 1] = g;
			rawImg->pixelData[rowStrideByteOffset + 4*pixelIndex + 2] = b;
			rawImg->pixelData[rowStrideByteOffset + 4*pixelIndex + 3] = 0xFF;
		break;
		case KorlPixelDataFormat::BGR:
			rawImg->pixelData[rowStrideByteOffset + 3*pixelIndex + 0] = b;
			rawImg->pixelData[rowStrideByteOffset + 3*pixelIndex + 1] = g;
			rawImg->pixelData[rowStrideByteOffset + 3*pixelIndex + 2] = r;
		break;
	}
}