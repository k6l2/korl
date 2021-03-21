#include "kgtAsset.h"
#include "gen_ptu_KgtAsset_dispatch.cpp"
internal KgtAssetHandle kgt_asset_makeHandle(KgtAssetIndex kai)
{
	if(kai >= KgtAssetIndex::ENUM_SIZE)
		return 0;
	korlAssert(
		std::numeric_limits<KgtAssetHandle>::max() > 
		static_cast<KgtAssetHandle>(kai));
	return static_cast<KgtAssetHandle>(kai) + 1;
}
internal KgtAssetIndex kgt_asset_findKgtAssetIndex(const char* assetName)
{
	for(size_t id = 0; id < KGT_ASSET_COUNT; id++)
		if(strcmp(kgtAssetFileNames[id], assetName) == 0)
			return static_cast<KgtAssetIndex>(id);
	return KgtAssetIndex::ENUM_SIZE;
}