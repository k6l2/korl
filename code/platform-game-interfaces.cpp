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
	return rawImg.pixelData[pixelIndex*pixelByteStride + componentByteOffset];
}
internal void 
	korlRawImageSetPixel(RawImage* rawImg, u64 pixelIndex, u8 r, u8 g, u8 b)
{
	switch(rawImg->pixelDataFormat)
	{
		case KorlPixelDataFormat::RGBA:
			rawImg->pixelData[4*pixelIndex + 0] = r;
			rawImg->pixelData[4*pixelIndex + 1] = g;
			rawImg->pixelData[4*pixelIndex + 2] = b;
			rawImg->pixelData[4*pixelIndex + 3] = 0xFF;
		break;
		case KorlPixelDataFormat::BGR:
			rawImg->pixelData[3*pixelIndex + 0] = b;
			rawImg->pixelData[3*pixelIndex + 1] = g;
			rawImg->pixelData[3*pixelIndex + 2] = r;
		break;
	}
}