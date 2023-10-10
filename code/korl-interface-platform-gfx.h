#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-utility-math.h"
#include "korl-interface-platform-memory.h"
#include "korl-interface-platform-resource.h"
typedef Korl_Math_V4u8 Korl_Gfx_Color4u8;
#define KORL_COLOR4U8_TRANSPARENT korl_math_v4u8_make(  0,   0,   0,   0)
#define KORL_COLOR4U8_RED         korl_math_v4u8_make(255,   0,   0, 255)
#define KORL_COLOR4U8_GREEN       korl_math_v4u8_make(  0, 255,   0, 255)
#define KORL_COLOR4U8_BLUE        korl_math_v4u8_make(  0,   0, 255, 255)
#define KORL_COLOR4U8_YELLOW      korl_math_v4u8_make(255, 255,   0, 255)
#define KORL_COLOR4U8_CYAN        korl_math_v4u8_make(  0, 255, 255, 255)
#define KORL_COLOR4U8_MAGENTA     korl_math_v4u8_make(255,   0, 255, 255)
#define KORL_COLOR4U8_WHITE       korl_math_v4u8_make(255, 255, 255, 255)
#define KORL_COLOR4U8_BLACK       korl_math_v4u8_make(  0,   0,   0, 255)
typedef struct Korl_Gfx_DrawState_SceneProperties
{
    Korl_Math_M4f32 projection, view;
    f32             seconds;
    f32             _padding_0[3];
} Korl_Gfx_DrawState_SceneProperties;
typedef struct Korl_Gfx_DrawState_PushConstantData
{
    //KORL-ISSUE-000-000-188: gfx: add ability to specify "bytesUsed" for each pipeline stage to prevent the renderer from uploading push constants for each draw call; related to KORL-PERFORMANCE-000-000-052
    u8 vertex  [64];// enough bytes to upload a 4x4 f32 model matrix
    u8 fragment[64];
} Korl_Gfx_DrawState_PushConstantData;
KORL_STATIC_ASSERT(sizeof(Korl_Gfx_DrawState_PushConstantData) <= 128);// vulkan spec 42.1 table 53; minimum limit for VkPhysicalDeviceLimits::maxPushConstantsSize
typedef struct Korl_Gfx_DrawState_Scissor
{
    u32 x, y;
    u32 width, height;
} Korl_Gfx_DrawState_Scissor;
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
typedef struct Korl_Gfx_Material Korl_Gfx_Material;
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
    ,KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE = 1 << 2/// NOTE: if KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST is not set, this flag is ignored, since disabling depth test in the pipeline prevents the ability to interact with the depth buffer in any way; see Vulkan spec. 28.7
    ,KORL_GFX_MATERIAL_MODE_FLAGS_ALL               = KORL_U32_MAX
};
typedef u32 Korl_Gfx_Material_Mode_Flags;
typedef struct Korl_Gfx_Material_Modes
{
    Korl_Gfx_Material_PolygonMode polygonMode;
    Korl_Gfx_Material_CullMode    cullMode;
    Korl_Gfx_Material_Blend       blend;
    Korl_Gfx_Material_Mode_Flags  flags;
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
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_JOINTS_0
    ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_JOINT_WEIGHTS_0
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
    Korl_Gfx_VertexAttributeElementType elementType;// if == INVALID, this vertex attribute is not used
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
/** For \c KORL_GFX_DRAWABLE_TYPE_RUNTIME , do _not_ store pointers to update 
 * buffers obtained from any of the \c korl_gfx_* APIs, as they are invalidated 
 * either at end-of-frame, _or_ the next call to obtain an update buffer (such 
 * as a resize call). */
typedef enum Korl_Gfx_Drawable_Type
    {KORL_GFX_DRAWABLE_TYPE_INVALID
    ,KORL_GFX_DRAWABLE_TYPE_RUNTIME
    ,KORL_GFX_DRAWABLE_TYPE_MESH
} Korl_Gfx_Drawable_Type;
/** For \c KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME , the user _must_ call 
 * \c korl_gfx_drawable_destroy when they are done using it; otherwise, memory 
 * will be leaked from \c korl-resource and/or whatever renderer we are using, 
 * such as \c korl-vulkan . */
typedef enum Korl_Gfx_Drawable_Runtime_Type
    {KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME
    ,KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME
} Korl_Gfx_Drawable_Runtime_Type;
typedef struct Korl_Resource_Scene3d_Skin Korl_Resource_Scene3d_Skin;
typedef struct Korl_Gfx_Drawable
{
    Korl_Gfx_Drawable_Type type;
    Korl_Math_Transform3d  transform;
    union
    {
        struct
        {
            Korl_Gfx_Drawable_Runtime_Type  type;
            Korl_Gfx_Material_PrimitiveType primitiveType;
            Korl_Gfx_VertexStagingMeta      vertexStagingMeta;
            bool                            interleavedAttributes;
            union
            {
                struct
                {
                    Korl_Gfx_StagingAllocation stagingAllocation;
                } singleFrame;
                struct
                {
                    Korl_Resource_Handle resourceHandleBuffer;
                } multiFrame;
            } subType;
            struct
            {
                /* all members below here are effectively _optional_ */
                Korl_Gfx_Material_Mode_Flags               materialModeFlags;
                Korl_Resource_Handle                       shaderVertex;
                Korl_Resource_Handle                       shaderFragment;
                Korl_Resource_Handle                       storageBufferVertex;
                Korl_Resource_Handle                       materialMapBase;
                const Korl_Gfx_DrawState_PushConstantData* pushConstantData;// kinda hacky, but this effectively allows the user to customize their own PushConstant data on a per-draw-call basis; likely to be refactored in the distant future
            } overrides;
        } runtime;
        struct
        {
            Korl_Resource_Handle        resourceHandleScene3d;
            Korl_Resource_Scene3d_Skin* skin;
            u8                          rawUtf8Scene3dMeshName[32];
            u8                          rawUtf8Scene3dMeshNameSize;// _excluding_ null-terminator
            u32                         meshIndex;// invalid until the scene3d resource becomes loaded
            u8                          meshPrimitives;// 0 => scene3d resource has not yet been loaded; set automatically when user calls korl_gfx_draw with this object; defines the maximum # of Materials the user can pass to korl_gfx_draw
        } mesh;
    } subType;
} Korl_Gfx_Drawable;
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
#define KORL_FUNCTION_korl_gfx_setDrawState(name)                            bool                       name(const Korl_Gfx_DrawState* drawState)
#define KORL_FUNCTION_korl_gfx_stagingAllocate(name)                         Korl_Gfx_StagingAllocation name(const Korl_Gfx_VertexStagingMeta* stagingMeta)
#define KORL_FUNCTION_korl_gfx_stagingReallocate(name)                       Korl_Gfx_StagingAllocation name(const Korl_Gfx_VertexStagingMeta* stagingMeta, const Korl_Gfx_StagingAllocation* stagingAllocation)
#define KORL_FUNCTION_korl_gfx_drawStagingAllocation(name)                   void                       name(const Korl_Gfx_StagingAllocation* stagingAllocation, const Korl_Gfx_VertexStagingMeta* stagingMeta, Korl_Gfx_Material_PrimitiveType primitiveType)
#define KORL_FUNCTION_korl_gfx_drawVertexBuffer(name)                        void                       name(Korl_Resource_Handle resourceHandleBuffer, u$ bufferByteOffset, const Korl_Gfx_VertexStagingMeta* stagingMeta, Korl_Gfx_Material_PrimitiveType primitiveType)
#define KORL_FUNCTION_korl_gfx_getBuiltInShaderVertex(name)                  Korl_Resource_Handle       name(const Korl_Gfx_VertexStagingMeta* vertexStagingMeta)
#define KORL_FUNCTION_korl_gfx_getBuiltInShaderFragment(name)                Korl_Resource_Handle       name(const Korl_Gfx_Material* material)
