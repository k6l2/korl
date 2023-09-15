#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-gfx.h"
#include "utility/korl-utility-resource.h"
/** \return \c true if the codepoint should be drawn, \c false otherwise */
#define KORL_GFX_TEXT_CODEPOINT_TEST(name) bool name(void* userData, u32 codepoint, u8 codepointCodeUnits, const u8* currentCodeUnit, u8 bytesPerCodeUnit, Korl_Math_V4f32* o_currentLineColor)
typedef KORL_GFX_TEXT_CODEPOINT_TEST(fnSig_korl_gfx_text_codepointTest);
typedef struct Korl_Gfx_Text
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Gfx_VertexStagingMeta  vertexStagingMeta;
    Korl_Resource_Handle        resourceHandleBufferText;// used to store the vertex buffer which contains all of the _Korl_Gfx_Text_GlyphInstance data for all the lines contained in this Text object; each line will access an offset into this vertex buffer determined by the sum of all visible characters of all lines prior to it in the stbDaLines list
    struct _Korl_Gfx_Text_Line* stbDaLines;// the user can obtain the line count by calling arrlenu(text->stbDaLines)
    f32                         textPixelHeight;
    Korl_Resource_Handle        resourceHandleFont;
    Korl_Math_Transform3d       transform;
    Korl_Math_V2f32             _modelAabbSize;// not valid until fifo add/remove APIs have been called
    u32                         totalVisibleGlyphs;// redundant value; acceleration for operations on resourceHandleBufferText
} Korl_Gfx_Text;
korl_internal Korl_Gfx_Text*     korl_gfx_text_create(Korl_Memory_AllocatorHandle allocator, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight);
korl_internal void               korl_gfx_text_destroy(Korl_Gfx_Text* context);
korl_internal void               korl_gfx_text_collectDefragmentPointers(Korl_Gfx_Text* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent);
korl_internal void               korl_gfx_text_fifoAdd(Korl_Gfx_Text* context, acu16 utf16Text, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData);
korl_internal void               korl_gfx_text_fifoRemove(Korl_Gfx_Text* context, u$ lineCount);
korl_internal void               korl_gfx_text_draw(Korl_Gfx_Text* context, Korl_Math_Aabb2f32 visibleRegion);
korl_internal Korl_Math_V2f32    korl_gfx_text_getModelAabbSize(const Korl_Gfx_Text* context);
korl_internal u8                 korl_gfx_indexBytes(Korl_Gfx_VertexIndexType vertexIndexType);
korl_internal Korl_Math_V4f32    korl_gfx_color_toLinear(Korl_Gfx_Color4u8 color);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultUnlit(void);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultUnlitFlags(Korl_Gfx_Material_Mode_Flags flags);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultUnlitFlagsColorbase(Korl_Gfx_Material_Mode_Flags flags, Korl_Math_V4f32 colorLinear4Base);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultSolid(void);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultSolidColorbase(Korl_Math_V4f32 colorLinear4Base);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultLit(void);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultLitColorbase(Korl_Math_V4f32 colorLinear4Base);
korl_internal Korl_Gfx_Camera    korl_gfx_camera_createFov(f32 fovVerticalDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 normalForward, Korl_Math_V3f32 normalUp);
korl_internal Korl_Gfx_Camera    korl_gfx_camera_createOrtho(f32 clipDepth);
korl_internal Korl_Gfx_Camera    korl_gfx_camera_createOrthoFixedHeight(f32 fixedHeight, f32 clipDepth);
korl_internal void               korl_gfx_camera_fovRotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians, Korl_Math_V3f32 target);
/** Note that scissor coordinates use swap chain coordinates, where the origin 
 * is the upper-left corner of the swap chain, & {+X,+Y} points to the 
 * bottom-right.  */
//KORL-ISSUE-000-000-004
korl_internal void               korl_gfx_camera_setScissor(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY);
/** Note that scissor coordinates use swap chain coordinates, where the origin is 
 * the upper-left corner of the swap chain, & {+X,+Y} points to the bottom-right.  */
korl_internal void               korl_gfx_camera_setScissorPercent(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight);
/** Translate the camera such that the orthographic camera's origin is located 
 * at the position in the swapchain's coordinate space relative to the 
 * bottom-left corner.
 * Negative size ratio values are valid.
 * This function does NOT flip the coordinate space or anything like that!
 * Examples:
 * - by default (without calling this function) the origin will be 
 *   \c {0.5f,0.5f}, which is the center of the screen
 * - if you want the origin to be in the bottom-left corner of the window, pass 
 *   \c {0.f,0.f} as size ratio coordinates */
korl_internal void               korl_gfx_camera_orthoSetOriginAnchor(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY);
korl_internal Korl_Math_M4f32    korl_gfx_camera_projection(const Korl_Gfx_Camera*const context);
korl_internal Korl_Math_M4f32    korl_gfx_camera_view(const Korl_Gfx_Camera*const context);
korl_internal void               korl_gfx_camera_drawFrustum(const Korl_Gfx_Camera*const context, const Korl_Gfx_Material* material);
korl_internal void               korl_gfx_drawable_destroy(Korl_Gfx_Drawable* context);
typedef enum Korl_Gfx_RuntimeDrawableAttributeDatatype
    {KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_INVALID
    ,KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_U32
    ,KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32
    ,KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32
    ,KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8
} Korl_Gfx_RuntimeDrawableAttributeDatatype;
typedef struct Korl_Gfx_CreateInfoRuntimeDrawable
{
    Korl_Gfx_Drawable_Runtime_Type            type;// if set to KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME, you _must_ call `korl_gfx_drawable_destroy` on the resulting Korl_Gfx_Drawable, or resources will be leaked
    Korl_Gfx_VertexIndexType                  vertexIndexType;// only set this to non-zero if you want to draw indexed geometry
    Korl_Gfx_RuntimeDrawableAttributeDatatype attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT];// by default (the {0} struct), all attributes are INVALID (even BINDING_POSITION), and thus wont be allocated; you must configure each attribute datatype you want to have allocated for this Drawable
    bool                                      interleavedAttributes;// only set this to true if you plan on reallocating vertex attributes, since interleaving data will likely have negative performance impact (wide-instruction optimizations more difficult, kill cache hits when operating on only certain attributes)
} Korl_Gfx_CreateInfoRuntimeDrawable;
korl_internal Korl_Gfx_Drawable korl_gfx_drawableLines(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 lineCount);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableLineStrip(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 vertexCount);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTriangles(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 triangleCount);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTrianglesIndexed(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 triangleCount, u32 vertexCount);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTriangleFan(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 vertexCount);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTriangleStrip(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 vertexCount);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableRectangle(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableCircle(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableUtf8(Korl_Gfx_Drawable_Runtime_Type type, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, Korl_Resource_Font_TextMetrics* o_textMetrics);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableUtf16(Korl_Gfx_Drawable_Runtime_Type type, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, Korl_Resource_Font_TextMetrics* o_textMetrics);
korl_internal Korl_Gfx_Drawable korl_gfx_drawableAxisNormalLines(Korl_Gfx_Drawable_Runtime_Type type);
korl_internal Korl_Gfx_Drawable korl_gfx_mesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName);
korl_internal Korl_Gfx_Drawable korl_gfx_meshIndexed(Korl_Resource_Handle resourceHandleScene3d, u32 meshIndex);
korl_internal void              korl_gfx_drawable_addInterleavedVertices(Korl_Gfx_Drawable*const context, u32 vertexCount);
korl_internal u16*              korl_gfx_drawable_indexU16(const Korl_Gfx_Drawable*const context, u32 indicesIndex);
korl_internal u32*              korl_gfx_drawable_indexU32(const Korl_Gfx_Drawable*const context, u32 indicesIndex);
korl_internal u32*              korl_gfx_drawable_attributeU32(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex);
korl_internal Korl_Math_V2f32*  korl_gfx_drawable_attributeV2f32(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex);
korl_internal Korl_Math_V3f32*  korl_gfx_drawable_attributeV3f32(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex);
korl_internal Korl_Math_V4u8*   korl_gfx_drawable_attributeV4u8(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex);
korl_internal Korl_Gfx_Drawable korl_gfx_drawSphere(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, f32 radius, u32 latitudeSegments, u32 longitudeSegments, const Korl_Gfx_Material* material);
korl_internal void              korl_gfx_drawRectangle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* materialInner, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_innerColors, Korl_Math_V2f32** o_innerUvs);
korl_internal void              korl_gfx_drawRectangle3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* materialInner, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_innerColors, Korl_Math_V2f32** o_innerUvs);
korl_internal void              korl_gfx_drawCircle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices, f32 outlineThickness, const Korl_Gfx_Material* materialInner, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_innerColors);
korl_internal void              korl_gfx_drawLines2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void              korl_gfx_drawLines3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void              korl_gfx_drawLineStrip2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void              korl_gfx_drawLineStrip3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void              korl_gfx_drawTriangles2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs);
korl_internal void              korl_gfx_drawTriangles3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs);
korl_internal void              korl_gfx_drawTrianglesIndexed3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 triangleCount, u32 vertexCount, const Korl_Gfx_Material* material, u32** o_indices, Korl_Math_V3f32** o_positions, Korl_Math_V3f32** o_normals, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs);
korl_internal void              korl_gfx_drawTriangleFan2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void              korl_gfx_drawTriangleFan3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void              korl_gfx_drawUtf82d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics);
korl_internal void              korl_gfx_drawUtf83d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics);
korl_internal void              korl_gfx_drawUtf162d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics);
korl_internal void              korl_gfx_drawUtf163d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics);
korl_internal void              korl_gfx_drawAabb3(Korl_Math_Aabb3f32 aabb, const Korl_Gfx_Material* material);
korl_internal void              korl_gfx_drawMesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material *materials, u8 materialsSize, Korl_Resource_Scene3d_Skin* skin);
korl_internal void              korl_gfx_drawMeshIndexed(Korl_Resource_Handle resourceHandleScene3d, u32 meshIndex, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material *materials, u8 materialsSize, Korl_Resource_Scene3d_Skin* skin);
korl_internal void              korl_gfx_drawAxisNormalLines(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale);
korl_internal void              korl_gfx_drawTriangleMesh(Korl_Math_TriangleMesh* context, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, const Korl_Gfx_Material *material);
korl_internal void              korl_gfx_setRectangleUvAabb(Korl_Math_V2f32* uvs, const Korl_Math_Aabb2f32 aabb);
