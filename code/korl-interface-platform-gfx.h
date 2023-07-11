#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-utility-math.h"
#include "korl-interface-platform-memory.h"
#include "korl-interface-platform-resource.h"
/** \return \c true if the codepoint should be drawn, \c false otherwise */
#define KORL_GFX_TEXT_CODEPOINT_TEST(name) bool name(void* userData, u32 codepoint, u8 codepointCodeUnits, const u8* currentCodeUnit, u8 bytesPerCodeUnit, Korl_Math_V4f32* o_currentLineColor)
typedef KORL_GFX_TEXT_CODEPOINT_TEST(fnSig_korl_gfx_text_codepointTest);
//KORL-ISSUE-000-000-096: interface-platform, gfx: get rid of all "Vulkan" identifiers here; we don't want to expose the underlying renderer implementation to the user!
typedef u16            Korl_Vulkan_VertexIndex;//@TODO: delete/deprecate
typedef Korl_Math_V4u8 Korl_Vulkan_Color4u8;//@TODO: rename to Korl_Gfx_Color4u8
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_TRANSPARENT = {  0,   0,   0,   0};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_RED         = {255,   0,   0, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_GREEN       = {  0, 255,   0, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_BLUE        = {  0,   0, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_YELLOW      = {255, 255,   0, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_CYAN        = {  0, 255, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_MAGENTA     = {255,   0, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_WHITE       = {255, 255, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_BLACK       = {  0,   0,   0, 255};
typedef enum Korl_Gfx_PrimitiveType
    {KORL_GFX_PRIMITIVE_TYPE_INVALID
    ,KORL_GFX_PRIMITIVE_TYPE_TRIANGLES
    ,KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_STRIP
    ,KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_FAN
    ,KORL_GFX_PRIMITIVE_TYPE_LINES
    ,KORL_GFX_PRIMITIVE_TYPE_LINE_STRIP
} Korl_Gfx_PrimitiveType;
typedef enum Korl_Gfx_PolygonMode
    {KORL_GFX_POLYGON_MODE_FILL
    ,KORL_GFX_POLYGON_MODE_LINE
} Korl_Gfx_PolygonMode;
typedef enum Korl_Gfx_CullMode
    {KORL_GFX_CULL_MODE_NONE
    ,KORL_GFX_CULL_MODE_BACK
} Korl_Gfx_CullMode;
typedef struct Korl_Gfx_DrawState_Modes
{
    Korl_Gfx_PrimitiveType primitiveType;
    Korl_Gfx_PolygonMode   polygonMode;
    Korl_Gfx_CullMode      cullMode;
    u32                    enableBlend     : 1;
    u32                    enableDepthTest : 1;//KORL-ISSUE-000-000-155: vulkan/gfx: separate depth test & depth write operations in draw state
} Korl_Gfx_DrawState_Modes;
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
typedef struct Korl_Gfx_DrawState_Blend
{
    struct
    {
        Korl_Gfx_BlendOperation operation;
        Korl_Gfx_BlendFactor    factorSource;
        Korl_Gfx_BlendFactor    factorTarget;
    } color, alpha;
} Korl_Gfx_DrawState_Blend;
korl_global_const Korl_Gfx_DrawState_Blend KORL_GFX_BLEND_ALPHA               = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_SRC_ALPHA, KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}
                                                                                ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ZERO}};
korl_global_const Korl_Gfx_DrawState_Blend KORL_GFX_BLEND_ALPHA_PREMULTIPLIED = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}
                                                                                ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ZERO}};
korl_global_const Korl_Gfx_DrawState_Blend KORL_GFX_BLEND_ADD                 = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_SRC_ALPHA, KORL_GFX_BLEND_FACTOR_ONE}
                                                                                ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE}};
korl_global_const Korl_Gfx_DrawState_Blend KORL_GFX_BLEND_ADD_PREMULTIPLIED   = {.color = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE}
                                                                                ,.alpha = {KORL_GFX_BLEND_OPERATION_ADD, KORL_GFX_BLEND_FACTOR_ONE      , KORL_GFX_BLEND_FACTOR_ONE}};
typedef struct Korl_Gfx_DrawState_SceneProperties
{
    Korl_Math_M4f32 view, projection;
    f32             seconds;
} Korl_Gfx_DrawState_SceneProperties;
typedef struct Korl_Gfx_DrawState_Model
{
    Korl_Math_M4f32    transform;
    Korl_Math_Aabb2f32 uvAabb;//@TODO: this was a hack; get rid of this?
} Korl_Gfx_DrawState_Model;
typedef struct Korl_Gfx_DrawState_Scissor
{
    u32 x, y;
    u32 width, height;
} Korl_Gfx_DrawState_Scissor;
typedef struct Korl_Gfx_Material Korl_Gfx_Material;
typedef Korl_Gfx_Material Korl_Gfx_DrawState_Material;//KORL-ISSUE-000-000-160: vulkan: Korl_Gfx_DrawState_Material; likely unnecessary abstraction
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
    const Korl_Gfx_DrawState_Modes*           modes;
    const Korl_Gfx_DrawState_Blend*           blend;//@TODO: eliminate this member; merge this data into `material`?
    const Korl_Gfx_DrawState_SceneProperties* sceneProperties;
    const Korl_Gfx_DrawState_Model*           model;
    const Korl_Gfx_DrawState_Scissor*         scissor;
    const Korl_Gfx_DrawState_Material*        material;
    const Korl_Gfx_DrawState_StorageBuffers*  storageBuffers;
    const Korl_Gfx_DrawState_Lighting*        lighting;
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
typedef struct Korl_Gfx_Material_DrawState
{
    Korl_Gfx_PolygonMode polygonMode;
    Korl_Gfx_CullMode    cullMode;
    // @TODO: alphaMode, blendMode
} Korl_Gfx_Material_DrawState;
/** \note: this struct is padded for GLSL compatibility */
typedef struct Korl_Gfx_Material_Properties
{
    Korl_Math_V4f32 factorColorBase;
    Korl_Math_V3f32 factorColorEmissive;
    f32             _padding_0;
    Korl_Math_V4f32 factorColorSpecular;
    f32             shininess;
    f32             _padding_1[3];
} Korl_Gfx_Material_Properties;
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
    Korl_Gfx_Material_DrawState  drawState;
    Korl_Gfx_Material_Properties properties;
    Korl_Gfx_Material_Maps       maps;
    Korl_Gfx_Material_Shaders    shaders;
} Korl_Gfx_Material;// korl-utility-gfx has APIs to obtain "default" materials
typedef struct Korl_Gfx_Drawable_MaterialSlot
{
    Korl_Gfx_Material material;
    bool              used;
} Korl_Gfx_Drawable_MaterialSlot;
typedef enum Korl_Gfx_Drawable_Type
    {KORL_GFX_DRAWABLE_TYPE_MESH
} Korl_Gfx_Drawable_Type;
typedef struct Korl_Gfx_Drawable
{
    Korl_Gfx_Drawable_Type type;
    struct
    {
        Korl_Math_V3f32      position;
        Korl_Math_V3f32      scale;
        Korl_Math_Quaternion rotation;
    } _model;
    union
    {
        struct
        {
            Korl_Resource_Handle           resourceHandleScene3d;
            Korl_Gfx_Drawable_MaterialSlot materialSlots[1];
            u8                             rawUtf8Scene3dMeshName[32];//KORL-ISSUE-000-000-163: gfx: we should be able to refactor korl-resource such that we can obtain a ResourceHandle to a "child" MESH resource of a SCENE3D resource, removing the need to have to store the mesh name string of the mesh we're trying to use
            u8                             rawUtf8Scene3dMeshNameSize;// _excluding_ null-terminator
        } mesh;
    } subType;
} Korl_Gfx_Drawable;
enum
    {KORL_GFX_BATCH_FLAGS_NONE              = 0
    ,KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST = 1 << 0
    ,KORL_GFX_BATCH_FLAG_DISABLE_BLENDING   = 1 << 1
};
typedef u32 Korl_Gfx_Batch_Flags;// separate flag type definition for C++ compatibility reasons (C++ does not easily allow the | operator for enums; cool language, bro)
//KORL-ISSUE-000-000-097: interface-platform, gfx: maybe just destroy Korl_Gfx_Batch & start over, since we've gotten rid of the concept of "batching" in the platform renderer; the primitive storage struct might also benefit from being a polymorphic tagged union
typedef struct Korl_Gfx_Batch
{
    Korl_Memory_AllocatorHandle allocatorHandle;// storing the allocator handle allows us to simplify API which expands the capacity of the batch (example: add new line to a line batch)
    Korl_Gfx_PrimitiveType primitiveType;
    Korl_Math_V3f32 _position;
    Korl_Math_V3f32 _scale;
    Korl_Math_Quaternion _rotation;
    Korl_Vulkan_Color4u8 modelColor;
    //KORL-PERFORMANCE-000-000-017: GFX; separate batch capacity with batch vertex/index counts
    u32 _vertexIndexCount;
    u32 _vertexCount;
    u32 _instanceCount;
    Korl_Resource_Handle _texture;
    f32                  _textPixelHeight;
    f32                  _textPixelOutline;
    u$                   _textVisibleCharacterCount;
    Korl_Vulkan_Color4u8 _textColor;
    Korl_Vulkan_Color4u8 _textColorOutline;
    Korl_Math_Aabb2f32   _textAabb;
    /** A value of \c {0,0} means the text will be drawn such that the 
     * bottom-left corner of the text AABB will be equivalent to \c _position .  
     * A value of \c {1,1} means the text will be drawn such that the top-right 
     * corner of the text AABB will be equivalent to \c _position .  
     * Default value is \c {0,1} (upper-left corner of text AABB will be located 
     * at \c _position ). */
    Korl_Math_V2f32      _textPositionAnchor;
    //KORL-PERFORMANCE-000-000-002: memory: you will never have a texture & font asset at the same time, so we could potentially overload this to the same string pointer
    wchar_t* _assetNameFont;
    //KORL-PERFORMANCE-000-000-003: memory: once again, only used by text batches; create a PTU here to save space?
    wchar_t* _text;
    Korl_Gfx_DrawState_Blend blend;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Resource_Handle _fontTextureHandle;
    Korl_Resource_Handle _glyphMeshBufferVertices;
    Korl_Vulkan_VertexIndex* _vertexIndices;
    u8                    _vertexPositionDimensions;
    f32*                  _vertexPositions;
    Korl_Vulkan_Color4u8* _vertexColors;
    Korl_Math_V2f32*      _vertexUvs;
    f32                   uvAabbOffset;
    u8                    _instancePositionDimensions;
    f32*                  _instancePositions;
    u32*                  _instanceU32s;// we need a place to store the indices of each glyph in the instanced draw call
    Korl_Resource_Handle resourceHandleShaderFragment;
    Korl_Resource_Handle resourceHandleShaderVertex;
} Korl_Gfx_Batch;
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
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT//@TODO: rename this (and the corresponding constant in korl.glsl) to something generic, like "EXTRA_0" or something, since we can no longer expect this attribute to _just_ be used as text glyph indices
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
typedef struct Korl_Gfx_StagingAllocation
{
    void* buffer;
    u$    bytes;
    void* deviceBuffer;// opaque type; example: for Vulkan implementation, it's a VkBuffer
    u$    deviceBufferOffset;
} Korl_Gfx_StagingAllocation;
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
#define KORL_FUNCTION_korl_gfx_batch(name)                                   void                       name(Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags)
//KORL-ISSUE-000-000-005
#define KORL_FUNCTION_korl_gfx_createBatchRectangleTextured(name)            Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Resource_Handle resourceHandleTexture)
/**
 * \param localOriginNormal relative to the rectangle's lower-left corner
 */
#define KORL_FUNCTION_korl_gfx_createBatchRectangleColored(name)             Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Math_V2f32 localOriginNormal, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_createBatchCircle(name)                       Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, f32 radius, u32 pointCount, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_createBatchCircleSector(name)                 Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, f32 radius, u32 pointCount, Korl_Vulkan_Color4u8 color, f32 sectorRadians)
#define KORL_FUNCTION_korl_gfx_createBatchTriangles(name)                    Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, u32 triangleCount)
#define KORL_FUNCTION_korl_gfx_createBatchQuadsTextured(name)                Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, u32 quadCount, Korl_Resource_Handle resourceHandleTexture)
#define KORL_FUNCTION_korl_gfx_createBatchQuadsTexturedColored(name)         Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, u32 quadCount, Korl_Resource_Handle resourceHandleTexture)
#define KORL_FUNCTION_korl_gfx_createBatchLines(name)                        Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, u32 lineCount)
/** 
 * \param assetNameFont If this value is \c NULL , a default internal font asset 
 * will be used.
 */
#define KORL_FUNCTION_korl_gfx_createBatchText(name)                         Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* assetNameFont, const wchar_t* text, f32 textPixelHeight, Korl_Vulkan_Color4u8 color, f32 outlinePixelSize, Korl_Vulkan_Color4u8 colorOutline)
#define KORL_FUNCTION_korl_gfx_createBatchAxisLines(name)                    Korl_Gfx_Batch*            name(Korl_Memory_AllocatorHandle allocatorHandle)
#define KORL_FUNCTION_korl_gfx_batchSetBlendState(name)                      void                       name(Korl_Gfx_Batch*const context, Korl_Gfx_DrawState_Blend blend)
#define KORL_FUNCTION_korl_gfx_batchSetPosition(name)                        void                       name(Korl_Gfx_Batch*const context, const f32* position, u8 positionDimensions)
#define KORL_FUNCTION_korl_gfx_batchSetPosition2d(name)                      void                       name(Korl_Gfx_Batch*const context, f32 x, f32 y)
#define KORL_FUNCTION_korl_gfx_batchSetPosition2dV2f32(name)                 void                       name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 position)
#define KORL_FUNCTION_korl_gfx_batchSetScale(name)                           void                       name(Korl_Gfx_Batch*const context, Korl_Math_V3f32 scale)
#define KORL_FUNCTION_korl_gfx_batchSetQuaternion(name)                      void                       name(Korl_Gfx_Batch*const context, Korl_Math_Quaternion quaternion)
#define KORL_FUNCTION_korl_gfx_batchSetRotation(name)                        void                       name(Korl_Gfx_Batch*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
#define KORL_FUNCTION_korl_gfx_batchSetVertexColor(name)                     void                       name(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batchAddLine(name)                            void                       name(Korl_Gfx_Batch**const pContext, const f32* p0, const f32* p1, u8 positionDimensions, Korl_Vulkan_Color4u8 color0, Korl_Vulkan_Color4u8 color1)
#define KORL_FUNCTION_korl_gfx_batchSetLine(name)                            void                       name(Korl_Gfx_Batch*const context, u32 lineIndex, const f32* p0, const f32* p1, u8 positionDimensions, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batchTextGetAabb(name)                        Korl_Math_Aabb2f32         name(Korl_Gfx_Batch*const batchContext)
#define KORL_FUNCTION_korl_gfx_batchTextSetPositionAnchor(name)              void                       name(Korl_Gfx_Batch*const batchContext, Korl_Math_V2f32 textPositionAnchor)
#define KORL_FUNCTION_korl_gfx_batchRectangleSetSize(name)                   void                       name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 size)
#define KORL_FUNCTION_korl_gfx_batchRectangleSetColor(name)                  void                       name(Korl_Gfx_Batch*const context, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batch_rectangle_setUv_raw(name)               void                       name(Korl_Gfx_Batch*const context, Korl_Math_Aabb2f32 aabbRawUvs)
#define KORL_FUNCTION_korl_gfx_batch_rectangle_setUv_pixel_to_normal(name)   void                       name(Korl_Gfx_Batch*const context, Korl_Math_Aabb2f32 pixelSpaceAabb)
#define KORL_FUNCTION_korl_gfx_batchCircleSetColor(name)                     void                       name(Korl_Gfx_Batch*const context, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batch_quadsTexturedColored_set(name)          void                       name(Korl_Gfx_Batch*const context, u32 quadIndex, Korl_Math_V2f32 positions[4], Korl_Math_Aabb2f32 aabbRawUvs, const Korl_Vulkan_Color4u8 colors[4], u8 colorsSize)
#define KORL_FUNCTION_korl_gfx_batch_quadsTextured_raw(name)                 void                       name(Korl_Gfx_Batch*const context, u32 quadIndex, Korl_Math_V2f32 size, Korl_Math_V2f32 positionMinimum, Korl_Math_Aabb2f32 aabbRawUvs, const Korl_Vulkan_Color4u8 colors[4], u8 colorsSize)
#define KORL_FUNCTION_korl_gfx_batch_quadsTextured_pixel_to_normal(name)     void                       name(Korl_Gfx_Batch*const context, u32 quadIndex, Korl_Math_V2f32 size, Korl_Math_V2f32 positionMinimum, Korl_Math_Aabb2f32 pixelSpaceAabb)
/** NOTE: this API is stupid; during the inevitable re-write of korl-gfx, 
 * destroy any data structure member pointers which reference memory within the 
 * same allocation */
#define KORL_FUNCTION_korl_gfx_batch_collectDefragmentPointers(name)         void                       name(Korl_Gfx_Batch**const pContext, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent)
#define KORL_FUNCTION_korl_gfx_setClearColor(name)                           void                       name(u8 red, u8 green, u8 blue)
#define KORL_FUNCTION_korl_gfx_draw(name)                                    void                       name(const Korl_Gfx_Drawable*const context)
#define KORL_FUNCTION_korl_gfx_light_use(name)                               void                       name(const Korl_Gfx_Light*const lights, u$ lightsSize)
#define KORL_FUNCTION_korl_gfx_getBlankTexture(name)                         Korl_Resource_Handle       name(void)
//@TODO: why do we need a separate korl_gfx_setDrawState API?  technically, this could just be an _optional_ parameter to `drawStagingAllocation`; maybe just delete this API and see how things work out!
#define KORL_FUNCTION_korl_gfx_setDrawState(name)                            void                       name(const Korl_Gfx_DrawState* drawState)
#define KORL_FUNCTION_korl_gfx_stagingAllocate(name)                         Korl_Gfx_StagingAllocation name(const Korl_Gfx_VertexStagingMeta* stagingMeta)
#define KORL_FUNCTION_korl_gfx_drawStagingAllocation(name)                   void                       name(const Korl_Gfx_StagingAllocation* stagingAllocation, const Korl_Gfx_VertexStagingMeta* stagingMeta)
