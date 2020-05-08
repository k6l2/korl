// Kyle's Renderer Backend interface //
#pragma once
#include "global-defines.h"
#define KRB_BEGIN_FRAME(name) void name(f32 clamped0_1_red, \
                                        f32 clamped0_1_green, \
                                        f32 clamped0_1_blue)
typedef KRB_BEGIN_FRAME(fnSig_krbBeginFrame);
internal KRB_BEGIN_FRAME(krbBeginFrame);