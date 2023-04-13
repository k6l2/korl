#include "korl-codec-glb.h"
#include "korl-jsmn.h"
#include "korl-memoryPool.h"
#include "korl-string.h"
#include "korl-interface-platform.h"
#include "korl-stb-ds.h"
#include "korl-checkCast.h"
typedef struct _Korl_Codec_Glb_Chunk
{
    u32       bytes;
    u32       type;
    const u8* data;
} _Korl_Codec_Glb_Chunk;
typedef enum Korl_Gltf_Object_Type
    {KORL_GLTF_OBJECT_UNKNOWN
    ,KORL_GLTF_OBJECT_ASSET
    ,KORL_GLTF_OBJECT_ASSET_OBJECT
    ,KORL_GLTF_OBJECT_ASSET_OBJECT_VERSION
    ,KORL_GLTF_OBJECT_SCENE
    ,KORL_GLTF_OBJECT_SCENES
    ,KORL_GLTF_OBJECT_SCENES_ARRAY
    ,KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT
    ,KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NAME
    ,KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NODES
    ,KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NODES_ARRAY
    ,KORL_GLTF_OBJECT_NODES
    ,KORL_GLTF_OBJECT_NODES_ARRAY
    ,KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT
    ,KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT_MESH
    ,KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT_NAME
    ,KORL_GLTF_OBJECT_MESHES
    ,KORL_GLTF_OBJECT_MESHES_ARRAY
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_NAME
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_POSITION
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_NORMAL
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_TEXCOORD_0
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_INDICES
    ,KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_MATERIAL
    ,KORL_GLTF_OBJECT_ACCESSORS
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_BUFFER_VIEW
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_COMPONENT_TYPE
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_COUNT
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_TYPE
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MAX
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MAX_ARRAY
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MIN
    ,KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MIN_ARRAY
    ,KORL_GLTF_OBJECT_BUFFER_VIEWS
    ,KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY
    ,KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT
    ,KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BUFFER
    ,KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BYTE_LENGTH
    ,KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BYTE_OFFSET
    ,KORL_GLTF_OBJECT_BUFFERS
    ,KORL_GLTF_OBJECT_BUFFERS_ARRAY
    ,KORL_GLTF_OBJECT_BUFFERS_ARRAY_ELEMENT
    ,KORL_GLTF_OBJECT_BUFFERS_ARRAY_ELEMENT_BYTE_LENGTH
} Korl_Gltf_Object_Type;
typedef struct Korl_Gltf_Object
{
    Korl_Gltf_Object_Type type;
    const jsmntok_t*      token;
    u32                   parsedChildren;
    void*                 array;// only valid if `type == JSMN_ARRAY`; the size of the array is determined by `token->size`; the current index is determined by `parsedChildren`
} Korl_Gltf_Object;
#define _korl_codec_glb_decodeChunkJson_processPass_getString(rawUtf8) \
    if(context) \
        (rawUtf8).byteOffset = korl_checkCast_i$_to_u32(contextByteNext - KORL_C_CAST(u8*, context));\
    contextByteNext += korl_jsmn_getString(chunk->data, jsonToken, context ? contextByteNext : NULL);\
    if(context) \
        (rawUtf8).size = korl_checkCast_i$_to_u32(contextByteNext - KORL_C_CAST(u8*, context)) - (rawUtf8).byteOffset - 1/*null-terminator*/;
korl_internal void* _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(Korl_Gltf_Object* objectStack, u$ objectStackSize, Korl_Gltf_Object_Type objectType, u32 arrayStride)
{
    for(i$ i = objectStackSize - 1; i >= 0; i--)
        if(objectStack[i].type == objectType)
            return objectStack[i].array 
                   ? KORL_C_CAST(u8*, objectStack[i].array) + objectStack[i].parsedChildren * arrayStride 
                   : NULL;
    return NULL;
}
korl_internal void* _korl_codec_glb_decodeChunkJson_processPass_newArray(Korl_Codec_Gltf*const context, const jsmntok_t* jsonToken, u8** contextByteNext, Korl_Codec_Gltf_Data* data, u32 arrayStride)
{
    if(context)
    {
        data->byteOffset = korl_checkCast_i$_to_u32(*contextByteNext - KORL_C_CAST(u8*, context));
        data->size       = korl_checkCast_i$_to_u32(jsonToken->size);
    }
    *contextByteNext += jsonToken->size * arrayStride;
    return context ? KORL_C_CAST(u8*, context) + data->byteOffset : NULL;
}
korl_internal u32 _korl_codec_glb_decodeChunkJson_processPass(_Korl_Codec_Glb_Chunk*const chunk, const jsmntok_t*const jsonTokens, u32 jsonTokensSize, Korl_Codec_Gltf*const context)
{
    u8* contextByteNext = KORL_C_CAST(u8*, context + 1);
    KORL_MEMORY_POOL_DECLARE(Korl_Gltf_Object, objectStack, 16);
    KORL_MEMORY_POOL_EMPTY(objectStack);
    korl_assert(jsonTokens[0].type == JSMN_OBJECT);
    *KORL_MEMORY_POOL_ADD(objectStack) = (Korl_Gltf_Object){.token = jsonTokens};
    const jsmntok_t*const jsonTokensEnd = jsonTokens + jsonTokensSize;
    for(const jsmntok_t* jsonToken = jsonTokens + 1; jsonToken < jsonTokensEnd; jsonToken++)
    {
        const acu8 tokenRawUtf8 = {.data = chunk->data + jsonToken->start, .size = jsonToken->end - jsonToken->start};
        korl_assert(!KORL_MEMORY_POOL_ISEMPTY(objectStack));
        Korl_Gltf_Object*const object = KORL_MEMORY_POOL_LAST_POINTER(objectStack);
        if(jsonToken->size)// this token has children, and so it _may_ be a GLTF object
        {
            /* this JSON token has children to process; determine what type of 
                GLTF object it is based on our current object stack */
            Korl_Gltf_Object_Type objectType = KORL_GLTF_OBJECT_UNKNOWN;
            void*                 array      = NULL;
            if(KORL_MEMORY_POOL_SIZE(objectStack) == 1)
            {
                korl_assert(jsonToken->type == JSMN_STRING);// top-level tokens of the root JSON object _must_ be key strings
                if(     0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("asset")))
                    objectType = KORL_GLTF_OBJECT_ASSET;
                else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("scene")))
                    objectType = KORL_GLTF_OBJECT_SCENE;
                else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("scenes")))
                    objectType = KORL_GLTF_OBJECT_SCENES;
                else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("nodes")))
                    objectType = KORL_GLTF_OBJECT_NODES;
                else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("meshes")))
                    objectType = KORL_GLTF_OBJECT_MESHES;
                else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("accessors")))
                    objectType = KORL_GLTF_OBJECT_ACCESSORS;
                else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("bufferViews")))
                    objectType = KORL_GLTF_OBJECT_BUFFER_VIEWS;
                else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("buffers")))
                    objectType = KORL_GLTF_OBJECT_BUFFERS;
            }
            else
                switch(object->type)
                {
                case KORL_GLTF_OBJECT_ASSET:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_ASSET_OBJECT;
                    break;}
                case KORL_GLTF_OBJECT_ASSET_OBJECT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("version")))
                        objectType = KORL_GLTF_OBJECT_ASSET_OBJECT_VERSION;
                    break;}
                case KORL_GLTF_OBJECT_SCENES:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_SCENES_ARRAY;
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &context->scenes, sizeof(Korl_Codec_Gltf_Scene));
                    break;}
                case KORL_GLTF_OBJECT_SCENES_ARRAY:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT;
                    break;}
                case KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(     0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("name")))
                        objectType = KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NAME;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("nodes")))
                        objectType = KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NODES;
                    break;}
                case KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NODES:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NODES_ARRAY;
                    Korl_Codec_Gltf_Scene*const currentScene = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_SCENES_ARRAY, sizeof(*currentScene));
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &currentScene->nodes, sizeof(u32));
                    break;}
                case KORL_GLTF_OBJECT_NODES:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_NODES_ARRAY;
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &context->nodes, sizeof(Korl_Codec_Gltf_Node));
                    break;}
                case KORL_GLTF_OBJECT_NODES_ARRAY:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT;
                    break;}
                case KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("name")))
                        objectType = KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT_NAME;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("mesh")))
                        objectType = KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT_MESH;
                    break;}
                case KORL_GLTF_OBJECT_MESHES:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_MESHES_ARRAY;
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &context->meshes, sizeof(Korl_Codec_Gltf_Mesh));
                    break;}
                case KORL_GLTF_OBJECT_MESHES_ARRAY:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT;
                    break;}
                case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("name")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_NAME;
                    if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("primitives")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES;
                    break;}
                case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY;
                    Korl_Codec_Gltf_Mesh*const currentMesh = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_MESHES_ARRAY, sizeof(*currentMesh));
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &currentMesh->primitives, sizeof(Korl_Codec_Gltf_Mesh_Primitive));
                    Korl_Codec_Gltf_Mesh_Primitive*const meshPrimitiveArray = array;
                    for(i32 i = 0; meshPrimitiveArray && i < jsonToken->size; i++)
                        /* default all mesh primitive data to invalid indices */
                        meshPrimitiveArray[i] = (Korl_Codec_Gltf_Mesh_Primitive){.attributes = {.position = -1, .normal = -1, .texCoord0 = -1}, .indices = -1, .material = -1};
                    break;}
                case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT;
                    break;}
                case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(     0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("attributes")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("indices")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_INDICES;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("material")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_MATERIAL;
                    break;}
                case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT;
                    break;}
                case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(     0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("POSITION")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_POSITION;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("NORMAL")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_NORMAL;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("TEXCOORD_0")))
                        objectType = KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_TEXCOORD_0;
                    break;}
                case KORL_GLTF_OBJECT_ACCESSORS:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY;
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &context->accessors, sizeof(Korl_Codec_Gltf_Accessor));
                    Korl_Codec_Gltf_Accessor*const accessorArray = array;
                    for(i32 i = 0; accessorArray && i < jsonToken->size; i++)
                        accessorArray[i] = (Korl_Codec_Gltf_Accessor){.aabb = KORL_MATH_AABB3F32_EMPTY};
                    break;}
                case KORL_GLTF_OBJECT_ACCESSORS_ARRAY:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT;
                    break;}
                case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(     0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("bufferView")))
                        objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_BUFFER_VIEW;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("componentType")))
                        objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_COMPONENT_TYPE;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("count")))
                        objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_COUNT;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("type")))
                        objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_TYPE;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("max")))
                        objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MAX;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("min")))
                        objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MIN;
                    break;}
                case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MAX:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MAX_ARRAY;
                    break;}
                case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MIN:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MIN_ARRAY;
                    break;}
                case KORL_GLTF_OBJECT_BUFFER_VIEWS:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY;
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &context->bufferViews, sizeof(Korl_Codec_Gltf_BufferView));
                    break;}
                case KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT;
                    break;}
                case KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(     0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("buffer")))
                        objectType = KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BUFFER;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("byteLength")))
                        objectType = KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BYTE_LENGTH;
                    else if(0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("byteOffset")))
                        objectType = KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BYTE_OFFSET;
                    break;}
                case KORL_GLTF_OBJECT_BUFFERS:{
                    korl_assert(jsonToken->type == JSMN_ARRAY);
                    objectType = KORL_GLTF_OBJECT_BUFFERS_ARRAY;
                    array = _korl_codec_glb_decodeChunkJson_processPass_newArray(context, jsonToken, &contextByteNext, &context->buffers, sizeof(Korl_Codec_Gltf_Buffer));
                    break;}
                case KORL_GLTF_OBJECT_BUFFERS_ARRAY:{
                    korl_assert(jsonToken->type == JSMN_OBJECT);
                    objectType = KORL_GLTF_OBJECT_BUFFERS_ARRAY_ELEMENT;
                    break;}
                case KORL_GLTF_OBJECT_BUFFERS_ARRAY_ELEMENT:{
                    korl_assert(jsonToken->type == JSMN_STRING);
                    if(     0 == korl_string_compareAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("byteLength")))
                        objectType = KORL_GLTF_OBJECT_BUFFERS_ARRAY_ELEMENT_BYTE_LENGTH;
                    break;}
                default: break;
                }
            /* now we can push it onto the stack */
            *KORL_MEMORY_POOL_ADD(objectStack) = (Korl_Gltf_Object){.type = objectType, .token = jsonToken, .array = array};
        }
        else// otherwise this must be a property value token
        {
            switch(object->type)
            {
            case KORL_GLTF_OBJECT_ASSET_OBJECT_VERSION:{
                _korl_codec_glb_decodeChunkJson_processPass_getString(context->asset.rawUtf8Version);
                break;}
            case KORL_GLTF_OBJECT_SCENE:{
                if(context) context->scene = KORL_C_CAST(i32, korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NAME:{
                Korl_Codec_Gltf_Scene*const currentScene = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_SCENES_ARRAY, sizeof(*currentScene));
                _korl_codec_glb_decodeChunkJson_processPass_getString(currentScene->rawUtf8Name);
                break;}
            case KORL_GLTF_OBJECT_SCENES_ARRAY_ELEMENT_NODES_ARRAY:{
                Korl_Codec_Gltf_Scene*const currentScene = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_SCENES_ARRAY, sizeof(*currentScene));
                /* set the current scene's current node's index value */
                if(currentScene)
                {
                    const u32 currentNodeIndex = KORL_MEMORY_POOL_LAST_POINTER(objectStack)->parsedChildren;
                    korl_assert(currentNodeIndex < currentScene->nodes.size);
                    u32*const nodes = KORL_C_CAST(u32*, KORL_C_CAST(u8*, context) + currentScene->nodes.byteOffset) + currentNodeIndex;
                    nodes[currentNodeIndex] = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                }
                break;}
            case KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT_NAME:{
                Korl_Codec_Gltf_Node*const currentNode = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_NODES_ARRAY, sizeof(*currentNode));
                _korl_codec_glb_decodeChunkJson_processPass_getString(currentNode->rawUtf8Name);
                break;}
            case KORL_GLTF_OBJECT_NODES_ARRAY_ELEMENT_MESH:{
                Korl_Codec_Gltf_Node*const currentNode = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_NODES_ARRAY, sizeof(*currentNode));
                if(currentNode) currentNode->mesh = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_NAME:{
                Korl_Codec_Gltf_Mesh*const currentMesh = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_MESHES_ARRAY, sizeof(*currentMesh));
                _korl_codec_glb_decodeChunkJson_processPass_getString(currentMesh->rawUtf8Name);
                break;}
            case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_POSITION:{
                Korl_Codec_Gltf_Mesh_Primitive*const currentMeshPrimitive = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY, sizeof(*currentMeshPrimitive));
                if(currentMeshPrimitive) currentMeshPrimitive->attributes.position = korl_checkCast_f32_to_i32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_NORMAL:{
                Korl_Codec_Gltf_Mesh_Primitive*const currentMeshPrimitive = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY, sizeof(*currentMeshPrimitive));
                if(currentMeshPrimitive) currentMeshPrimitive->attributes.normal = korl_checkCast_f32_to_i32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_ATTRIBUTES_OBJECT_TEXCOORD_0:{
                Korl_Codec_Gltf_Mesh_Primitive*const currentMeshPrimitive = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY, sizeof(*currentMeshPrimitive));
                if(currentMeshPrimitive) currentMeshPrimitive->attributes.texCoord0 = korl_checkCast_f32_to_i32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_INDICES:{
                Korl_Codec_Gltf_Mesh_Primitive*const currentMeshPrimitive = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY, sizeof(*currentMeshPrimitive));
                if(currentMeshPrimitive) currentMeshPrimitive->indices = korl_checkCast_f32_to_i32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY_ELEMENT_MATERIAL:{
                Korl_Codec_Gltf_Mesh_Primitive*const currentMeshPrimitive = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_MESHES_ARRAY_ELEMENT_PRIMITIVES_ARRAY, sizeof(*currentMeshPrimitive));
                if(currentMeshPrimitive) currentMeshPrimitive->material = korl_checkCast_f32_to_i32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_BUFFER_VIEW:{
                Korl_Codec_Gltf_Accessor*const currentAccessor = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_ACCESSORS_ARRAY, sizeof(*currentAccessor));
                if(currentAccessor) currentAccessor->bufferView = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_COMPONENT_TYPE:{
                Korl_Codec_Gltf_Accessor*const currentAccessor = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_ACCESSORS_ARRAY, sizeof(*currentAccessor));
                if(currentAccessor) currentAccessor->componentType = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_COUNT:{
                Korl_Codec_Gltf_Accessor*const currentAccessor = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_ACCESSORS_ARRAY, sizeof(*currentAccessor));
                if(currentAccessor) currentAccessor->count = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_TYPE:{
                Korl_Codec_Gltf_Accessor*const currentAccessor = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_ACCESSORS_ARRAY, sizeof(*currentAccessor));
                if(currentAccessor)
                {
                    const acu8 jsonAcu8 = {.data = chunk->data + jsonToken->start, .size = jsonToken->end - jsonToken->start};
                    if(     0 == korl_string_compareAcu8(KORL_RAW_CONST_UTF8("SCALAR"), jsonAcu8))
                        currentAccessor->type = KORL_CODEC_GLTF_ACCESSOR_TYPE_SCALAR;
                    else if(0 == korl_string_compareAcu8(KORL_RAW_CONST_UTF8("VEC2"), jsonAcu8))
                        currentAccessor->type = KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC2;
                    else if(0 == korl_string_compareAcu8(KORL_RAW_CONST_UTF8("VEC3"), jsonAcu8))
                        currentAccessor->type = KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3;
                    else if(0 == korl_string_compareAcu8(KORL_RAW_CONST_UTF8("VEC4"), jsonAcu8))
                        currentAccessor->type = KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC4;
                    else if(0 == korl_string_compareAcu8(KORL_RAW_CONST_UTF8("MAT2"), jsonAcu8))
                        currentAccessor->type = KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT2;
                    else if(0 == korl_string_compareAcu8(KORL_RAW_CONST_UTF8("MAT3"), jsonAcu8))
                        currentAccessor->type = KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT3;
                    else if(0 == korl_string_compareAcu8(KORL_RAW_CONST_UTF8("MAT4"), jsonAcu8))
                        currentAccessor->type = KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT4;
                    else
                        korl_log(ERROR, "invalid accessor type \"%.*hs\"", jsonAcu8.size, jsonAcu8.data);
                }
                break;}
            case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MAX_ARRAY:{
                Korl_Codec_Gltf_Accessor*const currentAccessor = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_ACCESSORS_ARRAY, sizeof(*currentAccessor));
                if(currentAccessor) currentAccessor->aabb.max.elements[KORL_MEMORY_POOL_LAST_POINTER(objectStack)->parsedChildren] = korl_jsmn_getF32(chunk->data, jsonToken);
                break;}
            case KORL_GLTF_OBJECT_ACCESSORS_ARRAY_ELEMENT_MIN_ARRAY:{
                Korl_Codec_Gltf_Accessor*const currentAccessor = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_ACCESSORS_ARRAY, sizeof(*currentAccessor));
                if(currentAccessor) currentAccessor->aabb.min.elements[KORL_MEMORY_POOL_LAST_POINTER(objectStack)->parsedChildren] = korl_jsmn_getF32(chunk->data, jsonToken);
                break;}
            case KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BUFFER:{
                Korl_Codec_Gltf_BufferView*const currentBufferView = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY, sizeof(*currentBufferView));
                if(currentBufferView) currentBufferView->buffer = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BYTE_LENGTH:{
                Korl_Codec_Gltf_BufferView*const currentBufferView = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY, sizeof(*currentBufferView));
                if(currentBufferView) currentBufferView->byteLength = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY_ELEMENT_BYTE_OFFSET:{
                Korl_Codec_Gltf_BufferView*const currentBufferView = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_BUFFER_VIEWS_ARRAY, sizeof(*currentBufferView));
                if(currentBufferView) currentBufferView->byteOffset = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            case KORL_GLTF_OBJECT_BUFFERS_ARRAY_ELEMENT_BYTE_LENGTH:{
                Korl_Codec_Gltf_Buffer*const currentBuffer = _korl_codec_glb_decodeChunkJson_processPass_currentArrayItem(objectStack, KORL_MEMORY_POOL_SIZE(objectStack), KORL_GLTF_OBJECT_BUFFERS_ARRAY, sizeof(*currentBuffer));
                if(currentBuffer) currentBuffer->byteLength = korl_checkCast_f32_to_u32(korl_jsmn_getF32(chunk->data, jsonToken));
                break;}
            default:{break;}
            }
            /* recursively update our parent objects, popping them off the stack 
                when their parsedChildren count exceeds their token->size */
            Korl_Gltf_Object* currentObject = KORL_MEMORY_POOL_ISEMPTY(objectStack) ? NULL : &objectStack[KORL_MEMORY_POOL_SIZE(objectStack) - 1];
            while(currentObject)
            {
                currentObject->parsedChildren++;
                if(currentObject->parsedChildren >= korl_checkCast_i$_to_u32(currentObject->token->size))
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
    }
    return korl_checkCast_i$_to_u32(contextByteNext - KORL_C_CAST(u8*, context));
}
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
    const u32 bytesRequired = _korl_codec_glb_decodeChunkJson_processPass(chunk, jsonTokens, jsonTokensSize, NULL);
    result = korl_allocate(resultAllocator, bytesRequired);
    _korl_codec_glb_decodeChunkJson_processPass(chunk, jsonTokens, jsonTokensSize, result);
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
