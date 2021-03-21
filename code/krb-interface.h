// Kyle's Renderer Backend interface //
#pragma once
#include "kutil.h"
#include "kmath.h"
#include "korl-texture.h"
using KrbTextureHandle = u32;
using RgbaF32 = v4f32;
struct KrbVertexAttributeOffsets
{
	/**
	 * If one of these values is >= `vertexStride` in the below API, that means 
	 * the vertex data does not possess the given attribute.
	 */
	u32 position_3f32;
	u32 color_4f32;
	u32 texCoord_2f32;
public:
	bool operator==(const KrbVertexAttributeOffsets& other) const;
};
namespace krb
{
	global_variable const KrbTextureHandle INVALID_TEXTURE_HANDLE = 0;
	global_variable const RgbaF32 TRANSPARENT = {0,0,0,0};
	global_variable const RgbaF32 BLACK       = {0,0,0,1};
	global_variable const RgbaF32 WHITE       = {1,1,1,1};
	global_variable const RgbaF32 RED         = {1,0,0,1};
	global_variable const RgbaF32 GREEN       = {0,1,0,1};
	global_variable const RgbaF32 CYAN        = {0,1,1,1};
	global_variable const RgbaF32 BLUE        = {0,0,1,1};
	global_variable const RgbaF32 YELLOW      = {1,1,0,1};
	struct Context
	{
		bool frameInProgress = false;
		bool initialized = false;
		u32 windowSizeX;
		u32 windowSizeY;
		/* internal VAO for "immidiate" draw API */
		u32 vaoImmediate;
		/* internal buffer for holding vertex data of "immediate" draw API */
		u32 vboImmediate = 0;
		u32 vboImmediateVertexCount;
		u32 vboImmediateCapacity;
		/* internal shaders for use with "immediate" draw API */
		u32 shaderImmediateVertex;
		u32 shaderImmediateVertexColor;
		u32 shaderImmediateVertexTexNormal;
		u32 shaderImmediateVertexColorTexNormal;
		u32 shaderImmediateFragment;
		u32 shaderImmediateFragmentTexture;
		u32 programImmediate;
		u32 programImmediateColor;
		u32 programImmediateTexture;
		u32 programImmediateColorTexture;
		/* immediate-mode render state */
		KrbVertexAttributeOffsets immediateVertexAttributeOffsets;
		u32 immediateVertexStride;
		u32 immediatePrimitiveType;
		RgbaF32 defaultColor;
		m4f32 m4Projection          = m4f32::IDENTITY;
		m4f32 m4View                = m4f32::IDENTITY;
		m4f32 m4Model               = m4f32::IDENTITY;
		m4f32 m4ModelViewProjection = m4f32::IDENTITY;
	};
	global_variable Context* g_context;
}
/** This API MUST be paired with a call to KRB_END_FRAME! */
#define KRB_BEGIN_FRAME(name) void name(\
	const f32 clamped_0_1_colorRgb[3], const u32 windowSize[2])
/** Calling this function ensures that any buffers that are currently being 
 * filled with data are drawn to the screen if they haven't already been.  This 
 * API MUST be called for every call to KRB_BEGIN_FRAME! */
#define KRB_END_FRAME(name) void name()
#define KRB_SET_DEPTH_TESTING(name) void name(\
	bool enable)
#define KRB_SET_BACKFACE_CULLING(name) void name(\
	bool enable)
#define KRB_SET_WIREFRAME(name) void name(\
	bool enable)
/** Setup a right-handed axis where +Y is UP in screen-space. */
#define KRB_SET_PROJECTION_ORTHO(name) void name(\
	f32 halfDepth)
/** Setup a right-handed axis where +Y is UP in screen-space. */
#define KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(name) void name(\
	f32 fixedHeight, f32 halfDepth)
#define KRB_SET_PROJECTION_FOV(name) void name(\
	f32 horizonFovDegrees, f32 clipNear, f32 clipFar)
#define KRB_LOOK_AT(name) void name(\
	const f32 v3f32_eye[3], const f32 v3f32_target[3], \
	const f32 v3f32_worldUp[3])
#define KRB_DRAW_POINTS(name) void name(\
	const void* vertices, u32 vertexCount, u32 vertexStride, \
	const KrbVertexAttributeOffsets& vertexAttribOffsets)
#define KRB_DRAW_LINES(name) void name(\
	const void* vertices, u32 vertexCount, u32 vertexStride, \
	const KrbVertexAttributeOffsets& vertexAttribOffsets)
#define KRB_DRAW_TRIS(name) void name(\
	const void* vertices, u32 vertexCount, u32 vertexStride, \
	const KrbVertexAttributeOffsets& vertexAttribOffsets)
/** 
 * @param ratioAnchor 
 * is relative to the top-left (-X, Y) most point of the quad mesh.  Positive 
 * values point in the {X,-Y} direction in model-space.  Examples: {0.5f,0.5f} 
 * will set the pivot of the mesh to be in the center.  {0,0} will set the pivot 
 * of the mesh to be the top-left corner of the mesh.
 * @param colors [up-left, down-left, down-right, up-right]
*/
#define KRB_DRAW_QUAD(name) void name(\
	const f32 size[2], const f32 ratioAnchor[2], const RgbaF32 colors[4])
/** 
 * @param ratioAnchor 
 * is relative to the top-left (-X, Y) most point of the quad mesh.  Positive 
 * values point in the {X,-Y} direction in model-space.  Examples: {0.5f,0.5f} 
 * will set the pivot of the mesh to be in the center.  {0,0} will set the pivot 
 * of the mesh to be the top-left corner of the mesh, which should correspond to 
 * the top-left corner of the texture!
 * @param colors [up-left, down-left, down-right, up-right]
 * @param texNormalMin 
 * Texture-space origin is the upper-left corner of the texture.
 * @param texNormalMax 
 * Texture-space origin is the upper-left corner of the texture.
*/
#define KRB_DRAW_QUAD_TEXTURED(name) void name(\
	const f32 size[2], const f32 ratioAnchor[2], \
	const RgbaF32 colors[4], const f32 texNormalMin[2], \
	const f32 texNormalMax[2])
#define KRB_DRAW_CIRCLE(name) void name(\
	f32 radius, f32 outlineThickness, const RgbaF32& colorFill, \
	const RgbaF32& colorOutline, u16 vertexCount)
#define KRB_SET_VIEW_XFORM_2D(name) void name(\
	const v2f32& worldPositionCenter)
#define KRB_SET_MODEL_XFORM(name) void name(\
	const v3f32& translation, const q32& orientation, const v3f32& scale)
#define KRB_SET_MODEL_XFORM_2D(name) void name(\
	const v2f32& translation, const q32& orientation, const v2f32& scale)
#define KRB_SET_MODEL_MATRIX(name) void name(\
	const f32 rowMajorMatrix4x4[16])
#define KRB_GET_MATRICES_MVP(name) void name(\
	f32 o_model[16], f32 o_view[16], f32 o_projection[16])
#define KRB_SET_MATRICES_MVP(name) void name(\
	const f32 model[16], const f32 view[16], const f32 projection[16])
#if 0
#define KRB_SET_MODEL_XFORM_BILLBOARD(name) void name(\
	bool lockX, bool lockY, bool lockZ)
#endif//0
enum class KorlPixelDataFormat : u8
	{ RGBA
	, BGR };
#if 0/* I'm not sure if this is even being used anywhere anymore... */
global_variable const u8 KORL_PIXEL_DATA_FORMAT_BITS_PER_PIXEL[] = 
	{ 32
	, 24 };
#endif//0
#define KRB_LOAD_IMAGE(name) KrbTextureHandle name(\
	u32 imageSizeX, u32 imageSizeY, u8* pixelData, \
	KorlPixelDataFormat pixelDataFormat)
#define KRB_DELETE_TEXTURE(name) void name(\
	KrbTextureHandle krbTextureHandle)
#define KRB_CONFIGURE_TEXTURE(name) void name(\
	KrbTextureHandle kth, const KorlTextureMetaData& texMeta)
#define KRB_USE_TEXTURE(name) void name(KrbTextureHandle kth)
/** 
 * @return {NAN,NAN} if the provided world position is not contained within the 
 * camera's clip space.  This does NOT mean that non-{NAN,NAN} values are on the 
 * screen!
 */
#define KRB_WORLD_TO_SCREEN(name) v2f32 name(\
	const f32* pWorldPosition, u8 worldPositionDimension)
/**
 * @return 
 * false if the ray could not be computed.  Reasons for this function failing 
 * include:
 *     - view matrix not being invertable
 *     - projection matrix not being invertable
 */
#define KRB_SCREEN_TO_WORLD(name) bool name(\
	const i32 windowPosition[2], \
	f32 o_worldEyeRayPosition[3], f32 o_worldEyeRayDirection[3])
#define KRB_SET_CURRENT_CONTEXT(name) void name(\
	krb::Context* context)
#define KRB_SET_DEFAULT_COLOR(name) void name(\
	const RgbaF32& color)
/** The origin of clip box coordinates is the bottom-left corner of the window, 
 * with both axes moving toward the upper-right corner of the window. */
#define KRB_SET_CLIP_BOX(name) void name(\
	i32 left, i32 bottom, u32 width, u32 height)
#define KRB_DISABLE_CLIP_BOX(name) void name()
typedef KRB_BEGIN_FRAME(fnSig_krbBeginFrame);
typedef KRB_END_FRAME(fnSig_krbEndFrame);
typedef KRB_SET_DEPTH_TESTING(fnSig_krbSetDepthTesting);
typedef KRB_SET_BACKFACE_CULLING(fnSig_krbSetBackfaceCulling);
typedef KRB_SET_WIREFRAME(fnSig_krbSetWireframe);
typedef KRB_SET_PROJECTION_ORTHO(fnSig_krbSetProjectionOrtho);
typedef KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(
	fnSig_krbSetProjectionOrthoFixedHeight);
typedef KRB_SET_PROJECTION_FOV(fnSig_krbSetProjectionFov);
typedef KRB_LOOK_AT(fnSig_krbLookAt);
typedef KRB_DRAW_POINTS(fnSig_krbDrawPoints);
typedef KRB_DRAW_LINES(fnSig_krbDrawLines);
typedef KRB_DRAW_TRIS(fnSig_krbDrawTris);
typedef KRB_DRAW_QUAD(fnSig_krbDrawQuad);
typedef KRB_DRAW_QUAD_TEXTURED(fnSig_krbDrawQuadTextured);
typedef KRB_DRAW_CIRCLE(fnSig_krbDrawCircle);
typedef KRB_SET_VIEW_XFORM_2D(fnSig_krbSetViewXform2d);
typedef KRB_SET_MODEL_XFORM(fnSig_krbSetModelXform);
typedef KRB_SET_MODEL_XFORM_2D(fnSig_krbSetModelXform2d);
typedef KRB_SET_MODEL_MATRIX(fnSig_krbSetModelMatrix);
typedef KRB_GET_MATRICES_MVP(fnSig_krbGetMatricesMvp);
typedef KRB_SET_MATRICES_MVP(fnSig_krbSetMatricesMvp);
#if 0
typedef KRB_SET_MODEL_XFORM_BILLBOARD(fnSig_krbSetModelXformBillboard);
#endif//0
typedef KRB_LOAD_IMAGE(fnSig_krbLoadImage);
typedef KRB_DELETE_TEXTURE(fnSig_krbDeleteTexture);
typedef KRB_CONFIGURE_TEXTURE(fnSig_krbConfigureTexture);
typedef KRB_USE_TEXTURE(fnSig_krbUseTexture);
typedef KRB_WORLD_TO_SCREEN(fnSig_krbWorldToScreen);
typedef KRB_SCREEN_TO_WORLD(fnSig_krbScreenToWorld);
typedef KRB_SET_CURRENT_CONTEXT(fnSig_krbSetCurrentContext);
typedef KRB_SET_DEFAULT_COLOR(fnSig_krbSetDefaultColor);
typedef KRB_SET_CLIP_BOX(fnSig_krbSetClipBox);
typedef KRB_DISABLE_CLIP_BOX(fnSig_krbDisableClipBox);
internal KRB_BEGIN_FRAME(krbBeginFrame);
internal KRB_END_FRAME(krbEndFrame);
internal KRB_SET_DEPTH_TESTING(krbSetDepthTesting);
internal KRB_SET_BACKFACE_CULLING(krbSetBackfaceCulling);
internal KRB_SET_WIREFRAME(krbSetWireframe);
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho);
internal KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(
	krbSetProjectionOrthoFixedHeight);
internal KRB_SET_PROJECTION_FOV(krbSetProjectionFov);
internal KRB_LOOK_AT(krbLookAt);
internal KRB_DRAW_POINTS(krbDrawPoints);
internal KRB_DRAW_LINES(krbDrawLines);
internal KRB_DRAW_TRIS(krbDrawTris);
internal KRB_DRAW_QUAD(krbDrawQuad);
internal KRB_DRAW_QUAD_TEXTURED(krbDrawQuadTextured);
internal KRB_DRAW_CIRCLE(krbDrawCircle);
internal KRB_SET_VIEW_XFORM_2D(krbSetViewXform2d);
internal KRB_SET_MODEL_XFORM(krbSetModelXform);
internal KRB_SET_MODEL_XFORM_2D(krbSetModelXform2d);
internal KRB_SET_MODEL_MATRIX(krbSetModelMatrix);
internal KRB_GET_MATRICES_MVP(krbGetMatricesMvp);
internal KRB_SET_MATRICES_MVP(krbSetMatricesMvp);
#if 0
internal KRB_SET_MODEL_XFORM_BILLBOARD(krbSetModelXformBillboard);
#endif//0
internal KRB_LOAD_IMAGE(krbLoadImage);
internal KRB_DELETE_TEXTURE(krbDeleteTexture);
internal KRB_CONFIGURE_TEXTURE(krbConfigureTexture);
internal KRB_USE_TEXTURE(krbUseTexture);
internal KRB_WORLD_TO_SCREEN(krbWorldToScreen);
internal KRB_SCREEN_TO_WORLD(krbScreenToWorld);
internal KRB_SET_CURRENT_CONTEXT(krbSetCurrentContext);
internal KRB_SET_DEFAULT_COLOR(krbSetDefaultColor);
internal KRB_SET_CLIP_BOX(krbSetClipBox);
internal KRB_DISABLE_CLIP_BOX(krbDisableClipBox);
struct KrbApi
{
	fnSig_krbBeginFrame*         beginFrame         = krbBeginFrame;
	fnSig_krbEndFrame*           endFrame           = krbEndFrame;
	fnSig_krbSetDepthTesting*    setDepthTesting    = krbSetDepthTesting;
	fnSig_krbSetBackfaceCulling* setBackfaceCulling = krbSetBackfaceCulling;
	fnSig_krbSetWireframe*       setWireframe       = krbSetWireframe;
	fnSig_krbSetProjectionOrtho* setProjectionOrtho = krbSetProjectionOrtho;
	fnSig_krbSetProjectionOrthoFixedHeight* 
		setProjectionOrthoFixedHeight = krbSetProjectionOrthoFixedHeight;
	fnSig_krbSetProjectionFov*   setProjectionFov   = krbSetProjectionFov;
	fnSig_krbLookAt*             lookAt             = krbLookAt;
	fnSig_krbDrawPoints*         drawPoints         = krbDrawPoints;
	fnSig_krbDrawLines*          drawLines          = krbDrawLines;
	fnSig_krbDrawTris*           drawTris           = krbDrawTris;
	fnSig_krbDrawQuad*           drawQuad           = krbDrawQuad;
	fnSig_krbDrawQuadTextured*   drawQuadTextured   = krbDrawQuadTextured;
	fnSig_krbDrawCircle*         drawCircle         = krbDrawCircle;
	fnSig_krbSetViewXform2d*     setViewXform2d     = krbSetViewXform2d;
	fnSig_krbSetModelXform*      setModelXform      = krbSetModelXform;
	fnSig_krbSetModelXform2d*    setModelXform2d    = krbSetModelXform2d;
	fnSig_krbSetModelMatrix*     setModelMatrix     = krbSetModelMatrix;
	fnSig_krbGetMatricesMvp*     getMatricesMvp     = krbGetMatricesMvp;
	fnSig_krbSetMatricesMvp*     setMatricesMvp     = krbSetMatricesMvp;
#if 0
	fnSig_krbSetModelXformBillboard* 
		setModelXformBillboard = krbSetModelXformBillboard;
#endif//0
	fnSig_krbLoadImage*          loadImage          = krbLoadImage;
	fnSig_krbDeleteTexture*      deleteTexture      = krbDeleteTexture;
	fnSig_krbConfigureTexture*   configureTexture   = krbConfigureTexture;
	fnSig_krbUseTexture*         useTexture         = krbUseTexture;
	fnSig_krbWorldToScreen*      worldToScreen      = krbWorldToScreen;
	fnSig_krbScreenToWorld*      screenToWorld      = krbScreenToWorld;
	fnSig_krbSetCurrentContext*  setCurrentContext  = krbSetCurrentContext;
	fnSig_krbSetDefaultColor*    setDefaultColor    = krbSetDefaultColor;
	fnSig_krbSetClipBox*         setClipBox         = krbSetClipBox;
	fnSig_krbDisableClipBox*     disableClipBox     = krbDisableClipBox;
};
