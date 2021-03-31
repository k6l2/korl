#include "kgtAssetGlb.h"
#define KGT_ASSETGLB_UNPACK(pValue, pDataCursor, dataEnd) \
	kutil::dataUnpack(pValue, pDataCursor, dataEnd, true)
internal void kgt_assetGlb_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	const u8* dataCursor = data;
	const u8*const dataEnd = data + dataBytes;
	/* parse the 12-byte GLB header */
	{
		u32 magicNumber;
		u32 version;
		u32 glbBytes;
		u32 bytesUnpacked = 
			KGT_ASSETGLB_UNPACK(&magicNumber, &dataCursor, dataEnd);
		bytesUnpacked = KGT_ASSETGLB_UNPACK(&version, &dataCursor, dataEnd);
		bytesUnpacked = KGT_ASSETGLB_UNPACK(&glbBytes, &dataCursor, dataEnd);
		korlAssert(magicNumber == 0x46546C67);
		korlAssert(version == 2);
	}
	/* parse chunk 0 (JSON) */
	///@todo
	/* parse chunk 1 (binary) */
	///@todo
}
internal void kgt_assetGlb_onDependenciesLoaded(
	KgtAsset* a, KgtAssetManager* kam)
{
}
internal void kgt_assetGlb_free(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData)
{
}