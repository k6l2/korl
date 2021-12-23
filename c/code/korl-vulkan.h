#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
#include "korl-math.h"
#include "korl-checkCast.h"
#include "korl-memory.h"
#include "korl-assetCache.h"
typedef u16             Korl_Vulkan_VertexIndex;
typedef Korl_Math_V3f32 Korl_Vulkan_Position;
typedef Korl_Math_V2f32 Korl_Vulkan_Uv;
typedef Korl_Math_V3u8  Korl_Vulkan_Color;
korl_internal void korl_vulkan_construct(void);
korl_internal void korl_vulkan_destroy(void);
/** 
 * Not to be confused with \c _korl_vulkan_createSurface (an internal API to 
 * create an actual Vulkan Surface resource)!  "Surface" here refers to an 
 * entire rendering region on the screen.  A common use case is to create a 
 * desktop window, and then create a KORL Vulkan Surface to fill the entire 
 * region of the window.
 */
korl_internal void korl_vulkan_createSurface(
    void* createSurfaceUserData, u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_destroySurface(void);
#if 0/* @todo: bring these API back later when we want the ability to create 
               pipelines using externally managed resources like shaders */
/* @hack: shader loading should probably be more robust */
/** 
 * This must be called AFTER \c korl_vulkan_createSurface since shader modules 
 * require a device to be created!
 */
korl_internal void korl_vulkan_loadShaders(void);
/* @hack: pipeline creation should probably be a little more intelligent */
korl_internal void korl_vulkan_createPipeline(void);
#endif//0
korl_internal void korl_vulkan_frameBegin(const f32 clearRgb[3]);
/** submit all batched graphics command buffers to the Vulkan device */
korl_internal void korl_vulkan_frameEnd(void);
/** 
 * Call this whenever the window is resized.  This will trigger a resize of the 
 * swap chain right before the next draw operation in \c korl_vulkan_draw .
 */
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_batchTriangles_color(
    u32 vertexIndexCount, const Korl_Vulkan_VertexIndex* vertexIndices, 
    u32 vertexCount, const Korl_Vulkan_Position* positions, 
    const Korl_Vulkan_Color* colors);
korl_internal void korl_vulkan_batchTriangles_uv(
    u32 vertexIndexCount, const Korl_Vulkan_VertexIndex* vertexIndices, 
    u32 vertexCount, const Korl_Vulkan_Position* positions, 
    const Korl_Vulkan_Uv* vertexTextureUvs);
korl_internal void korl_vulkan_batchLines_color(
    u32 vertexCount, const Korl_Vulkan_Position* positions, 
    const Korl_Vulkan_Color* colors);
korl_internal void korl_vulkan_setProjectionFov(
    f32 horizontalFovDegrees, f32 clipNear, f32 clipFar);
korl_internal void korl_vulkan_setProjectionOrthographicFixedHeight(f32 fixedHeight, f32 halfDepth);
korl_internal void korl_vulkan_setView(
    Korl_Math_V3f32 positionEye, Korl_Math_V3f32 positionTarget, Korl_Math_V3f32 worldUpNormal);
korl_internal void korl_vulkan_useImageAssetAsTexture(const wchar_t* assetName);
