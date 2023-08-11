#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-gfx.h"
#include "korl-memory.h"
#include "utility/korl-utility-math.h"
#include "utility/korl-utility-memory.h"
korl_internal void korl_gfx_initialize(void);
korl_internal void korl_gfx_initializePostRendererLogicalDevice(void);
korl_internal void korl_gfx_update(Korl_Math_V2u32 surfaceSize, f32 deltaSeconds);
korl_internal void korl_gfx_flushGlyphPages(void);
typedef struct Korl_Gfx_Text
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Gfx_VertexStagingMeta  vertexStagingMeta;
    Korl_Resource_Handle        resourceHandleBufferText;// used to store the vertex buffer which contains all of the _Korl_Gfx_FontGlyphInstance data for all the lines contained in this Text object; each line will access an offset into this vertex buffer determined by the sum of all visible characters of all lines prior to it in the stbDaLines list
    struct _Korl_Gfx_Text_Line* stbDaLines;// the user can obtain the line count by calling arrlenu(text->stbDaLines)
    f32                         textPixelHeight;
    Korl_Resource_Handle        resourceHandleFont;
    Korl_Math_V3f32             modelTranslate;// the model-space origin of a Gfx_Text is the upper-left corner of the model-space AABB
    Korl_Math_Quaternion        modelRotate;
    Korl_Math_V3f32             modelScale;
    Korl_Math_Aabb2f32          _modelAabb;// not valid until fifo add/remove APIs have been called
    u32                         totalVisibleGlyphs;// redundant value; acceleration for operations on resourceHandleBufferText
} Korl_Gfx_Text;
korl_internal Korl_Gfx_Text*     korl_gfx_text_create(Korl_Memory_AllocatorHandle allocator, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight);
korl_internal void               korl_gfx_text_destroy(Korl_Gfx_Text* context);
korl_internal void               korl_gfx_text_collectDefragmentPointers(Korl_Gfx_Text* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent);
korl_internal void               korl_gfx_text_fifoAdd(Korl_Gfx_Text* context, acu16 utf16Text, Korl_Memory_AllocatorHandle stackAllocator, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData);
korl_internal void               korl_gfx_text_fifoRemove(Korl_Gfx_Text* context, u$ lineCount);
korl_internal void               korl_gfx_text_draw(Korl_Gfx_Text* context, Korl_Math_Aabb2f32 visibleRegion);
korl_internal Korl_Math_Aabb2f32 korl_gfx_text_getModelAabb(const Korl_Gfx_Text* context);
korl_internal void korl_gfx_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32  korl_gfx_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_gfx_memoryStateRead(const u8* memoryState);
