#pragma once
#include "stb/stb_image.h"
#include "korl-globalDefines.h"
korl_internal void korl_stb_image_initialize(void);
//KORL-ISSUE-000-000-012: add a cleanup function to verify that there are not memory leaks
