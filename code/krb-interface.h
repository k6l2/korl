// Kyle's Renderer Backend interface //
#pragma once
#include "global-defines.h"
struct Color4f32
{
	f32 r;
	f32 g;
	f32 b;
	f32 a;
};
namespace krb
{
	global_variable const Color4f32 RED   = {1,0,0,1};
	global_variable const Color4f32 GREEN = {0,1,0,1};
}
#define KRB_BEGIN_FRAME(name) void name(f32 clamped0_1_red, \
                                        f32 clamped0_1_green, \
                                        f32 clamped0_1_blue)
#define KRB_SET_PROJECTION_ORTHO(name) void name(f32 windowSizeX, \
                                                 f32 windowSizeY, \
                                                 f32 halfDepth)
#define KRB_DRAW_LINE(name) void name(const v2f32& p0, const v2f32& p1, \
                                      const Color4f32& color)
#define KRB_DRAW_TRI(name) void name(const v2f32& p0, const v2f32& p1, \
                                     const v2f32& p2)
#define KRB_VIEW_TRANSLATE(name) void name(const v2f32& offset)
typedef KRB_BEGIN_FRAME(fnSig_krbBeginFrame);
typedef KRB_SET_PROJECTION_ORTHO(fnSig_krbSetProjectionOrtho);
typedef KRB_DRAW_LINE(fnSig_krbDrawLine);
typedef KRB_DRAW_TRI(fnSig_krbDrawTri);
typedef KRB_VIEW_TRANSLATE(fnSig_krbViewTranslate);
internal KRB_BEGIN_FRAME(krbBeginFrame);
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho);
internal KRB_DRAW_LINE(krbDrawLine);
internal KRB_DRAW_TRI(krbDrawTri);
internal KRB_VIEW_TRANSLATE(krbViewTranslate);