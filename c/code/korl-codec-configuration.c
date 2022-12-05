#include "korl-codec-configuration.h"
korl_internal void korl_codec_configuration_create(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocator)
{
}
korl_internal void korl_codec_configuration_destroy(Korl_Codec_Configuration* context)
{
}
korl_internal void korl_codec_configuration_setU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, u32 value)
{
}
korl_internal void korl_codec_configuration_setI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, i32 value)
{
}
korl_internal acu8 korl_codec_configuration_toUtf8(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocatorResult)
{
    korl_assert(!"@TODO"); return (acu8){0};
}