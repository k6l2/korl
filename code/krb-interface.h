// Kyle's Renderer Backend interface //
#pragma once
#include "kutil.h"
#include "kmath.h"
using KrbTextureHandle = u32;
struct Color4f32
{
	f32 r, g, b, a;
};
internal Color4f32 lerp(const Color4f32& a, const Color4f32& b, f32 ratio)
{
	Color4f32 result;
	result.r = a.r + ratio*(b.r - a.r);
	result.g = a.g + ratio*(b.g - a.g);
	result.b = a.b + ratio*(b.b - a.b);
	result.a = a.a + ratio*(b.a - a.a);
	return result;
}
namespace krb
{
	global_variable const KrbTextureHandle INVALID_TEXTURE_HANDLE = 0;
	global_variable const Color4f32 TRANSPARENT = {0,0,0,0};
	global_variable const Color4f32 WHITE       = {1,1,1,1};
	global_variable const Color4f32 RED         = {1,0,0,1};
	global_variable const Color4f32 GREEN       = {0,1,0,1};
	global_variable const Color4f32 BLUE        = {0,0,1,1};
	global_variable const Color4f32 YELLOW      = {1,1,0,1};
	struct Context
	{
		Color4f32 defaultColor = WHITE;
	};
	global_variable Context* g_context;
}
#define KRB_BEGIN_FRAME(name) \
	void name(f32 clamped0_1_red, f32 clamped0_1_green, f32 clamped0_1_blue)
#define KRB_SET_DEPTH_TESTING(name) \
	void name(bool enable)
#define KRB_SET_BACKFACE_CULLING(name) \
	void name(bool enable)
#define KRB_SET_WIREFRAME(name) \
	void name(bool enable)
/** Setup a right-handed axis where +Y is UP. */
#define KRB_SET_PROJECTION_ORTHO(name) \
	void name(u32 windowSizeX, u32 windowSizeY, f32 halfDepth)
/** Setup a right-handed axis where +Y is UP. */
#define KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(name) \
	void name(u32 windowSizeX, u32 windowSizeY, u32 fixedHeight, f32 halfDepth)
#define KRB_SET_PROJECTION_FOV(name) \
	void name(f32 horizonFovDegrees, const u32* windowSize, \
	          f32 clipNear, f32 clipFar)
#define KRB_LOOK_AT(name) \
	void name(const f32 v3f32_eye[3], const f32 v3f32_target[3], \
	          const f32 v3f32_worldUp[3])
/**
 * If one of these values is >= `vertexStride` in the below API, that means the 
 * vertex data does not possess the given attribute, so it should be supplied 
 * with some sort of default value.
 */
struct KrbVertexAttributeOffsets
{
	size_t position_3f32;
	size_t color_4f32;
	size_t texCoord_2f32;
};
#define KRB_DRAW_LINES(name) \
	void name(const void* vertices, size_t vertexCount, size_t vertexStride, \
	          const KrbVertexAttributeOffsets& vertexAttribOffsets)
#define KRB_DRAW_TRIS(name) \
	void name(const void* vertices, size_t vertexCount, size_t vertexStride, \
	          const KrbVertexAttributeOffsets& vertexAttribOffsets)
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
#define KRB_DRAW_QUAD_TEXTURED(name) \
	void name(const v2f32& size, const v2f32& ratioAnchor, \
	          const v2f32 texCoords[4], const Color4f32 colors[4])
#define KRB_DRAW_CIRCLE(name) \
	void name(f32 radius, f32 outlineThickness, const Color4f32& colorFill, \
	          const Color4f32& colorOutline, u16 vertexCount)
#define KRB_VIEW_TRANSLATE(name) \
	void name(const v2f32& offset)
#define KRB_SET_MODEL_XFORM(name) \
	void name(const v3f32& translation, const kQuaternion& orientation, \
	          const v3f32& scale)
#define KRB_SET_MODEL_XFORM_2D(name) \
	void name(const v2f32& translation, const kQuaternion& orientation, \
	          const v2f32& scale)
#define KRB_SET_MODEL_XFORM_BILLBOARD(name) \
	void name(bool lockX, bool lockY, bool lockZ)
#define KRB_LOAD_IMAGE(name) \
	KrbTextureHandle name(u32 imageSizeX, u32 imageSizeY, u8* imageDataRGBA)
#define KRB_DELETE_TEXTURE(name) \
	void name(KrbTextureHandle krbTextureHandle)
#define KRB_USE_TEXTURE(name) \
	void name(KrbTextureHandle kth)
/** 
 * @return {NAN,NAN} if the provided world position is not contained within the 
 *         camera's clip space.  This does NOT mean that non-{NAN,NAN} values 
 *         are on the screen!
*/
#define KRB_WORLD_TO_SCREEN(name) \
	v2f32 name(const f32* pWorldPosition, u8 worldPositionDimension)
/**
 * @return false if the ray could not be computed.  Reasons for this function 
 *         failing include:
 *          - view matrix not being invertable
 *          - projection matrix not being invertable
 */
#define KRB_SCREEN_TO_WORLD(name) \
	bool name(const i32 windowPosition[2], const u32 windowSize[2], \
	          f32 o_worldEyeRayPosition[3], f32 o_worldEyeRayDirection[3])
#define KRB_SET_CURRENT_CONTEXT(name) \
	void name(krb::Context* context)
#define KRB_SET_DEFAULT_COLOR(name) \
	void name(const Color4f32& color)
typedef KRB_BEGIN_FRAME(fnSig_krbBeginFrame);
typedef KRB_SET_DEPTH_TESTING(fnSig_krbSetDepthTesting);
typedef KRB_SET_BACKFACE_CULLING(fnSig_krbSetBackfaceCulling);
typedef KRB_SET_WIREFRAME(fnSig_krbSetWireframe);
typedef KRB_SET_PROJECTION_ORTHO(fnSig_krbSetProjectionOrtho);
typedef KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(
	fnSig_krbSetProjectionOrthoFixedHeight);
typedef KRB_SET_PROJECTION_FOV(fnSig_krbSetProjectionFov);
typedef KRB_LOOK_AT(fnSig_krbLookAt);
typedef KRB_DRAW_LINES(fnSig_krbDrawLines);
typedef KRB_DRAW_TRIS(fnSig_krbDrawTris);
typedef KRB_DRAW_QUAD_TEXTURED(fnSig_krbDrawQuadTextured);
typedef KRB_DRAW_CIRCLE(fnSig_krbDrawCircle);
typedef KRB_VIEW_TRANSLATE(fnSig_krbViewTranslate);
typedef KRB_SET_MODEL_XFORM(fnSig_krbSetModelXform);
typedef KRB_SET_MODEL_XFORM_2D(fnSig_krbSetModelXform2d);
typedef KRB_SET_MODEL_XFORM_BILLBOARD(fnSig_krbSetModelXformBillboard);
typedef KRB_LOAD_IMAGE(fnSig_krbLoadImage);
typedef KRB_DELETE_TEXTURE(fnSig_krbDeleteTexture);
typedef KRB_USE_TEXTURE(fnSig_krbUseTexture);
typedef KRB_WORLD_TO_SCREEN(fnSig_krbWorldToScreen);
typedef KRB_SCREEN_TO_WORLD(fnSig_krbScreenToWorld);
typedef KRB_SET_CURRENT_CONTEXT(fnSig_krbSetCurrentContext);
typedef KRB_SET_DEFAULT_COLOR(fnSig_krbSetDefaultColor);
internal KRB_BEGIN_FRAME(krbBeginFrame);
internal KRB_SET_DEPTH_TESTING(krbSetDepthTesting);
internal KRB_SET_BACKFACE_CULLING(krbSetBackfaceCulling);
internal KRB_SET_WIREFRAME(krbSetWireframe);
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho);
internal KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(
	krbSetProjectionOrthoFixedHeight);
internal KRB_SET_PROJECTION_FOV(krbSetProjectionFov);
internal KRB_LOOK_AT(krbLookAt);
internal KRB_DRAW_LINES(krbDrawLines);
internal KRB_DRAW_TRIS(krbDrawTris);
internal KRB_DRAW_QUAD_TEXTURED(krbDrawQuadTextured);
internal KRB_DRAW_CIRCLE(krbDrawCircle);
internal KRB_VIEW_TRANSLATE(krbViewTranslate);
internal KRB_SET_MODEL_XFORM(krbSetModelXform);
internal KRB_SET_MODEL_XFORM_2D(krbSetModelXform2d);
internal KRB_SET_MODEL_XFORM_BILLBOARD(krbSetModelXformBillboard);
internal KRB_LOAD_IMAGE(krbLoadImage);
internal KRB_DELETE_TEXTURE(krbDeleteTexture);
internal KRB_USE_TEXTURE(krbUseTexture);
internal KRB_WORLD_TO_SCREEN(krbWorldToScreen);
internal KRB_SCREEN_TO_WORLD(krbScreenToWorld);
internal KRB_SET_CURRENT_CONTEXT(krbSetCurrentContext);
internal KRB_SET_DEFAULT_COLOR(krbSetDefaultColor);
struct KrbApi
{
	fnSig_krbBeginFrame*         beginFrame         = krbBeginFrame;
	fnSig_krbSetDepthTesting*    setDepthTesting    = krbSetDepthTesting;
	fnSig_krbSetBackfaceCulling* setBackfaceCulling = krbSetBackfaceCulling;
	fnSig_krbSetWireframe*       setWireframe       = krbSetWireframe;
	fnSig_krbSetProjectionOrtho* setProjectionOrtho = krbSetProjectionOrtho;
	fnSig_krbSetProjectionOrthoFixedHeight* 
		setProjectionOrthoFixedHeight = krbSetProjectionOrthoFixedHeight;
	fnSig_krbSetProjectionFov*   setProjectionFov   = krbSetProjectionFov;
	fnSig_krbLookAt*             lookAt             = krbLookAt;
	fnSig_krbDrawLines*          drawLines          = krbDrawLines;
	fnSig_krbDrawTris*           drawTris           = krbDrawTris;
	fnSig_krbDrawQuadTextured*   drawQuadTextured   = krbDrawQuadTextured;
	fnSig_krbDrawCircle*         drawCircle         = krbDrawCircle;
	fnSig_krbViewTranslate*      viewTranslate      = krbViewTranslate;
	fnSig_krbSetModelXform*      setModelXform      = krbSetModelXform;
	fnSig_krbSetModelXform2d*    setModelXform2d    = krbSetModelXform2d;
	fnSig_krbSetModelXformBillboard* 
		setModelXformBillboard = krbSetModelXformBillboard;
	fnSig_krbLoadImage*          loadImage          = krbLoadImage;
	fnSig_krbDeleteTexture*      deleteTexture      = krbDeleteTexture;
	fnSig_krbUseTexture*         useTexture         = krbUseTexture;
	fnSig_krbWorldToScreen*      worldToScreen      = krbWorldToScreen;
	fnSig_krbScreenToWorld*      screenToWorld      = krbScreenToWorld;
	fnSig_krbSetCurrentContext*  setCurrentContext  = krbSetCurrentContext;
	fnSig_krbSetDefaultColor*    setDefaultColor    = krbSetDefaultColor;
};