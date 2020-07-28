// Kyle's Renderer Backend interface //
#pragma once
#include "kutil.h"
#include "kmath.h"
using KrbTextureHandle = u32;
struct Color4f32
{
	f32 r, g, b, a;
};
internal Color4f32 lerp(const Color4f32& a, const Color4f32& b, f32 ratioLerp)
{
	Color4f32 result;
	result.r = a.r + (b.r - a.r)*ratioLerp;
	result.g = a.g + (b.g - a.g)*ratioLerp;
	result.b = a.b + (b.b - a.b)*ratioLerp;
	result.a = a.a + (b.a - a.a)*ratioLerp;
	return result;
}
namespace krb
{
	global_variable const KrbTextureHandle INVALID_TEXTURE_HANDLE = 0;
	global_variable const Color4f32 WHITE  = {1,1,1,1};
	global_variable const Color4f32 RED    = {1,0,0,1};
	global_variable const Color4f32 GREEN  = {0,1,0,1};
	global_variable const Color4f32 BLUE   = {0,0,1,1};
	global_variable const Color4f32 YELLOW = {1,1,0,1};
}
#define KRB_BEGIN_FRAME(name) void name(f32 clamped0_1_red, \
                                        f32 clamped0_1_green, \
                                        f32 clamped0_1_blue)
/** Setup a right-handed axis where +Y is UP. */
#define KRB_SET_PROJECTION_ORTHO(name) void name(u32 windowSizeX, \
                                                 u32 windowSizeY, \
                                                 f32 halfDepth)
/** Setup a right-handed axis where +Y is UP. */
#define KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(name) void name(u32 windowSizeX, \
                                                              u32 windowSizeY, \
                                                              u32 fixedHeight, \
                                                              f32 halfDepth)
///TODO: use separate colors per vertex
#define KRB_DRAW_LINE(name) void name(const v2f32& p0, const v2f32& p1, \
                                      const Color4f32& color)
#define KRB_DRAW_TRI(name) void name(const v2f32& p0, const v2f32& p1, \
                                     const v2f32& p2)
#define KRB_DRAW_TRI_TEXTURED(name) void name(const v2f32& p0, const v2f32& p1,\
                                              const v2f32& p2, const v2f32& t0,\
                                              const v2f32& t1, const v2f32& t2)
/** 
 * @param ratioAnchor is relative to the top-left (-X, Y) most point of the quad 
 *                    mesh.  Positive values point in the {X,-Y} direction in 
 *                    model-space.  Examples: {0.5f,0.5f} will set the pivot of 
 *                    the mesh to be in the center.  {0,0} will set the pivot of 
 *                    the mesh to be the top-left corner of the mesh, which 
 *                    should correspond to the top-left corner of the texture!
 * @param texCoords [up-left, down-left, down-right, up-right]
 * @param colors [up-left, down-left, down-right, up-right]
*/
#define KRB_DRAW_QUAD_TEXTURED(name) void name(const v2f32& size, \
                                               const v2f32& ratioAnchor, \
                                               const v2f32 texCoords[4], \
											   const Color4f32 colors[4])
#define KRB_DRAW_CIRCLE(name) void name(f32 radius, f32 outlineThickness, \
                                        const Color4f32& colorFill, \
                                        const Color4f32& colorOutline, \
										u16 vertexCount)
#define KRB_VIEW_TRANSLATE(name) void name(const v2f32& offset)
#define KRB_SET_MODEL_XFORM(name) void name(const v3f32& translation, \
                                         const kQuaternion& orientation, \
                                         const v3f32& scale)
#define KRB_SET_MODEL_XFORM_2D(name) void name(const v2f32& translation, \
                                         const kQuaternion& orientation, \
                                         const v2f32& scale)
#define KRB_LOAD_IMAGE(name) KrbTextureHandle name(u32 imageSizeX, \
                                                   u32 imageSizeY, \
                                                   u8* imageDataRGBA)
#define KRB_DELETE_TEXTURE(name) void name(KrbTextureHandle krbTextureHandle)
#define KRB_USE_TEXTURE(name) void name(KrbTextureHandle kth)
/** 
 * @return {NAN,NAN} if the provided world position is not contained within the 
 *         camera's clip space.  This does NOT mean that non-{NAN,NAN} values 
 *         are on the screen!
*/
#define KRB_WORLD_TO_SCREEN(name) v2f32 name(const f32* pWorldPosition, \
                                             u8 worldPositionDimension)
typedef KRB_BEGIN_FRAME(fnSig_krbBeginFrame);
typedef KRB_SET_PROJECTION_ORTHO(fnSig_krbSetProjectionOrtho);
typedef KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(
	fnSig_krbSetProjectionOrthoFixedHeight);
typedef KRB_DRAW_LINE(fnSig_krbDrawLine);
typedef KRB_DRAW_TRI(fnSig_krbDrawTri);
typedef KRB_DRAW_TRI_TEXTURED(fnSig_krbDrawTriTextured);
typedef KRB_DRAW_QUAD_TEXTURED(fnSig_krbDrawQuadTextured);
typedef KRB_DRAW_CIRCLE(fnSig_krbDrawCircle);
typedef KRB_VIEW_TRANSLATE(fnSig_krbViewTranslate);
typedef KRB_SET_MODEL_XFORM(fnSig_krbSetModelXform);
typedef KRB_SET_MODEL_XFORM_2D(fnSig_krbSetModelXform2d);
typedef KRB_LOAD_IMAGE(fnSig_krbLoadImage);
typedef KRB_DELETE_TEXTURE(fnSig_krbDeleteTexture);
typedef KRB_USE_TEXTURE(fnSig_krbUseTexture);
typedef KRB_WORLD_TO_SCREEN(fnSig_krbWorldToScreen);
internal KRB_BEGIN_FRAME(krbBeginFrame);
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho);
internal KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(
	krbSetProjectionOrthoFixedHeight);
internal KRB_DRAW_LINE(krbDrawLine);
internal KRB_DRAW_TRI(krbDrawTri);
internal KRB_DRAW_TRI_TEXTURED(krbDrawTriTextured);
internal KRB_DRAW_QUAD_TEXTURED(krbDrawQuadTextured);
internal KRB_DRAW_CIRCLE(krbDrawCircle);
internal KRB_VIEW_TRANSLATE(krbViewTranslate);
internal KRB_SET_MODEL_XFORM(krbSetModelXform);
internal KRB_SET_MODEL_XFORM_2D(krbSetModelXform2d);
internal KRB_LOAD_IMAGE(krbLoadImage);
internal KRB_DELETE_TEXTURE(krbDeleteTexture);
internal KRB_USE_TEXTURE(krbUseTexture);
internal KRB_WORLD_TO_SCREEN(krbWorldToScreen);
struct KrbApi
{
	fnSig_krbBeginFrame*         beginFrame         = krbBeginFrame;
	fnSig_krbSetProjectionOrtho* setProjectionOrtho = krbSetProjectionOrtho;
	fnSig_krbSetProjectionOrthoFixedHeight* setProjectionOrthoFixedHeight = 
	                                           krbSetProjectionOrthoFixedHeight;
	fnSig_krbDrawLine*           drawLine           = krbDrawLine;
	fnSig_krbDrawTri*            drawTri            = krbDrawTri;
	fnSig_krbDrawTriTextured*    drawTriTextured    = krbDrawTriTextured;
	fnSig_krbDrawQuadTextured*   drawQuadTextured   = krbDrawQuadTextured;
	fnSig_krbDrawCircle*         drawCircle         = krbDrawCircle;
	fnSig_krbViewTranslate*      viewTranslate      = krbViewTranslate;
	fnSig_krbSetModelXform*      setModelXform      = krbSetModelXform;
	fnSig_krbSetModelXform2d*    setModelXform2d    = krbSetModelXform2d;
	fnSig_krbLoadImage*          loadImage          = krbLoadImage;
	fnSig_krbDeleteTexture*      deleteTexture      = krbDeleteTexture;
	fnSig_krbUseTexture*         useTexture         = krbUseTexture;
	fnSig_krbWorldToScreen*      worldToScreen      = krbWorldToScreen;
};