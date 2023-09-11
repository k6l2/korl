#pragma once
#include "korl-interface-platform-resource.h"
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_LIGHT = "korl-rd-light";
korl_internal void korl_resource_light_register(void);
korl_internal void korl_resource_light_use(Korl_Resource_Handle resourceHandleLight, Korl_Math_V3f32 position);
