// Kyle's Renderer Backend interface //
#pragma once
#include "global-defines.h"
using KrbTextureHandle = u32;
struct RawImage;
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
///TODO: use separate colors per vertex
#define KRB_DRAW_LINE(name) void name(const v2f32& p0, const v2f32& p1, \
                                      const Color4f32& color)
#define KRB_DRAW_TRI(name) void name(const v2f32& p0, const v2f32& p1, \
                                     const v2f32& p2)
#define KRB_DRAW_TRI_TEXTURED(name) void name(const v2f32& p0, const v2f32& p1,\
                                              const v2f32& p2, const v2f32& t0,\
                                              const v2f32& t1, const v2f32& t2)
#define KRB_VIEW_TRANSLATE(name) void name(const v2f32& offset)
#define KRB_SET_MODEL_XFORM(name) void name(const v2f32& translation)
#define KRB_LOAD_IMAGE(name) KrbTextureHandle name(const RawImage& rawImage)
#define KRB_USE_TEXTURE(name) void name(KrbTextureHandle kth)
typedef KRB_BEGIN_FRAME(fnSig_krbBeginFrame);
typedef KRB_SET_PROJECTION_ORTHO(fnSig_krbSetProjectionOrtho);
typedef KRB_DRAW_LINE(fnSig_krbDrawLine);
typedef KRB_DRAW_TRI(fnSig_krbDrawTri);
typedef KRB_DRAW_TRI_TEXTURED(fnSig_krbDrawTriTextured);
typedef KRB_VIEW_TRANSLATE(fnSig_krbViewTranslate);
typedef KRB_SET_MODEL_XFORM(fnSig_krbSetModelXform);
typedef KRB_LOAD_IMAGE(fnSig_krbLoadImage);
typedef KRB_USE_TEXTURE(fnSig_krbUseTexture);
internal KRB_BEGIN_FRAME(krbBeginFrame);
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho);
internal KRB_DRAW_LINE(krbDrawLine);
internal KRB_DRAW_TRI(krbDrawTri);
internal KRB_DRAW_TRI_TEXTURED(krbDrawTriTextured);
internal KRB_VIEW_TRANSLATE(krbViewTranslate);
internal KRB_SET_MODEL_XFORM(krbSetModelXform);
/** @param tempImageDataBuffer must point to an array of bytes which has a 
 *                             minimum size of:
 *                                 z85::decodedFileSizeBytes(z85ImageNumBytes)
 */
internal KRB_LOAD_IMAGE(krbLoadImage);
internal KRB_USE_TEXTURE(krbUseTexture);