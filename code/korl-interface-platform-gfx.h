#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-utility-math.h"
#include "korl-interface-platform-memory.h"
#include "korl-interface-platform-resource.h"
/** \return \c true if the codepoint should be drawn, \c false otherwise */
#define KORL_GFX_TEXT_CODEPOINT_TEST(name) bool name(void* userData, u32 codepoint, u8 codepointCodeUnits, const u8* currentCodeUnit, u8 bytesPerCodeUnit, Korl_Math_V4f32* o_currentLineColor)
typedef KORL_GFX_TEXT_CODEPOINT_TEST(fnSig_korl_gfx_text_codepointTest);
typedef Korl_Math_V4u8 Korl_Gfx_Color4u8;
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_TRANSPARENT = {  0,   0,   0,   0};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_RED         = {255,   0,   0, 255};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_GREEN       = {  0, 255,   0, 255};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_BLUE        = {  0,   0, 255, 255};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_YELLOW      = {255, 255,   0, 255};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_CYAN        = {  0, 255, 255, 255};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_MAGENTA     = {255,   0, 255, 255};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_WHITE       = {255, 255, 255, 255};
korl_global_const Korl_Gfx_Color4u8 KORL_COLOR4U8_BLACK       = {  0,   0,   0, 255};
typedef struct Korl_Gfx_DrawState_SceneProperties
{
    Korl_Math_M4f32 view, projection;
    f32             seconds;
} Korl_Gfx_DrawState_SceneProperties;
typedef struct Korl_Gfx_DrawState_PushConstantData
{
    u8 vertex  [64];// enough bytes to upload a 4x4 f32 model matrix
    u8 fragment[64];
} Korl_Gfx_DrawState_PushConstantData;
KORL_STATIC_ASSERT(sizeof(Korl_Gfx_DrawState_PushConstantData) <= 128);// vulkan spec 42.1 table 53; minimum limit for VkPhysicalDeviceLimits::maxPushConstantsSize
typedef struct Korl_Gfx_DrawState_Scissor
{
    u32 x, y;
    u32 width, height;
} Korl_Gfx_DrawState_Scissor;
typedef struct Korl_Gfx_Material Korl_Gfx_Material;
typedef struct Korl_Gfx_DrawState_StorageBuffers
{
    Korl_Resource_Handle resourceHandleVertex;
} Korl_Gfx_DrawState_StorageBuffers;
typedef enum Korl_Gfx_Light_Type
    {KORL_GFX_LIGHT_TYPE_POINT       = 1
    ,KORL_GFX_LIGHT_TYPE_DIRECTIONAL = 2
    ,KORL_GFX_LIGHT_TYPE_SPOT        = 3
} Korl_Gfx_Light_Type;
typedef struct Korl_Gfx_Light
{
    u32             type;// Korl_Gfx_Light_Type
    u32             _padding_0[3];
    Korl_Math_V3f32 position; // in world-space; NOTE: this will get automatically transformed into view-space for GLSL
    f32             _padding_1;
    Korl_Math_V3f32 direction;// in world-space; NOTE: this will get automatically transformed into view-space for GLSL
    f32             _padding_2;
    struct
    {
        Korl_Math_V3f32 ambient;
        f32             _padding_0;
        Korl_Math_V3f32 diffuse;
        f32             _padding_1;
        Korl_Math_V3f32 specular;
        f32             _padding_2;
    } color;
    struct
    {
        f32 constant;
        f32 linear;
        f32 quadratic;
    } attenuation;
    struct
    {
        f32 inner;
        f32 outer;
        f32 _padding_0[2];
    } cutOffCosines;
    f32 _padding_3;
} Korl_Gfx_Light;
typedef struct Korl_Gfx_DrawState_Lighting
{
    u32                   lightsCount;
    const Korl_Gfx_Light* lights;
} Korl_Gfx_DrawState_Lighting;
typedef struct Korl_Gfx_DrawState
{
    const Korl_Gfx_Material*                   material;
    const Korl_Gfx_DrawState_SceneProperties*  sceneProperties;
    const Korl_Gfx_DrawState_PushConstantData* pushConstantData;
    const Korl_Gfx_DrawState_Scissor*          scissor;
    const Korl_Gfx_DrawState_StorageBuffers*   storageBuffers;
    const Korl_Gfx_DrawState_Lighting*         lighting;
} Korl_Gfx_DrawState;
typedef enum Korl_Gfx_Camera_Type
    {KORL_GFX_CAMERA_TYPE_PERSPECTIVE
    ,KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC
    ,KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT
} Korl_Gfx_Camera_Type;
typedef enum Korl_Gfx_Camera_ScissorType
    {KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO// default
    ,KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE
} Korl_Gfx_Camera_ScissorType;
typedef struct Korl_Gfx_Camera
{
    Korl_Gfx_Camera_Type        type;
    Korl_Math_V3f32             position;
    Korl_Math_V3f32             normalForward;
    Korl_Math_V3f32             normalUp;
    /** If the viewport scissor coordinates are stored as ratios, they can 
     * always be valid up until the time the camera gets used to draw, allowing 
     * the swap chain dimensions to change however it likes without requiring us 
     * to update these values.  The downside is that these coordinates must 
     * eventually be transformed into integral values at some point, so some 
     * kind of rounding strategy must occur.
     * The scissor coordinates are relative to the upper-left corner of the swap 
     * chain, with the lower-right corner of the swap chain lying in the 
     * positive coordinate space of both axes.  */
    Korl_Math_V2f32             _viewportScissorPosition;// should default to {0,0}
    Korl_Math_V2f32             _viewportScissorSize;// should default to {1,1}
    Korl_Gfx_Camera_ScissorType _scissorType;
    union
    {
        struct
        {
            f32 fovVerticalDegrees;
            f32 clipNear;
            f32 clipFar;
        } perspective;
        struct
        {
            Korl_Math_V2f32 originAnchor;
            f32             clipDepth;
            f32             fixedHeight;
        } orthographic;
    } subCamera;
} Korl_Gfx_Camera;
typedef enum Korl_Gfx_Material_PrimitiveType
    {KORL_GFX_MATERIAL_PRIMITIVE_TYPE_INVALID
    ,KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES
    ,KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_STRIP
    ,KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN
    ,KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINES
    ,KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINE_STRIP
} Korl_Gfx_Material_PrimitiveType;
typedef enum Korl_Gfx_Material_PolygonMode
    {KORL_GFX_MATERIAL_POLYGON_MODE_FILL
    ,KORL_GFX_MATERIAL_POLYGON_MODE_LINE
} Korl_Gfx_Material_PolygonMode;
typedef enum Korl_Gfx_Material_CullMode
    {KORL_GFX_MATERIAL_CULL_MODE_NONE
    ,KORL_GFX_MATERIAL_CULL_MODE_BACK
} Korl_Gfx_Material_CullMode;
typedef enum Korl_Gfx_BlendOperation
    {KORL_GFX_BLEND_OPERATION_ADD
    ,KORL_GFX_BLEND_OPERATION_SUBTRACT
    ,KORL_GFX_BLEND_OPERATION_REVERSE_SUBTRACT
    ,KORL_GFX_BLEND_OPERATION_MIN
    ,KORL_GFX_BLEND_OPERATION_MAX
} Korl_Gfx_BlendOperation;
typedef enum Korl_Gfx_BlendFactor
    {KORL_GFX_BLEND_FACTOR_ZERO
    ,KORL_GFX_BLEND_FACTOR_ONE
    ,KORL_GFX_BLEND_FACTOR_SRC_COLOR
    ,KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_COLOR
    ,KORL_GFX_BLEND_FACTOR_DST_COLOR
    ,KORL_GFX_BLEND_FACTOR_ONE_MINUS_DST_COLOR
    ,KORL_GFX_BLEND_FACTOR_SRC_ALPHA
    ,KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
    ,KORL_GFX_BLEND_FACTOR_DST_ALPHA
    ,KORL_GFX_BLEND_FACTOR_ONE_MINUS_DST_ALPHA
    ,KORL_GFX_BLEND_FACTOR_CONSTANT_COLOR
    ,KORL_GFX_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
    ,KORL_GFX_BLEND_FACTOR_CONSTANT_ALPHA
    ,KORL_GFX_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
    ,KORL_GFX_BLEND_FACTOR_SRC_ALPHA_SATURATE
} Korl_Gfx_BlendFactor;
typedef struct Korl_Gfx_Material_Blend
{
    struct
    {
        Korl_Gfx_BlendOperation operation;
        Korl_Gfx_BlendFactor    factorSource;
        Korl_Gfx_BlendFactor    factorTarget;
    } color, alpha;
} Korl_Gfx_Material_Blend;
korl_global_const Korl_Gfx_Material_Blend KORL_GFX_BLEND_ALPHA               = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_SRC_ALPHA, KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}
                                                                               ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ZERO}};
korl_global_const Korl_Gfx_Material_Blend KORL_GFX_BLEND_ALPHA_PREMULTIPLIED = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}
                                                                               ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ZERO}};
korl_global_const Korl_Gfx_Material_Blend KORL_GFX_BLEND_ADD                 = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_SRC_ALPHA, KORL_GFX_BLEND_FACTOR_ONE}
                                                                               ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE}};
korl_global_const Korl_Gfx_Material_Blend KORL_GFX_BLEND_ADD_PREMULTIPLIED   = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE}
                                                                               ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE}};
enum
    {KORL_GFX_MATERIAL_MODE_FLAGS_NONE              = 0
    ,KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_BLEND       = 1 << 0
    ,KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST  = 1 << 1
    ,KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE = 1 << 2
    ,KORL_GFX_MATERIAL_MODE_FLAGS_ALL               = KORL_U32_MAX
};
typedef u32 Korl_Gfx_Material_Mode_Flags;
typedef struct Korl_Gfx_Material_Modes
{
    Korl_Gfx_Material_PrimitiveType primitiveType;
    Korl_Gfx_Material_PolygonMode   polygonMode;
    Korl_Gfx_Material_CullMode      cullMode;
    Korl_Gfx_Material_Blend         blend;
    Korl_Gfx_Material_Mode_Flags    flags;
} Korl_Gfx_Material_Modes;
/** \note: this struct is padded for GLSL compatibility */
typedef struct Korl_Gfx_Material_FragmentShaderUniform
{
    Korl_Math_V4f32 factorColorBase;
    Korl_Math_V3f32 factorColorEmissive;
    f32             _padding_0;
    Korl_Math_V4f32 factorColorSpecular;
    f32             shininess;
    f32             _padding_1[3];
} Korl_Gfx_Material_FragmentShaderUniform;
typedef struct Korl_Gfx_Material_Maps
{
    Korl_Resource_Handle resourceHandleTextureBase;
    Korl_Resource_Handle resourceHandleTextureSpecular;
    Korl_Resource_Handle resourceHandleTextureEmissive;
} Korl_Gfx_Material_Maps;
typedef struct Korl_Gfx_Material_Shaders
{
    Korl_Resource_Handle resourceHandleShaderVertex;
    Korl_Resource_Handle resourceHandleShaderFragment;
} Korl_Gfx_Material_Shaders;
typedef struct Korl_Gfx_Material
{
    Korl_Gfx_Material_Modes                 modes;
    Korl_Gfx_Material_FragmentShaderUniform fragmentShaderUniform;
    Korl_Gfx_Material_Maps                  maps;
    Korl_Gfx_Material_Shaders               shaders;
} Korl_Gfx_Material;// korl-utility-gfx has APIs to obtain "default" materials
typedef struct Korl_Gfx_ResultRay3d
{
    Korl_Math_V3f32 position;
    Korl_Math_V3f32 direction;
    f32             segmentDistance;
} Korl_Gfx_ResultRay3d;
/** to calculate the total spacing between two lines, use the formula: (ascent - decent) + lineGap */
typedef struct Korl_Gfx_Font_Metrics
{
    f32 ascent;
    f32 decent;
    f32 lineGap;
} Korl_Gfx_Font_Metrics;
typedef struct Korl_Gfx_Font_Resources
{
    Korl_Resource_Handle resourceHandleTexture;
    Korl_Resource_Handle resourceHandleSsboGlyphMeshVertices;
} Korl_Gfx_Font_Resources;
typedef struct Korl_Gfx_Font_TextMetrics
{
    Korl_Math_V2f32 aabbSize;
    u32             visibleGlyphCount;
} Korl_Gfx_Font_TextMetrics;
/* Since we expect that all KORL renderer code requires right-handed 
    triangle normals (counter-clockwise vertex winding), all tri quads have 
    the following formation:
    [3]-[2]
     | \ | 
    [0]-[1] */
korl_global_const u16 KORL_GFX_TRI_QUAD_INDICES[] = 
    { 0, 1, 3
    , 1, 2, 3 };
typedef enum Korl_Gfx_VertexIndexType
    {KORL_GFX_VERTEX_INDEX_TYPE_INVALID
    ,KORL_GFX_VERTEX_INDEX_TYPE_U16
    ,KORL_GFX_VERTEX_INDEX_TYPE_U32
} Korl_Gfx_VertexIndexType;
typedef enum Korl_Gfx_VertexAttributeBinding
    {KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_NORMAL
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT// keep last!
} Korl_Gfx_VertexAttributeBinding;
typedef enum Korl_Gfx_VertexAttributeElementType
    {KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
    ,KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32
    ,KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8
    ,KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32
} Korl_Gfx_VertexAttributeElementType;
typedef enum Korl_Gfx_VertexAttributeInputRate
    {KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX
    ,KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE
} Korl_Gfx_VertexAttributeInputRate;
typedef struct Korl_Gfx_VertexAttributeDescriptor
{
    u32                                 byteOffsetBuffer;// the byte offset within the buffer containing this attribute which contains the first attribute value
    u32                                 byteStride;// aka byteOffsetPerVertex
    Korl_Gfx_VertexAttributeInputRate   inputRate;
    Korl_Gfx_VertexAttributeElementType elementType;
    u8                                  vectorSize;
} Korl_Gfx_VertexAttributeDescriptor;
typedef struct Korl_Gfx_VertexStagingMeta
{
    u32                                instanceCount;
    // indexByteStride is implicitly _always_ == sizeof(indexType), so we don't need such a member
    u32                                indexByteOffsetBuffer;
    u32                                indexCount;
    Korl_Gfx_VertexIndexType           indexType;
    u32                                vertexCount;
    Korl_Gfx_VertexAttributeDescriptor vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT];
} Korl_Gfx_VertexStagingMeta;
typedef void* Korl_Gfx_DeviceBufferHandle;// opaque type; example: for Vulkan implementation, it's a VkBuffer
typedef struct Korl_Gfx_StagingAllocation
{
    void*                       buffer;
    u$                          bytes;
    Korl_Gfx_DeviceBufferHandle deviceBuffer;
    u$                          deviceBufferOffset;
} Korl_Gfx_StagingAllocation;
typedef enum Korl_Gfx_Drawable_Type
    {KORL_GFX_DRAWABLE_TYPE_IMMEDIATE = 1
    ,KORL_GFX_DRAWABLE_TYPE_MESH
} Korl_Gfx_Drawable_Type;
typedef struct Korl_Gfx_Drawable
{
    Korl_Gfx_Drawable_Type type;
    Korl_Math_Transform3d  transform;
    union
    {
        struct
        {
            Korl_Gfx_Material_PrimitiveType primitiveType;
            Korl_Gfx_Material_Mode_Flags    materialModeFlags;
            Korl_Gfx_VertexStagingMeta      vertexStagingMeta;
            Korl_Gfx_StagingAllocation      stagingAllocation;
        } immediate;
        struct
        {
            Korl_Resource_Handle resourceHandleScene3d;
            u8                   rawUtf8Scene3dMeshName[32];//KORL-ISSUE-000-000-163: gfx: we should be able to refactor korl-resource such that we can obtain a ResourceHandle to a "child" MESH resource of a SCENE3D resource, removing the need to have to store the mesh name string of the mesh we're trying to use
            u8                   rawUtf8Scene3dMeshNameSize;// _excluding_ null-terminator
        } mesh;
    } subType;
} Korl_Gfx_Drawable;
#define KORL_FUNCTION_korl_gfx_font_getMetrics(name)                         Korl_Gfx_Font_Metrics      name(acu16 utf16AssetNameFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_gfx_font_getResources(name)                       Korl_Gfx_Font_Resources    name(acu16 utf16AssetNameFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_gfx_font_getUtf8Metrics(name)                     Korl_Gfx_Font_TextMetrics  name(acu16 utf16AssetNameFont, f32 textPixelHeight, acu8 utf8Text)
#define KORL_FUNCTION_korl_gfx_font_generateUtf8(name)                       void                       name(acu16 utf16AssetNameFont, f32 textPixelHeight, acu8 utf8Text, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u32* o_glyphInstanceIndices)
#define KORL_FUNCTION_korl_gfx_font_getUtf16Metrics(name)                    Korl_Gfx_Font_TextMetrics  name(acu16 utf16AssetNameFont, f32 textPixelHeight, acu16 utf16Text)
#define KORL_FUNCTION_korl_gfx_font_generateUtf16(name)                      void                       name(acu16 utf16AssetNameFont, f32 textPixelHeight, acu16 utf16Text, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u32* o_glyphInstanceIndices)
#define KORL_FUNCTION_korl_gfx_useCamera(name)                               void                       name(Korl_Gfx_Camera camera)
#define KORL_FUNCTION_korl_gfx_camera_getCurrent(name)                       Korl_Gfx_Camera            name(void)
#define KORL_FUNCTION_korl_gfx_cameraOrthoGetSize(name)                      Korl_Math_V2f32            name(const Korl_Gfx_Camera*const context)
/** \return \c {{NaN,NaN,NaN},{NaN,NaN,NaN}} if the coordinate transform fails */
#define KORL_FUNCTION_korl_gfx_camera_windowToWorld(name)                    Korl_Gfx_ResultRay3d       name(const Korl_Gfx_Camera*const context, Korl_Math_V2i32 windowPosition)
/** \return \c {Nan,Nan} if the world position is not contained within the 
 * camera's clip space. This does NOT mean that \c non-{NaN,NaN} values are on 
 * the screen!*/
#define KORL_FUNCTION_korl_gfx_camera_worldToWindow(name)                    Korl_Math_V2f32            name(const Korl_Gfx_Camera*const context, Korl_Math_V3f32 worldPosition)
#define KORL_FUNCTION_korl_gfx_getSurfaceSize(name)                          Korl_Math_V2u32            name(void)
#define KORL_FUNCTION_korl_gfx_setClearColor(name)                           void                       name(u8 red, u8 green, u8 blue)
#define KORL_FUNCTION_korl_gfx_draw(name)                                    void                       name(Korl_Gfx_Drawable*const context, const Korl_Gfx_Material* materials, u8 materialsSize)
#define KORL_FUNCTION_korl_gfx_light_use(name)                               void                       name(const Korl_Gfx_Light*const lights, u$ lightsSize)
#define KORL_FUNCTION_korl_gfx_getBlankTexture(name)                         Korl_Resource_Handle       name(void)
#define KORL_FUNCTION_korl_gfx_setDrawState(name)                            void                       name(const Korl_Gfx_DrawState* drawState)
#define KORL_FUNCTION_korl_gfx_stagingAllocate(name)                         Korl_Gfx_StagingAllocation name(const Korl_Gfx_VertexStagingMeta* stagingMeta)
#define KORL_FUNCTION_korl_gfx_stagingReallocate(name)                       Korl_Gfx_StagingAllocation name(const Korl_Gfx_VertexStagingMeta* stagingMeta, const Korl_Gfx_StagingAllocation* stagingAllocation)
#define KORL_FUNCTION_korl_gfx_drawStagingAllocation(name)                   void                       name(const Korl_Gfx_StagingAllocation* stagingAllocation, const Korl_Gfx_VertexStagingMeta* stagingMeta)
#define KORL_FUNCTION_korl_gfx_getBuiltInShaderVertex(name)                  Korl_Resource_Handle       name(const Korl_Gfx_VertexStagingMeta* vertexStagingMeta)
#define KORL_FUNCTION_korl_gfx_getBuiltInShaderFragment(name)                Korl_Resource_Handle       name(const Korl_Gfx_Material* material)
