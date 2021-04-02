#include "kgtAssetGlb.h"
#ifndef JSMN_STATIC
	#define JSMN_STATIC
#endif
#ifndef JSMN_HEADER
	#define JSMN_HEADER
	#include "jsmn/jsmn.h"
#endif
#define KGT_ASSETGLB_UNPACK(pValue) \
	bytesUnpacked = kutil::dataUnpack(pValue, &dataCursor, dataEnd, true)
internal void kgt_assetGlb_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	const u8* dataCursor = data;
	const u8*const dataEnd = data + dataBytes;
	u32 bytesUnpacked;
	/* parse the 12-byte GLB header */
	{
		u32 magicNumber;
		u32 version;
		/* total size of the entire file, including header & all chunks */
		u32 glbBytes;
		KGT_ASSETGLB_UNPACK(&magicNumber);
		KGT_ASSETGLB_UNPACK(&version);
		KGT_ASSETGLB_UNPACK(&glbBytes);
		/* the magic # is literally the ASCII string "glTF" */
		korlAssert(magicNumber == 0x46546C67);
		korlAssert(version == 2);
		korlAssert(glbBytes == dataBytes);
	}
	/* parse chunk 0 (JSON) */
	{
		u32 chunkLength;
		u32 chunkType;
		KGT_ASSETGLB_UNPACK(&chunkLength);
		KGT_ASSETGLB_UNPACK(&chunkType);
		/* this chunkType value == the ASCII string "JSON" */
		korlAssert(chunkType == 0x4E4F534A);
		/* the remaining data of this chunk should be a JSON string of byte 
			length 'chunkLength', which we must now parse & process */
		jsmn_parser jsmnParser;
		jsmn_init(&jsmnParser);
		jsmntok_t tokens[256];
		static_assert(sizeof(char) == sizeof(u8));
		const char* json = reinterpret_cast<const char*>(dataCursor);
		const int parsedTokens = 
			jsmn_parse(&jsmnParser, json, chunkLength, tokens, 
				CARRAY_SIZE(tokens));
		if(parsedTokens == JSMN_ERROR_INVAL)
			KLOG(ERROR, "Invalid JSON!");
		else if(parsedTokens == JSMN_ERROR_NOMEM)
			KLOG(ERROR, "Not enough JSON token memory!");
		else if(parsedTokens == JSMN_ERROR_PART)
			KLOG(ERROR, "Incomplete JSON!");
		else
		/* no errors, let's process our glTF JSON tokens ~ */
		{
#if INTERNAL_BUILD && SLOW_BUILD
			/* for debug analysis, let's log all the parsed JSON tokens */
			for(int t = 0; t < parsedTokens; t++)
			{
				const jsmntok_t& token = tokens[t];
				const int tokenStrLen = token.end - token.start;
				switch(token.type)
				{
				case JSMN_UNDEFINED: {
					KLOG(ERROR, "Undefined token! {%i - %i}[%i]", 
						token.start, token.end, token.size);
					} break;
				case JSMN_OBJECT: {
					KLOG(INFO, "JSON_OBJECT [%i children]", token.size);
					} break;
				case JSMN_ARRAY: {
					KLOG(INFO, "JSON_ARRAY [%i children]", token.size);
					} break;
				case JSMN_STRING: {
					KLOG(INFO, "JSON_STRING '%.*s' [%i children]", 
						tokenStrLen, json + token.start, token.size);
					} break;
				case JSMN_PRIMITIVE: {
					KLOG(INFO, "JSON_PRIMITIVE '%.*s' [%i children]", 
						tokenStrLen, json + token.start, token.size);
					} break;
				}
			}
#endif// INTERNAL_BUILD && SLOW_BUILD
			///@todo: populate the GLB asset with this data
		}/* end of JSON token processing */
	}
	/* parse chunk 1 (binary) if it exists */
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