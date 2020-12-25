#pragma once
#include "kutil.h"
#include "kmath.h"
#include "gen_kgtAssets.h"
struct KgtSpriteFontMetaData
{
	KgtAssetIndex kaiTexture;
	KgtAssetIndex kaiTextureOutline;
	v2u32 monospaceSize;
	v2u32 texturePadding;
	char characters[0x7F];
	u8 charactersSize;
};
internal bool 
	kgtSpriteFontDecodeMeta(
		void* fileData, u32 fileBytes, const char* cStrAnsiAssetName, 
		KgtSpriteFontMetaData* o_sfMeta, 
		char* o_assetFileNameTexture, size_t assetFileNameTextureBufferSize, 
		char* o_assetFileNameTextureOutline, 
		size_t assetFileNameTextureOutlineBufferSize);
struct Color4f32;
struct KrbApi;
struct KgtAssetManager;
/**
 * @param anchor
 * 	Normalized.  Relative to the upper-left corner of the resulting text AABB.
 */
internal void 
	kgtSpriteFontDraw(
		KgtAssetIndex kaiSpriteFontMeta, const char* cStrText, 
		const v2f32& position, const v2f32& anchor, const v2f32& scale, 
		const Color4f32& color, const Color4f32& colorOutline, 
		const KrbApi*const krb, KgtAssetManager*const kam);
internal v2f32 
	kgtSpriteFontComputeAabbSize(
		KgtAssetIndex kaiSpriteFontMeta, const char* cStrText, 
		const v2f32& scale, KgtAssetManager*const kam);
internal v2f32 
	kgtSpriteFontComputeAabbTopLeft(
		KgtAssetIndex kaiSpriteFontMeta, const v2f32& position, 
		const v2f32& scale, KgtAssetManager*const kam);