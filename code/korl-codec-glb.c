#include "korl-codec-glb.h"
#include "korl-jsmn.h"
#include "korl-memoryPool.h"
#include "korl-string.h"
typedef struct _Korl_Codec_Glb_Chunk
{
    u32       bytes;
    u32       type;
    const u8* data;
} _Korl_Codec_Glb_Chunk;
korl_internal Korl_Codec_Gltf* _korl_codec_glb_decodeChunkJson(_Korl_Codec_Glb_Chunk*const chunk, Korl_Memory_AllocatorHandle resultAllocator)
{
    Korl_Codec_Gltf* result = NULL;
    /* parse the entire JSON chunk */
    KORL_ZERO_STACK(jsmn_parser, jasmine);
    jsmn_init(&jasmine);
    int resultJsmnParse = jsmn_parse(&jasmine, KORL_C_CAST(const char*, chunk->data), chunk->bytes, NULL, 0);
    if(resultJsmnParse <= 0)
    {
        korl_log(ERROR, "jsmn_parse failed: %i", resultJsmnParse);
        return NULL;
    }
    const u32  jsonTokensSize = korl_checkCast_i$_to_u32(resultJsmnParse);
    jsmntok_t* jsonTokens     = korl_dirtyAllocate(resultAllocator, jsonTokensSize * sizeof(*jsonTokens));
    jsmn_init(&jasmine);
    resultJsmnParse = jsmn_parse(&jasmine, KORL_C_CAST(const char*, chunk->data), chunk->bytes, jsonTokens, jsonTokensSize);
    korl_assert(korl_checkCast_i$_to_u32(resultJsmnParse) == jsonTokensSize);
    /* process each JSON token to decode the GLTF data */
    result = korl_allocate(resultAllocator, sizeof(*result));
    result->allocator = resultAllocator;
    typedef enum Gltf_Object_Type
        {GLTF_OBJECT_UNKNOWN
        ,GLTF_OBJECT_ASSET
        ,GLTF_OBJECT_ASSET_VERSION
        ,GLTF_OBJECT_SCENE
    } Gltf_Object_Type;
    typedef struct Gltf_Object
    {
        Gltf_Object_Type type;
        const jsmntok_t* token;
        int              parsedChildren;
    } Gltf_Object;
    KORL_MEMORY_POOL_DECLARE(Gltf_Object, objectStack, 16);
    KORL_MEMORY_POOL_EMPTY(objectStack);
    korl_assert(jsonTokens[0].type == JSMN_OBJECT);
    *KORL_MEMORY_POOL_ADD(objectStack) = (Gltf_Object){.token = jsonTokens};
    const jsmntok_t*const jsonTokensEnd = jsonTokens + jsonTokensSize;
    for(const jsmntok_t* jsonToken = jsonTokens + 1; jsonToken < jsonTokensEnd; jsonToken++)
    {
        const acu8 tokenRawUtf8 = {.data = chunk->data + jsonToken->start, .size = jsonToken->end - jsonToken->start};
        Gltf_Object_Type objectTypeParent = GLTF_OBJECT_UNKNOWN;
        for(i32 i = KORL_MEMORY_POOL_SIZE(objectStack) - 1; i >= 0; i--)
            if(objectStack[i].type != objectTypeParent)
            {
                objectTypeParent = objectStack[i].type;
                break;
            }
        if(jsonToken->size)// this token has children, and so it _may_ be a GLTF object
        {
            /* this JSON token has children to process; determine what type of 
                GLTF object it is based on our current object stack */
            Gltf_Object_Type objectType = GLTF_OBJECT_UNKNOWN;
            if(jsonToken->type == JSMN_STRING)
            {
                if(KORL_MEMORY_POOL_SIZE(objectStack) == 1)
                {
                    if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("asset")))
                        objectType = GLTF_OBJECT_ASSET;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("scene")))
                        objectType = GLTF_OBJECT_SCENE;
                }
                else if(objectTypeParent == GLTF_OBJECT_ASSET)
                {
                    if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("version")))
                        objectType = GLTF_OBJECT_ASSET_VERSION;
                }
            }
            /* now we can push it onto the stack */
            *KORL_MEMORY_POOL_ADD(objectStack) = (Gltf_Object){.type = objectType, .token = jsonToken};
        }
        else// otherwise this must be a property value token
        {
            switch(objectTypeParent)
            {
            case GLTF_OBJECT_ASSET_VERSION:{
                korl_jsmn_getString(chunk->data, jsonToken, result->asset.versionRawUtf8, korl_arraySize(result->asset.versionRawUtf8));
                break;}
            case GLTF_OBJECT_SCENE:{
                result->scene = KORL_C_CAST(i32, korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            default:{break;}
            }
            /* recursively update our parent objects, popping them off the stack 
                when their parsedChildren count exceeds their token->size */
            Gltf_Object* currentObject = KORL_MEMORY_POOL_ISEMPTY(objectStack) ? NULL : &objectStack[KORL_MEMORY_POOL_SIZE(objectStack) - 1];
            while(currentObject)
            {
                currentObject->parsedChildren++;
                if(currentObject->parsedChildren >= currentObject->token->size)
                {
                    KORL_MEMORY_POOL_POP(objectStack);
                    currentObject = KORL_MEMORY_POOL_ISEMPTY(objectStack) ? NULL : &objectStack[KORL_MEMORY_POOL_SIZE(objectStack) - 1];
                }
                else
                    currentObject = NULL;
            }
        }
        #if 0// DEBUG; print the JSON tokens
        korl_shared_const char* TYPE_STRINGS[] = 
            {"UNDEFINED"
            ,"OBJECT"
            ,"ARRAY"
            ,"STRING"
            ,"PRIMITIVE_NULL"
            ,"PRIMITIVE_BOOL"
            ,"PRIMITIVE_NUMBER"
        };
        const char* typeString = TYPE_STRINGS[0];
        switch(jsonToken->type)
        {
        case JSMN_OBJECT:{typeString = TYPE_STRINGS[1]; break;}
        case JSMN_ARRAY: {typeString = TYPE_STRINGS[2]; break;}
        case JSMN_STRING:{typeString = TYPE_STRINGS[3]; break;}
        case JSMN_PRIMITIVE:{
            if(chunk->data[jsonToken->start] == 'n')
                typeString = TYPE_STRINGS[4];
            else if(chunk->data[jsonToken->start] == 't' || chunk->data[jsonToken->start] == 'f')
                typeString = TYPE_STRINGS[5];
            else
                typeString = TYPE_STRINGS[6];
            break;}
        default: break;// everything else is UNDEFINED
        }
        const u$ jsonTokenSize = jsonToken->end - jsonToken->start;
        korl_log(VERBOSE, "jsonToken: %hs \"%.*hs\" [%i]", typeString, jsonTokenSize, chunk->data + jsonToken->start, jsonToken->size);
        #endif
        //@TODO
    }
    cleanUp_returnResult:
        korl_free(resultAllocator, jsonTokens);
        return result;
}
korl_internal Korl_Codec_Gltf* korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator)
{
    Korl_Codec_Gltf* result = NULL;
    korl_shared_const u32 GLB_VERSION = 2;
    if(!korl_memory_isLittleEndian())
        korl_log(ERROR, "big-endian systems not supported");
    const u8* glbDataU8 = KORL_C_CAST(u8*, glbData);
    /* decode GLB header */
    struct
    {
        u32 magic;
        u32 version;
        u32 length;
    } glbHeader;
    korl_assert(glbDataBytes >= sizeof(glbHeader));
    glbHeader.magic   = *KORL_C_CAST(u32*, glbDataU8); glbDataU8 += sizeof(glbHeader.magic);
    glbHeader.version = *KORL_C_CAST(u32*, glbDataU8); glbDataU8 += sizeof(glbHeader.version);
    glbHeader.length  = *KORL_C_CAST(u32*, glbDataU8); glbDataU8 += sizeof(glbHeader.length);
    korl_assert(glbHeader.magic == 0x46546C67/*ascii string "glTF"*/);
    korl_assert(glbHeader.version <= GLB_VERSION);
    korl_assert(glbHeader.length <= glbDataBytes);// the GLB data must be fully-contained in the provided `glbData`
    const u8*const glbDataEnd = KORL_C_CAST(u8*, glbData) + glbHeader.length;
    /* decode chunk 0 (JSON) */
    _Korl_Codec_Glb_Chunk glbChunk;
    glbChunk.bytes = *KORL_C_CAST(u32*, glbDataU8); glbDataU8 += sizeof(glbChunk.bytes);
    glbChunk.type  = *KORL_C_CAST(u32*, glbDataU8); glbDataU8 += sizeof(glbChunk.bytes);
    glbChunk.data  = glbDataU8;                     glbDataU8 += glbChunk.bytes;
    korl_assert(glbChunk.type == 0x4E4F534A/*ascii string "JSON"*/);
    result = _korl_codec_glb_decodeChunkJson(&glbChunk, resultAllocator);
    /* decode chunk 1 (binary buffer) */
    glbChunk.bytes = *KORL_C_CAST(u32*, glbDataU8); glbDataU8 += sizeof(glbChunk.bytes);
    glbChunk.type  = *KORL_C_CAST(u32*, glbDataU8); glbDataU8 += sizeof(glbChunk.bytes);
    glbChunk.data  = glbDataU8;                     glbDataU8 += glbChunk.bytes;
    korl_assert(glbChunk.type == 0x004E4942/*ascii string "\0BIN"*/);
    //@TODO: copy the buffer data into resultAllocator
    return result;
}
