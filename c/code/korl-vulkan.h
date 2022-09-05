/**
 * ----- API -----
 * korl_vulkan_safeCast_u$_to_vertexIndex
 * korl_vulkan_construct
 * korl_vulkan_destroy
 * korl_vulkan_createSurface
 *     Not to be confused with \c _korl_vulkan_createSurface (an internal API to 
 *     create an actual Vulkan Surface resource)!  "Surface" here refers to an 
 *     entire rendering region on the screen.  A common use case is to create a 
 *     desktop window, and then create a KORL Vulkan Surface to fill the entire 
 *     region of the window.
 * korl_vulkan_destroySurface
 * korl_vulkan_getSurfaceSize
 * korl_vulkan_clearAllDeviceAssets
 * korl_vulkan_setSurfaceClearColor
 * korl_vulkan_frameBegin
 * korl_vulkan_frameEnd
 * korl_vulkan_deferredResize
 *     Call this whenever the window is resized.  This will trigger a resize of the 
 *     swap chain right before the next draw operation in \c korl_vulkan_draw .
 * korl_vulkan_setDrawState
 * korl_vulkan_draw
 * ----- Data Structures -----
 * Korl_Vulkan_DrawVertexData
 * Korl_Vulkan_DrawState_Blend
 * Korl_Vulkan_DrawState
 */
#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
#include "korl-math.h"
#include "korl-checkCast.h"
#include "korl-memory.h"
#include "korl-assetCache.h"
#include "korl-interface-platform.h"
typedef struct Korl_Vulkan_DrawVertexData
{
    Korl_Vulkan_PrimitiveType      primitiveType;    // required
    Korl_Vulkan_VertexIndex        indexCount;       // optional
    const Korl_Vulkan_VertexIndex* indices;          // optional
    u32                            vertexCount;      // required
    const Korl_Math_V2f32*         positions2d;      // required
    u32                            positions2dStride;// required
    const f32*                     positionsZ;       // optional
    u32                            positionsZStride; // optional
    const Korl_Math_V2f32*         uvs;              // optional
    u32                            uvsStride;        // optional
    const Korl_Vulkan_Color4u8*    colors;           // optional
    u32                            colorsStride;     // optional
} Korl_Vulkan_DrawVertexData;
typedef struct Korl_Vulkan_DrawState_Blend
{
    Korl_Vulkan_BlendOperation opColor;
    Korl_Vulkan_BlendFactor factorColorSource;
    Korl_Vulkan_BlendFactor factorColorTarget;
    Korl_Vulkan_BlendOperation opAlpha;
    Korl_Vulkan_BlendFactor factorAlphaSource;
    Korl_Vulkan_BlendFactor factorAlphaTarget;
} Korl_Vulkan_DrawState_Blend;
typedef struct Korl_Vulkan_DrawState
{
    u32 enableBlend     : 1;
    u32 enableDepthTest : 1;
    const Korl_Vulkan_DrawState_Blend* blend;
} Korl_Vulkan_DrawState;
korl_internal Korl_Vulkan_VertexIndex korl_vulkan_safeCast_u$_to_vertexIndex(u$ x);
korl_internal void korl_vulkan_construct(void);
korl_internal void korl_vulkan_destroy(void);
korl_internal void korl_vulkan_createSurface(void* createSurfaceUserData, u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_destroySurface(void);
korl_internal Korl_Math_V2u32 korl_vulkan_getSurfaceSize(void);
korl_internal void korl_vulkan_clearAllDeviceAssets(void);
#if 0//KORL-ISSUE-000-000-027: Vulkan; feature: add shader/pipeline resource API
/** 
 * This must be called AFTER \c korl_vulkan_createSurface since shader modules 
 * require a device to be created!
 */
korl_internal void korl_vulkan_loadShaders(void);
korl_internal void korl_vulkan_createPipeline(void);
#endif//0
korl_internal void korl_vulkan_setSurfaceClearColor(const f32 clearRgb[3]);
korl_internal void korl_vulkan_frameBegin(void);
korl_internal void korl_vulkan_frameEnd(void);
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_setDrawState(const Korl_Vulkan_DrawState* state);
korl_internal void korl_vulkan_draw(const Korl_Vulkan_DrawVertexData* vertexData);














/** @TODO: everything below here needs to be refactored, and at the very least the implementation needs to be re-written; 
 * - consider making a lot of these be a part of `Korl_Vulkan_DrawState` for API simplification?  
 *   This could also potentially have nice implementation simplification as well, since I noticed a lot of the implementations of these
 *   start out & end in basically the same way!
 * ----------------------------------------------------------------------------------------------------------------------------- */
korl_internal void korl_vulkan_setProjectionFov(f32 horizontalFovDegrees, f32 clipNear, f32 clipFar);
/** 
 * \param originRatioX adjust the position of the origin relative to the size 
 * of the swap chain.  A value of \c 0.5f will center the origin on the screen.  
 * A value of \c 0.f will center the origin on the left side of the screen.
 * \param originRatioY adjust the position of the origin relative to the size 
 * of the swap chain.  A value of \c 0.5f will center the origin on the screen.  
 * A value of \c 0.f will center the origin on the top of the screen.
 */
korl_internal void korl_vulkan_setProjectionOrthographic(f32 depth, f32 originRatioX, f32 originRatioY);
korl_internal void korl_vulkan_setProjectionOrthographicFixedHeight(f32 fixedHeight, f32 depth, f32 originRatioX, f32 originRatioY);
korl_internal void korl_vulkan_setView(Korl_Math_V3f32 positionEye, Korl_Math_V3f32 positionTarget, Korl_Math_V3f32 worldUpNormal);
/**
 * Values of parameters are in window-space coordinates.  The upper-left corner 
 * of the viewport is the origin, and the lower-right corner lies in the 
 * positive range of both axes.
 */
korl_internal void korl_vulkan_setScissor(u32 x, u32 y, u32 width, u32 height);
korl_internal void korl_vulkan_setModel(Korl_Vulkan_Position position, Korl_Math_Quaternion rotation, Korl_Vulkan_Position scale);
korl_internal void korl_vulkan_useImageAssetAsTexture(const wchar_t* assetName);
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(korl_vulkan_onAssetHotReload);
korl_internal Korl_Vulkan_TextureHandle korl_vulkan_textureCreate(u32 sizeX, u32 sizeY, Korl_Vulkan_Color4u8* imageBuffer);
korl_internal void korl_vulkan_textureUpdate(Korl_Vulkan_TextureHandle textureHandle, u32 sizeX, u32 sizeY, Korl_Vulkan_Color4u8* imageBuffer);
korl_internal void korl_vulkan_textureDestroy(Korl_Vulkan_TextureHandle textureHandle);
korl_internal void korl_vulkan_useTexture(Korl_Vulkan_TextureHandle textureHandle);
