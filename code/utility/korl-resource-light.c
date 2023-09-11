#include "korl-resource-light.h"
#include "korl-interface-platform.h"
#include "utility/korl-jsmn.h"
#include "utility/korl-checkCast.h"
#include "utility/korl-utility-string.h"
typedef struct _Korl_Resource_Light
{
    Korl_Gfx_Light gfxLight;
    bool           isTranscoded;
} _Korl_Resource_Light;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_collectDefragmentPointers(_korl_resource_light_collectDefragmentPointers)
{
    // no dynamic memory
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_light_descriptorStructCreate)
{
    return korl_allocate(allocatorRuntime, sizeof(_Korl_Resource_Light));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_light_descriptorStructDestroy)
{
    _Korl_Resource_Light*const light = KORL_C_CAST(_Korl_Resource_Light*, resourceDescriptorStruct);
    korl_free(allocatorRuntime, light);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_light_unload)
{
    _Korl_Resource_Light*const light = KORL_C_CAST(_Korl_Resource_Light*, resourceDescriptorStruct);
    light->isTranscoded = false;
}
typedef enum _Korl_Resource_Light_Json
    {_KORL_RESOURCE_LIGHT_JSON_UNKNOWN
    ,_KORL_RESOURCE_LIGHT_JSON
    ,_KORL_RESOURCE_LIGHT_JSON_TYPE
    ,_KORL_RESOURCE_LIGHT_JSON_DIRECTION
    ,_KORL_RESOURCE_LIGHT_JSON_COLOR
    ,_KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT
    ,_KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_AMBIENT
    ,_KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_DIFFUSE
    ,_KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_SPECULAR
} _Korl_Resource_Light_Json;
typedef struct _Korl_Resource_Light_Transcode_Json
{
    _Korl_Resource_Light_Json                  type;
    const jsmntok_t*                           jsmnToken;
    // const _Korl_Resource_Light_Transcode_Json* parent;// linked list of parents, so we can traverse the stack at any point during JSON transcoding
} _Korl_Resource_Light_Transcode_Json;
/** \return the # of \c jsmntok_t processed from \c jsmnTokens */
korl_internal u32 _korl_resource_light_transcode_jsonRecursive(_Korl_Resource_Light*const light, const u8* rawJson, const jsmntok_t* jsmnTokens, const _Korl_Resource_Light_Transcode_Json*const jsonParent)
{
    const acu8 jsonTokenString = {.size = korl_checkCast_i$_to_u$(jsmnTokens->end - jsmnTokens->start)
                                 ,.data = rawJson + jsmnTokens->start};
    KORL_ZERO_STACK(_Korl_Resource_Light_Transcode_Json, json);
    json.jsmnToken = jsmnTokens;
    // json.parent    = jsonParent;
    if(!jsonParent)
    {
        korl_assert(json.jsmnToken->type == JSMN_OBJECT);
        json.type = _KORL_RESOURCE_LIGHT_JSON;
    }
    else
    {
        switch(jsonParent->type)
        {
        case _KORL_RESOURCE_LIGHT_JSON_UNKNOWN: break;
        case _KORL_RESOURCE_LIGHT_JSON:
            korl_assert(json.jsmnToken->type == JSMN_STRING);
            if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("type")))
                json.type = _KORL_RESOURCE_LIGHT_JSON_TYPE;
            else if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("direction")))
                json.type = _KORL_RESOURCE_LIGHT_JSON_DIRECTION;
            else if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("color")))
               json.type = _KORL_RESOURCE_LIGHT_JSON_COLOR;
            break;
        case _KORL_RESOURCE_LIGHT_JSON_TYPE:
            korl_assert(json.jsmnToken->type == JSMN_STRING);
            if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("point")))
                light->gfxLight.type = KORL_GFX_LIGHT_TYPE_POINT;
            else if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("directional")))
                light->gfxLight.type = KORL_GFX_LIGHT_TYPE_DIRECTIONAL;
            else if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("spot")))
                light->gfxLight.type = KORL_GFX_LIGHT_TYPE_SPOT;
            break;
        case _KORL_RESOURCE_LIGHT_JSON_DIRECTION: light->gfxLight.direction = korl_jsmn_getV3f32(rawJson, json.jsmnToken); break;
        case _KORL_RESOURCE_LIGHT_JSON_COLOR:
            korl_assert(json.jsmnToken->type == JSMN_OBJECT);
            json.type = _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT;
            break;
        case _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT:
            korl_assert(json.jsmnToken->type == JSMN_STRING);
            if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("ambient")))
                json.type = _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_AMBIENT;
            else if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("diffuse")))
                json.type = _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_DIFFUSE;
            else if(korl_string_equalsAcu8(jsonTokenString, KORL_RAW_CONST_UTF8("specular")))
                json.type = _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_SPECULAR;
            break;
        case _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_AMBIENT : light->gfxLight.color.ambient  = korl_jsmn_getV3f32(rawJson, json.jsmnToken); break;
        case _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_DIFFUSE : light->gfxLight.color.diffuse  = korl_jsmn_getV3f32(rawJson, json.jsmnToken); break;
        case _KORL_RESOURCE_LIGHT_JSON_COLOR_OBJECT_SPECULAR: light->gfxLight.color.specular = korl_jsmn_getV3f32(rawJson, json.jsmnToken); break;
        }
    }
    for(int i = 0; i < json.jsmnToken->size; i++)
        jsmnTokens += _korl_resource_light_transcode_jsonRecursive(light, rawJson, jsmnTokens + 1, &json);
    return 1 + korl_checkCast_i$_to_u32(jsmnTokens - json.jsmnToken);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_light_transcode)
{
    _Korl_Resource_Light*const light = KORL_C_CAST(_Korl_Resource_Light*, resourceDescriptorStruct);
    /* parse JSON */
    jsmn_parser jsmnParser;
    jsmn_init(&jsmnParser);
    const int jsmnTokensSize = jsmn_parse(&jsmnParser, KORL_C_CAST(const char*, data), dataBytes, NULL, 0);
    korl_assert(jsmnTokensSize > 0);
    jsmntok_t* jsmnTokens = KORL_C_CAST(jsmntok_t*, korl_allocateDirty(allocatorTransient, jsmnTokensSize * sizeof(*jsmnTokens)));
    jsmn_init(&jsmnParser);
    korl_assert(jsmnTokensSize == jsmn_parse(&jsmnParser, KORL_C_CAST(const char*, data), dataBytes, jsmnTokens, KORL_C_CAST(unsigned, jsmnTokensSize)));
    /* decode JSON into Light data */
    korl_assert(jsmnTokensSize == korl_checkCast_u$_to_i32(_korl_resource_light_transcode_jsonRecursive(light, KORL_C_CAST(const u8*, data), jsmnTokens, NULL)));
    light->isTranscoded = true;
    /* cleanup */
    korl_free(allocatorTransient, jsmnTokens);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_light_clearTransientData)
{
    _Korl_Resource_Light*const light = KORL_C_CAST(_Korl_Resource_Light*, resourceDescriptorStruct);
    korl_memory_zero(light, sizeof(*light));
}
korl_internal void korl_resource_light_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_LIGHT);
    descriptorManifest.callbacks.collectDefragmentPointers = korl_functionDynamo_register(_korl_resource_light_collectDefragmentPointers);
    descriptorManifest.callbacks.descriptorStructCreate    = korl_functionDynamo_register(_korl_resource_light_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy   = korl_functionDynamo_register(_korl_resource_light_descriptorStructDestroy);
    descriptorManifest.callbacks.unload                    = korl_functionDynamo_register(_korl_resource_light_unload);
    descriptorManifest.callbacks.transcode                 = korl_functionDynamo_register(_korl_resource_light_transcode);
    descriptorManifest.callbacks.clearTransientData        = korl_functionDynamo_register(_korl_resource_light_clearTransientData);
    korl_resource_descriptor_register(&descriptorManifest);
}
korl_internal void korl_resource_light_use(Korl_Resource_Handle resourceHandleLight)
{
    _Korl_Resource_Light*const light = KORL_C_CAST(_Korl_Resource_Light*, korl_resource_getDescriptorStruct(resourceHandleLight));
    if(!light->isTranscoded)
        return;
    korl_gfx_light_use(&light->gfxLight, 1);
}
