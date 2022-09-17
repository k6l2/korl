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
    Korl_Vulkan_PrimitiveType      primitiveType;     // required
    Korl_Vulkan_VertexIndex        indexCount;        // optional
    const Korl_Vulkan_VertexIndex* indices;           // optional
    u32                            vertexCount;       // required
    u8                             positionDimensions;// required; only acceptable values: {2, 3}
    const f32*                     positions;         // required
    u32                            positionsStride;   // required
    const Korl_Math_V2f32*         uvs;               // optional
    u32                            uvsStride;         // optional
    const Korl_Vulkan_Color4u8*    colors;            // optional
    u32                            colorsStride;      // optional
} Korl_Vulkan_DrawVertexData;
typedef struct Korl_Vulkan_DrawState_Features
{
    u32 enableBlend     : 1;
    u32 enableDepthTest : 1;
} Korl_Vulkan_DrawState_Features;
typedef struct Korl_Vulkan_DrawState_Blend
{
    Korl_Vulkan_BlendOperation opColor;
    Korl_Vulkan_BlendFactor factorColorSource;
    Korl_Vulkan_BlendFactor factorColorTarget;
    Korl_Vulkan_BlendOperation opAlpha;
    Korl_Vulkan_BlendFactor factorAlphaSource;
    Korl_Vulkan_BlendFactor factorAlphaTarget;
} Korl_Vulkan_DrawState_Blend;
typedef enum Korl_Vulkan_DrawState_ProjectionType
    { KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_FOV
    , KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_ORTHOGRAPHIC
    , KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT
} Korl_Vulkan_DrawState_ProjectionType;
typedef struct Korl_Vulkan_DrawState_Projection
{
    Korl_Vulkan_DrawState_ProjectionType type;
    union
    {
        struct
        {
            f32 horizontalFovDegrees;
            f32 clipNear;
            f32 clipFar;
        } fov;
        struct
        {
            f32 fixedHeight;
            f32 depth;
            f32 originRatioX;
            f32 originRatioY;
        } orthographic;
    } subType;
} Korl_Vulkan_DrawState_Projection;
typedef struct Korl_Vulkan_DrawState_View
{
    Korl_Math_V3f32 positionEye;
    Korl_Math_V3f32 positionTarget;
    Korl_Math_V3f32 worldUpNormal;
} Korl_Vulkan_DrawState_View;
typedef struct Korl_Vulkan_DrawState_Model
{
    Korl_Math_V3f32      translation;
    Korl_Math_Quaternion rotation;
    Korl_Math_V3f32      scale;
} Korl_Vulkan_DrawState_Model;
typedef struct Korl_Vulkan_DrawState_Scissor
{
    u32 x;
    u32 y;
    u32 width;
    u32 height;
} Korl_Vulkan_DrawState_Scissor;
typedef struct Korl_Vulkan_DrawState_Samplers
{
    Korl_Vulkan_DeviceAssetHandle texture;
} Korl_Vulkan_DrawState_Samplers;
typedef struct Korl_Vulkan_DrawState
{
    const Korl_Vulkan_DrawState_Features*   features;
    const Korl_Vulkan_DrawState_Blend*      blend;
    const Korl_Vulkan_DrawState_Projection* projection;
    const Korl_Vulkan_DrawState_View*       view;
    const Korl_Vulkan_DrawState_Model*      model;
    const Korl_Vulkan_DrawState_Scissor*    scissor;
    const Korl_Vulkan_DrawState_Samplers*   samplers;
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
korl_internal Korl_Vulkan_DeviceAssetHandle korl_vulkan_deviceAsset_createTexture(u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_deviceAsset_destroy(Korl_Vulkan_DeviceAssetHandle deviceAssetHandle);
korl_internal void korl_vulkan_texture_update(Korl_Vulkan_DeviceAssetHandle textureHandle, const Korl_Vulkan_Color4u8* pixelData);
korl_internal Korl_Math_V2u32 korl_vulkan_texture_getSize(const Korl_Vulkan_DeviceAssetHandle textureHandle);
/** @TODO: move this code into korl-gfx */
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(korl_vulkan_onAssetHotReload);
