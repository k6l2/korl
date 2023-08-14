#include "korl-gfx.h"
#include "korl-log.h"
#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-vulkan.h"
#include "korl-time.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
#include "korl-resource.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-utility-gfx.h"
#include "utility/korl-utility-resource.h"
#include "korl-resource-gfx-buffer.h"
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_gfx_context.stringPool))
typedef struct _Korl_Gfx_Context
{
    /** used to store persistent data, such as Font asset glyph cache/database */
    Korl_Memory_AllocatorHandle allocatorHandle;
    u8                          nextResourceSalt;
    Korl_StringPool*            stringPool;// used for Resource database strings; Korl_StringPool structs _must_ be unmanaged allocations (allocations with an unchanging memory address), because we're likely going to have a shit-ton of Strings which point to the pool address for convenience
    Korl_Math_V2u32             surfaceSize;// updated at the top of each frame, ideally before anything has a chance to use korl-gfx
    Korl_Gfx_Camera             currentCameraState;
    f32                         seconds;// passed to the renderer as UBO data to allow shader animations; passed when a Camera is used
    Korl_Resource_Handle        blankTexture;// a 1x1 texture whose color channels are fully-saturated; can be used as a "default" material map texture
    Korl_Resource_Handle        resourceShaderKorlVertex2d;
    Korl_Resource_Handle        resourceShaderKorlVertex2dColor;
    Korl_Resource_Handle        resourceShaderKorlVertex2dUv;
    Korl_Resource_Handle        resourceShaderKorlVertex3d;
    Korl_Resource_Handle        resourceShaderKorlVertex3dColor;
    Korl_Resource_Handle        resourceShaderKorlVertex3dUv;
    Korl_Resource_Handle        resourceShaderKorlVertexLit;
    Korl_Resource_Handle        resourceShaderKorlVertexText;
    Korl_Resource_Handle        resourceShaderKorlFragmentColor;
    Korl_Resource_Handle        resourceShaderKorlFragmentColorTexture;
    Korl_Resource_Handle        resourceShaderKorlFragmentLit;
    KORL_MEMORY_POOL_DECLARE(Korl_Gfx_Light, pendingLights, KORL_VULKAN_MAX_LIGHTS);// after being added to this pool, lights are flushed to the renderer's draw state upon the next call to `korl_gfx_draw`
} _Korl_Gfx_Context;
korl_global_variable _Korl_Gfx_Context* _korl_gfx_context;
korl_internal void korl_gfx_initialize(void)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-gfx", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_gfx_context = korl_allocate(allocator, sizeof(*_korl_gfx_context));
    _korl_gfx_context->allocatorHandle = allocator;
    _korl_gfx_context->stringPool      = korl_allocate(allocator, sizeof(*_korl_gfx_context->stringPool));// allocate this ASAP to reduce fragmentation, since this struct _must_ remain UNMANAGED
    *_korl_gfx_context->stringPool = korl_stringPool_create(_korl_gfx_context->allocatorHandle);
}
korl_internal void korl_gfx_initializePostRendererLogicalDevice(void)
{
    /* why don't we do this in korl_gfx_initialize?  It's because when 
        korl_gfx_initialize happens, korl-vulkan is not yet fully initialized!  
        we can't actually configure graphics device assets when the graphics 
        device has not even been created */
    KORL_ZERO_STACK(Korl_Resource_Texture_CreateInfo, createInfoBlankTexture);
    createInfoBlankTexture.formatImage = KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_UNORM;
    createInfoBlankTexture.size        = KORL_MATH_V2U32_ONE;
    _korl_gfx_context->blankTexture = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfoBlankTexture);
    const Korl_Gfx_Color4u8 blankTextureColor = KORL_COLOR4U8_WHITE;
    korl_resource_update(_korl_gfx_context->blankTexture, &blankTextureColor, sizeof(blankTextureColor), 0);
    /* initiate loading of all built-in shaders */
    _korl_gfx_context->resourceShaderKorlVertex2d             = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-2d.vert.spv"           ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex2dColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-2d-color.vert.spv"     ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex2dUv           = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-2d-uv.vert.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex3d             = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-3d.vert.spv"           ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex3dColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-3d-color.vert.spv"     ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex3dUv           = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-3d-uv.vert.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertexLit            = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.vert.spv"          ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertexText           = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-text.vert.spv"         ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlFragmentColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-color.frag.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlFragmentColorTexture = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-color-texture.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlFragmentLit          = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.frag.spv"          ), KORL_ASSETCACHE_GET_FLAG_LAZY);
}
korl_internal void korl_gfx_update(Korl_Math_V2u32 surfaceSize, f32 deltaSeconds)
{
    _korl_gfx_context->surfaceSize = surfaceSize;
    _korl_gfx_context->seconds    += deltaSeconds;
    KORL_MEMORY_POOL_EMPTY(_korl_gfx_context->pendingLights);
}
typedef struct _Korl_Gfx_Text_Line
{
    u32                visibleCharacters;// used to determine how many glyph instances are in the draw call
    f32                aabbSizeX;// no need to store the sizeY, since all Text_Lines should be the same Font lineDeltaY calculation
    Korl_Math_V4f32    color;
} _Korl_Gfx_Text_Line;
typedef struct _Korl_Gfx_Text_GlyphInstance
{
    u32             meshIndex;
    Korl_Math_V2f32 position;
} _Korl_Gfx_Text_GlyphInstance;
korl_internal Korl_Gfx_Text* korl_gfx_text_create(Korl_Memory_AllocatorHandle allocator, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight)
{
    KORL_ZERO_STACK(Korl_Resource_GfxBuffer_CreateInfo, createInfoBufferText);
    createInfoBufferText.bytes      = 1024;// some arbitrary non-zero value; likely not important to tune this, but we'll see
    createInfoBufferText.usageFlags =   KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_VERTEX
                                      | KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_INDEX;
    const u$ bytesRequired           = sizeof(Korl_Gfx_Text);
    Korl_Gfx_Text*const result       = korl_allocate(allocator, bytesRequired);
    result->allocator                = allocator;
    result->textPixelHeight          = textPixelHeight;
    result->resourceHandleFont       = resourceHandleFont;
    result->transform                = korl_math_transform3d_identity();
    result->resourceHandleBufferText = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER), &createInfoBufferText);
    // defer adding the vertex indices until _just_ before we draw the text, as this will allow us to shift the entire buffer to effectively delete lines of text @korl-gfx-text-defer-index-buffer
    result->vertexStagingMeta.indexCount = korl_arraySize(KORL_GFX_TRI_QUAD_INDICES);
    result->vertexStagingMeta.indexType  = KORL_GFX_VERTEX_INDEX_TYPE_U16; korl_assert(sizeof(*KORL_GFX_TRI_QUAD_INDICES) == sizeof(u16));
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = offsetof(_Korl_Gfx_Text_GlyphInstance, position);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(_Korl_Gfx_Text_GlyphInstance);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = VK_VERTEX_INPUT_RATE_INSTANCE;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteOffsetBuffer = offsetof(_Korl_Gfx_Text_GlyphInstance, meshIndex);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteStride       = sizeof(_Korl_Gfx_Text_GlyphInstance);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].inputRate        = VK_VERTEX_INPUT_RATE_INSTANCE;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].vectorSize       = 1;
    mcarrsetcap(KORL_STB_DS_MC_CAST(result->allocator), result->stbDaLines, 64);
    return result;
}
korl_internal void korl_gfx_text_destroy(Korl_Gfx_Text* context)
{
    korl_resource_destroy(context->resourceHandleBufferText);
    mcarrfree(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaLines);
    korl_free(context->allocator, context);
}
korl_internal void korl_gfx_text_collectDefragmentPointers(Korl_Gfx_Text* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent)
{
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context->stbDaLines, parent);
}
korl_internal void _korl_gfx_text_fifoAdd_flush(Korl_Gfx_Text* context, acu16 utf16Text, const Korl_String_CodepointIteratorUtf16* utf16It, _Korl_Gfx_Text_Line* currentLine, i$* io_substringStartIndex)
{
    if(currentLine && *io_substringStartIndex >= 0)
    {
        /* if we failed the codepointTest while building a Line, we need 
            to flush the current substring into the GlyphInstance vertex 
            buffer, and update our line metrics accordingly */
        const acu16 utf16Substring = {.size = utf16It->_currentRawUtf16 - utf16Text.data - *io_substringStartIndex
                                     ,.data = utf16Text.data + *io_substringStartIndex};
        if(utf16Substring.size)// no need to flush anything if there's nothing to flush
        {
            const Korl_Resource_Font_TextMetrics substringMetrics = korl_resource_font_getUtf16Metrics(context->resourceHandleFont, context->textPixelHeight, utf16Substring);
            /* reallocate our GlyphInstance vertex buffer as needed */
            u$ textBufferBytes = korl_resource_getByteSize(context->resourceHandleBufferText);
            const u$ textBufferBytesRequired = (context->totalVisibleGlyphs + substringMetrics.visibleGlyphCount) * sizeof(_Korl_Gfx_Text_GlyphInstance);
            if(textBufferBytes < textBufferBytesRequired)
                korl_resource_resize(context->resourceHandleBufferText, KORL_MATH_MAX(textBufferBytesRequired, 2 * textBufferBytes));
            /* obtain & append the new Line into our GlyphInstance buffer */
            u$ bytesRequested_bytesAvailable = substringMetrics.visibleGlyphCount * sizeof(_Korl_Gfx_Text_GlyphInstance);
            _Korl_Gfx_Text_GlyphInstance*const glyphInstanceUpdateBuffer = korl_resource_getUpdateBuffer(context->resourceHandleBufferText, context->totalVisibleGlyphs * sizeof(_Korl_Gfx_Text_GlyphInstance), &bytesRequested_bytesAvailable);
            korl_resource_font_generateUtf16(context->resourceHandleFont, context->textPixelHeight, utf16Substring, (Korl_Math_V2f32){substringMetrics.aabbSize.x, 0}
                                            ,&glyphInstanceUpdateBuffer[0].position , sizeof(_Korl_Gfx_Text_GlyphInstance)
                                            ,&glyphInstanceUpdateBuffer[0].meshIndex, sizeof(_Korl_Gfx_Text_GlyphInstance));
            /* update Line metrics (glyph count, AABB) */
            currentLine->visibleCharacters += substringMetrics.visibleGlyphCount;
            currentLine->aabbSizeX         += substringMetrics.aabbSize.x;
        }
    }
    *io_substringStartIndex = -1;
}
korl_internal void korl_gfx_text_fifoAdd(Korl_Gfx_Text* context, acu16 utf16Text, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData)
{
    /* iterate over utf16Text until we hit a line break; upon reaching a line break, we can add another _Korl_Gfx_Text_Line to the context */
    _Korl_Gfx_Text_Line*               currentLine         = NULL;// the current working Line of text; we will accumulate substrings of codepoints which pass codepointTest (if it's provided) into this Line, until we reach a line-break ('\n') character
    Korl_Math_V4f32                    currentLineColor    = korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE);// default all line colors to white
    i$                                 substringStartIndex = -1;// determines the next range of text to generate glyphs into `currentLine`; all text within [substringStartIndex, utf16It._currentRawUtf16) _must_ pass codepointTest if provided; values < 0 => no start index found yet
    Korl_String_CodepointIteratorUtf16 utf16It             = korl_string_codepointIteratorUtf16_initialize(utf16Text.data, utf16Text.size);
    for(;!korl_string_codepointIteratorUtf16_done(&utf16It)
        ; korl_string_codepointIteratorUtf16_next(&utf16It))
    {
        if(    codepointTest 
           && !codepointTest(codepointTestUserData
                            ,utf16It._codepoint, utf16It._codepointSize
                            ,KORL_C_CAST(const u8*, utf16It._currentRawUtf16)
                            ,sizeof(*utf16It._currentRawUtf16)
                            ,&currentLineColor))
        {
            _korl_gfx_text_fifoAdd_flush(context, utf16Text, &utf16It, currentLine, &substringStartIndex);
            continue;
        }
        if(utf16It._codepoint == '\n')
        {
            _korl_gfx_text_fifoAdd_flush(context, utf16Text, &utf16It, currentLine, &substringStartIndex);
            if(currentLine)
            {
                /* update context metrics (glyph count, AABB)*/
                context->totalVisibleGlyphs += currentLine->visibleCharacters;
                KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabb.max.x, currentLine->aabbSizeX);
                /* we're done with this Line now; nullify currentLine reference */
                currentLine = NULL;
            }
        }
        else
        {
            if(substringStartIndex < 0)
                substringStartIndex = utf16It._currentRawUtf16 - utf16Text.data;
            if(!currentLine)
            {
                mcarrpush(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaLines, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Gfx_Text_Line));
                currentLine = &arrlast(context->stbDaLines);
                currentLine->color = currentLineColor;
            }
        }
    }
    /* if utf16Text did not end in a new line, we still need to flush the remaining text */
    _korl_gfx_text_fifoAdd_flush(context, utf16Text, &utf16It, currentLine, &substringStartIndex);
    if(currentLine)
    {
        /* update context metrics (glyph count, AABB)*/
        context->totalVisibleGlyphs += currentLine->visibleCharacters;
        KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabb.max.x, currentLine->aabbSizeX);
    }
    /* we can easily calculate the height of the Text object's current AABB 
        using the font's cached metrics */
    const Korl_Resource_Font_Metrics fontMetrics = korl_resource_font_getMetrics(context->resourceHandleFont, context->textPixelHeight);
    const f32 sizeToSupportedSizeRatio = context->textPixelHeight / fontMetrics.nearestSupportedPixelHeight;// korl-resource-text will only bake glyphs at discrete pixel heights (for complexity/memory reasons); it is up to us to scale the final text mesh by the appropriate ratio to actually draw text at `textPixelHeight`
    //@TODO: use sizeToSupportedSizeRatio?
    const f32 lineDeltaY = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    context->_modelAabb.min.y = arrlenu(context->stbDaLines) * -lineDeltaY;
    #if 0//@TODO: refactor
    /* initialize scratch space for storing glyph instance data of the current 
        working text line */
    const u$                           currentLineBufferBytes = utf16Text.size * sizeof(_Korl_Gfx_Text_GlyphInstance);
    _Korl_Gfx_Text_GlyphInstance*const currentLineBuffer      = korl_allocate(stackAllocator, currentLineBufferBytes);
    /* iterate over each character of utf16Text & build all the text lines */
    _Korl_Gfx_Text_Line* currentLine        = NULL;
    Korl_Math_V2f32      textBaselineCursor = (Korl_Math_V2f32){0.f, 0.f};
    int                  glyphIndexPrevious = -1;// used to calculate kerning advance between the previous glyph and the current glyph
    Korl_Math_V4f32      currentLineColor   = KORL_MATH_V4F32_ONE;// default all line colors to white
    for(Korl_String_CodepointIteratorUtf16 utf16It = korl_string_codepointIteratorUtf16_initialize(utf16Text.data, utf16Text.size)
       ;!korl_string_codepointIteratorUtf16_done(&utf16It)
       ; korl_string_codepointIteratorUtf16_next(&utf16It))
    {
        if(    codepointTest 
           && !codepointTest(codepointTestUserData
                            ,utf16It._codepoint, utf16It._codepointSize
                            ,KORL_C_CAST(const u8*, utf16It._currentRawUtf16)
                            ,sizeof(*utf16It._currentRawUtf16)
                            ,&currentLineColor))
            continue;
        u32 glyphCodepoint     = utf16It._codepoint;
        f32 advanceXMultiplier = 1;
        if(utf16It._codepoint == '\t')
        {
            glyphCodepoint     = ' ';
            advanceXMultiplier = 4;
        }
        const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, glyphCodepoint);
        if(textBaselineCursor.x > 0.f)
        {
            const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo
                                                             ,glyphIndexPrevious
                                                             ,bakedGlyph->glyphIndex);
            glyphIndexPrevious = bakedGlyph->glyphIndex;
            textBaselineCursor.x += fontCache->fontScale*kernAdvance;
        }
        const f32 x0 = textBaselineCursor.x + bakedGlyph->bbox.offsetX;
        const f32 y0 = textBaselineCursor.y + bakedGlyph->bbox.offsetY;
        const f32 x1 = x0 + (bakedGlyph->bbox.x1 - bakedGlyph->bbox.x0);
        const f32 y1 = y0 + (bakedGlyph->bbox.y1 - bakedGlyph->bbox.y0);
        const Korl_Math_V2f32 glyphPosition = textBaselineCursor;
        textBaselineCursor.x += advanceXMultiplier*bakedGlyph->advanceX;
        if(utf16It._codepoint == '\n')
        {
            textBaselineCursor.x  = 0.f;
            // textBaselineCursor.y -= lineDeltaY;// no need to do this; each line has an implicit Y size, and when we draw we will move the line's model position appropriately
            glyphIndexPrevious = -1;
            if(currentLine)
            {
                /* if we had a current working text line, we need to flush the text 
                    instance data we've accumulated so far into a vertex buffer 
                    device asset; instead of creating a separate buffer resource 
                    for each line, we are just going to accumulate a giant 
                    buffer used by the entire Gfx_Text object */
                const u$ bufferBytesRequiredMin = (context->totalVisibleGlyphs + currentLine->visibleCharacters) * sizeof(*currentLineBuffer);
                const u$ currentBufferBytes     = korl_resource_getByteSize(context->resourceHandleBufferText);
                if(currentBufferBytes < bufferBytesRequiredMin)
                {
                    const u$ bufferBytesNew = KORL_MATH_MAX(bufferBytesRequiredMin, 2*currentBufferBytes);
                    korl_resource_resize(context->resourceHandleBufferText, bufferBytesNew);
                }
                korl_resource_update(context->resourceHandleBufferText, currentLineBuffer, currentLine->visibleCharacters * sizeof(*currentLineBuffer), context->totalVisibleGlyphs * sizeof(*currentLineBuffer));
                /* update the Text object's model AABB with the new line graphics we just added */
                KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabb.max.x, currentLine->modelAabb.max.x);
                /* update other Text object metrics */
                context->totalVisibleGlyphs += currentLine->visibleCharacters;
            }
            currentLine      = NULL;
            currentLineColor = KORL_MATH_V4F32_ONE;// default next line color to white
            continue;
        }
        if(bakedGlyph->isEmpty)
            continue;
        /* at this point, we know that this is a valid visible character, which 
            must be accumulated into a text line; if we don't have a current 
            working text line at this point, we need to make one */
        if(!currentLine)
        {
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaLines, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Gfx_Text_Line));
            currentLine = &arrlast(context->stbDaLines);
            currentLine->color = currentLineColor;
        }
        currentLineBuffer[currentLine->visibleCharacters].position  = glyphPosition;
        currentLineBuffer[currentLine->visibleCharacters].meshIndex = bakedGlyph->bakeOrder;
        currentLine->modelAabb.min = korl_math_v2f32_min(currentLine->modelAabb.min, (Korl_Math_V2f32){x0, y0});
        currentLine->modelAabb.max = korl_math_v2f32_max(currentLine->modelAabb.max, (Korl_Math_V2f32){x1, y1});
        currentLine->visibleCharacters++;
    }
    /* we can easily calculate the height of the Text object's current AABB 
        using the font's cached metrics */
    const f32 lineDeltaY = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    context->_modelAabb.min.y = arrlenu(context->stbDaLines) * -lineDeltaY;
    /* clean up */
    korl_free(stackAllocator, currentLineBuffer);
    #endif
}
korl_internal void korl_gfx_text_fifoRemove(Korl_Gfx_Text* context, u$ lineCount)
{
    const u$ linesToRemove = KORL_MATH_MIN(lineCount, arrlenu(context->stbDaLines));
    context->_modelAabb.max.x = 0;
    u$ glyphsToRemove = 0;
    for(u$ l = 0; l < arrlenu(context->stbDaLines); l++)
    {
        if(l < linesToRemove)
        {
            glyphsToRemove += context->stbDaLines[l].visibleCharacters;
            continue;
        }
        const _Korl_Gfx_Text_Line*const line = &(context->stbDaLines[l]);
        KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabb.max.x, line->aabbSizeX);
    }
    arrdeln(context->stbDaLines, 0, linesToRemove);
    context->totalVisibleGlyphs -= korl_checkCast_u$_to_u32(glyphsToRemove);
    korl_resource_shift(context->resourceHandleBufferText, -1/*shift to the left; remove the glyphs instances at the beginning of the buffer*/ * glyphsToRemove * sizeof(_Korl_Gfx_Text_GlyphInstance));
    const Korl_Resource_Font_Metrics fontMetrics = korl_resource_font_getMetrics(context->resourceHandleFont, context->textPixelHeight);
    const f32 sizeToSupportedSizeRatio = context->textPixelHeight / fontMetrics.nearestSupportedPixelHeight;// korl-resource-text will only bake glyphs at discrete pixel heights (for complexity/memory reasons); it is up to us to scale the final text mesh by the appropriate ratio to actually draw text at `textPixelHeight`
    //@TODO: use sizeToSupportedSizeRatio?
    const f32 lineDeltaY = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    context->_modelAabb.min.y = arrlenu(context->stbDaLines) * -lineDeltaY;
}
korl_internal void korl_gfx_text_draw(Korl_Gfx_Text* context, Korl_Math_Aabb2f32 visibleRegion)
{
    const Korl_Resource_Font_Metrics fontMetrics = korl_resource_font_getMetrics(context->resourceHandleFont, context->textPixelHeight);
    const f32 sizeToSupportedSizeRatio = context->textPixelHeight / fontMetrics.nearestSupportedPixelHeight;// korl-resource-text will only bake glyphs at discrete pixel heights (for complexity/memory reasons); it is up to us to scale the final text mesh by the appropriate ratio to actually draw text at `textPixelHeight`
    //@TODO: use sizeToSupportedSizeRatio?
    const f32 lineDeltaY = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    /* we can now send the vertex index data to the buffer, which will be used 
        for all subsequent text line draw calls @korl-gfx-text-defer-index-buffer */
    const u$ glyphInstanceBufferSize = context->totalVisibleGlyphs * sizeof(_Korl_Gfx_Text_GlyphInstance);
    {
        const u32 indexBufferBytes   = sizeof(KORL_GFX_TRI_QUAD_INDICES);
        const u$  currentBufferBytes = korl_resource_getByteSize(context->resourceHandleBufferText);
        if(currentBufferBytes < glyphInstanceBufferSize + indexBufferBytes)
            korl_resource_resize(context->resourceHandleBufferText, glyphInstanceBufferSize + indexBufferBytes);
        korl_resource_update(context->resourceHandleBufferText, KORL_GFX_TRI_QUAD_INDICES, sizeof(KORL_GFX_TRI_QUAD_INDICES), glyphInstanceBufferSize);
        // context->vertexStagingMeta.indexByteOffsetBuffer = glyphInstanceBufferSize;//can't do this yet, since this is relative to the buffer offset byte we choose per line!
    }
    /* configure the renderer draw state */
    const Korl_Resource_Font_Resources fontResources = korl_resource_font_getResources(context->resourceHandleFont, context->textPixelHeight);
    Korl_Gfx_Material material = korl_gfx_material_defaultUnlit(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_BLEND, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    material.maps.resourceHandleTextureBase       = fontResources.resourceHandleTexture;
    //@TODO: it seems that (based on what I've seen so far) the only thing preventing us from being able to move Korl_Gfx_Text into korl-utility-gfx is access to the built-in KORL text shader resource; do we even need to store this in korl-gfx?? It seems we should be able to just obtain this hand for each Korl_Gfx_Text object on construction
    material.shaders.resourceHandleShaderVertex   = _korl_gfx_context->resourceShaderKorlVertexText;
    material.shaders.resourceHandleShaderFragment = _korl_gfx_context->resourceShaderKorlFragmentColorTexture;
    KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
    storageBuffers.resourceHandleVertex = fontResources.resourceHandleSsboGlyphMeshVertices;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.storageBuffers = &storageBuffers;
    korl_vulkan_setDrawState(&drawState);
    /* now we can iterate over each text line and conditionally draw them using line-specific draw state */
    KORL_ZERO_STACK(Korl_Gfx_DrawState_PushConstantData, pushConstantData);
    Korl_Math_M4f32*const pushConstantModelM4f32 = KORL_C_CAST(Korl_Math_M4f32*, pushConstantData.vertex);
    Korl_Math_V3f32 modelTranslation = context->transform._position;
    modelTranslation.y -= fontMetrics.ascent;// start the text such that the translation XY position defines the location _directly_ above _all_ the text
    u$ currentVisibleGlyphOffset = 0;// used to determine the byte (transform required) offset into the Text object's text buffer resource
    const _Korl_Gfx_Text_Line*const linesEnd = context->stbDaLines + arrlen(context->stbDaLines);
    for(const _Korl_Gfx_Text_Line* line = context->stbDaLines; line < linesEnd; line++)
    {
        if(modelTranslation.y < visibleRegion.min.y - fontMetrics.ascent)
            break;
        if(modelTranslation.y <= visibleRegion.max.y + korl_math_f32_positive(fontMetrics.decent))
        {
            material.fragmentShaderUniform.factorColorBase = line->color;
            *pushConstantModelM4f32                        = korl_math_makeM4f32_rotateScaleTranslate(context->transform._versor, context->transform._scale, modelTranslation);
            KORL_ZERO_STACK(Korl_Gfx_DrawState, drawStateLine);
            drawStateLine.material         = &material;
            drawStateLine.pushConstantData = &pushConstantData;
            korl_vulkan_setDrawState(&drawStateLine);
            const u$ textLineByteOffset = currentVisibleGlyphOffset * sizeof(_Korl_Gfx_Text_GlyphInstance);
            context->vertexStagingMeta.indexByteOffsetBuffer = korl_checkCast_u$_to_u32(glyphInstanceBufferSize - textLineByteOffset);
            context->vertexStagingMeta.instanceCount         = line->visibleCharacters;
            korl_vulkan_drawVertexBuffer(korl_resource_gfxBuffer_getVulkanDeviceMemoryAllocationHandle(context->resourceHandleBufferText), textLineByteOffset, &context->vertexStagingMeta);
        }
        modelTranslation.y        -= lineDeltaY;
        currentVisibleGlyphOffset += line->visibleCharacters;
    }
}
korl_internal Korl_Math_Aabb2f32 korl_gfx_text_getModelAabb(const Korl_Gfx_Text* context)
{
    return context->_modelAabb;
}
korl_internal KORL_FUNCTION_korl_gfx_useCamera(korl_gfx_useCamera)
{
    _Korl_Gfx_Context*const context = _korl_gfx_context;
    korl_time_probeStart(useCamera);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Scissor, scissor);
    switch(camera._scissorType)
    {
    case KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO:{
        korl_assert(camera._viewportScissorPosition.x >= 0 && camera._viewportScissorPosition.x <= 1);
        korl_assert(camera._viewportScissorPosition.y >= 0 && camera._viewportScissorPosition.y <= 1);
        korl_assert(camera._viewportScissorSize.x >= 0 && camera._viewportScissorSize.x <= 1);
        korl_assert(camera._viewportScissorSize.y >= 0 && camera._viewportScissorSize.y <= 1);
        scissor.x      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.x * context->surfaceSize.x);
        scissor.y      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.y * context->surfaceSize.y);
        scissor.width  = korl_math_round_f32_to_u32(camera._viewportScissorSize.x * context->surfaceSize.x);
        scissor.height = korl_math_round_f32_to_u32(camera._viewportScissorSize.y * context->surfaceSize.y);
        break;}
    case KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE:{
        korl_assert(camera._viewportScissorPosition.x >= 0);
        korl_assert(camera._viewportScissorPosition.y >= 0);
        scissor.x      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.x);
        scissor.y      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.y);
        scissor.width  = korl_math_round_f32_to_u32(camera._viewportScissorSize.x);
        scissor.height = korl_math_round_f32_to_u32(camera._viewportScissorSize.y);
        break;}
    }
    KORL_ZERO_STACK(Korl_Gfx_DrawState_SceneProperties, sceneProperties);
    sceneProperties.view = korl_gfx_camera_view(&camera);
    switch(camera.type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        const f32 viewportWidthOverHeight = context->surfaceSize.y == 0 
                                            ? 1.f 
                                            : KORL_C_CAST(f32, context->surfaceSize.x) / KORL_C_CAST(f32, context->surfaceSize.y);
        sceneProperties.projection = korl_math_m4f32_projectionFov(camera.subCamera.perspective.fovVerticalDegrees
                                                                  ,viewportWidthOverHeight
                                                                  ,camera.subCamera.perspective.clipNear
                                                                  ,camera.subCamera.perspective.clipFar);
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        const f32 left   = 0.f - camera.subCamera.orthographic.originAnchor.x * KORL_C_CAST(f32, context->surfaceSize.x);
        const f32 bottom = 0.f - camera.subCamera.orthographic.originAnchor.y * KORL_C_CAST(f32, context->surfaceSize.y);
        const f32 right  = KORL_C_CAST(f32, context->surfaceSize.x) - camera.subCamera.orthographic.originAnchor.x * KORL_C_CAST(f32, context->surfaceSize.x);
        const f32 top    = KORL_C_CAST(f32, context->surfaceSize.y) - camera.subCamera.orthographic.originAnchor.y * KORL_C_CAST(f32, context->surfaceSize.y);
        const f32 far    = -camera.subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        sceneProperties.projection = korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = context->surfaceSize.y == 0 
                                            ? 1.f 
                                            : KORL_C_CAST(f32, context->surfaceSize.x) / KORL_C_CAST(f32, context->surfaceSize.y);
        /* w / fixedHeight == windowAspectRatio */
        const f32 width  = camera.subCamera.orthographic.fixedHeight * viewportWidthOverHeight;
        const f32 left   = 0.f - camera.subCamera.orthographic.originAnchor.x * width;
        const f32 bottom = 0.f - camera.subCamera.orthographic.originAnchor.y * camera.subCamera.orthographic.fixedHeight;
        const f32 right  = width       - camera.subCamera.orthographic.originAnchor.x * width;
        const f32 top    = camera.subCamera.orthographic.fixedHeight - camera.subCamera.orthographic.originAnchor.y * camera.subCamera.orthographic.fixedHeight;
        const f32 far    = -camera.subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        sceneProperties.projection = korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);
        break;}
    }
    sceneProperties.seconds = context->seconds;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.scissor         = &scissor;
    drawState.sceneProperties = &sceneProperties;
    korl_vulkan_setDrawState(&drawState);
    context->currentCameraState = camera;
    korl_time_probeStop(useCamera);
}
korl_internal KORL_FUNCTION_korl_gfx_camera_getCurrent(korl_gfx_camera_getCurrent)
{
    return _korl_gfx_context->currentCameraState;
}
korl_internal KORL_FUNCTION_korl_gfx_cameraOrthoGetSize(korl_gfx_cameraOrthoGetSize)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        return (Korl_Math_V2f32){KORL_C_CAST(f32, gfxContext->surfaceSize.x)
                                ,KORL_C_CAST(f32, gfxContext->surfaceSize.y)};}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = gfxContext->surfaceSize.y == 0 
            ? 1.f 
            :   KORL_C_CAST(f32, gfxContext->surfaceSize.x) 
              / KORL_C_CAST(f32, gfxContext->surfaceSize.y);
        return (Korl_Math_V2f32){context->subCamera.orthographic.fixedHeight*viewportWidthOverHeight// w / fixedHeight == windowAspectRatio
                                ,context->subCamera.orthographic.fixedHeight};}
    default:{
        korl_log(ERROR, "invalid camera type: %i", context->type);
        return (Korl_Math_V2f32){korl_math_f32_nan(), korl_math_f32_nan()};}
    }
}
korl_internal KORL_FUNCTION_korl_gfx_camera_windowToWorld(korl_gfx_camera_windowToWorld)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    Korl_Gfx_ResultRay3d result = {.position  = {korl_math_f32_nan(), korl_math_f32_nan(), korl_math_f32_nan()}
                                  ,.direction = {korl_math_f32_nan(), korl_math_f32_nan(), korl_math_f32_nan()}};
    //KORL-PERFORMANCE-000-000-041: gfx: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view                  = korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection            = korl_gfx_camera_projection(context);
    const Korl_Math_M4f32 viewProjection        = korl_math_m4f32_multiply(&projection, &view);
    const Korl_Math_M4f32 viewProjectionInverse = korl_math_m4f32_invert(&viewProjection);
    if(korl_math_f32_isNan(viewProjectionInverse.r0c0))
    {
        korl_log(WARNING, "failed to invert view-projection matrix");
        return result;
    }
    const Korl_Math_V2f32 v2f32WindowPos = {KORL_C_CAST(f32, windowPosition.x)
                                           ,KORL_C_CAST(f32, windowPosition.y)};
    /* viewport-space => normalized-device-space */
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_V2f32 eyeRayNds = {  2.f * v2f32WindowPos.x / KORL_C_CAST(f32, gfxContext->surfaceSize.x) - 1.f
                                      , -2.f * v2f32WindowPos.y / KORL_C_CAST(f32, gfxContext->surfaceSize.y) + 1.f };
    /* normalized-device-space => homogeneous-clip-space */
    /* here the eye ray becomes two coordinates, since the eye ray pierces the 
        clip-space box, which can be described as a line segment defined by the 
        eye ray's intersection w/ the near & far planes; the near & far clip 
        planes defined by korl-vulkan are 1 & 0 respectively, since Vulkan 
        (with no extensions, which I don't want to use) requires a normalized 
        depth buffer, and korl-vulkan uses a "reverse" depth buffer */
    //KORL-ISSUE-000-000-102: gfx: testing required; ASSUMPTION: right-handed HC-space coordinate system that spans [0,1]; need to actually test to see if this works
    const Korl_Math_V4f32 eyeRayHcsFar  = {eyeRayNds.x, eyeRayNds.y, 0.f, 1.f};
    const Korl_Math_V4f32 eyeRayHcsNear = {eyeRayNds.x, eyeRayNds.y, 1.f, 1.f};
    /* homogeneous-clip-space => UNSCALED world-space; unscaled because transforms through projection matrix likely modify the `w` component, so to get the "true" V3f32, we need to multiply all components by `1 / w` */
    const Korl_Math_V4f32 eyeRayFarUnscaled  = korl_math_m4f32_multiplyV4f32(&viewProjectionInverse, &eyeRayHcsFar);
    const Korl_Math_V4f32 eyeRayNearUnscaled = korl_math_m4f32_multiplyV4f32(&viewProjectionInverse, &eyeRayHcsNear);
    /* UNSCALED world-space => TRUE world-space */
    const Korl_Math_V3f32 eyeRayFar  = korl_math_v3f32_multiplyScalar(eyeRayFarUnscaled.xyz, 1.f / eyeRayFarUnscaled.w);
    const Korl_Math_V3f32 eyeRayNear = korl_math_v3f32_multiplyScalar(eyeRayNearUnscaled.xyz, 1.f / eyeRayNearUnscaled.w);
    /* compute the ray using the final transformed eye ray line segment */
    const Korl_Math_V3f32 eyeRayNearToFar         = korl_math_v3f32_subtract(eyeRayFar, eyeRayNear);
    const f32             eyeRayNearToFarDistance = korl_math_v3f32_magnitude(&eyeRayNearToFar);
    result.position        = eyeRayNear;
    result.direction       = korl_math_v3f32_normalKnownMagnitude(eyeRayNearToFar, eyeRayNearToFarDistance);
    result.segmentDistance = eyeRayNearToFarDistance;
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_camera_worldToWindow(korl_gfx_camera_worldToWindow)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    //KORL-PERFORMANCE-000-000-041: gfx: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view       = korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection = korl_gfx_camera_projection(context);
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_Aabb2f32 viewport     = {.min={0,0}
                                            ,.max={KORL_C_CAST(f32, gfxContext->surfaceSize.x)
                                                  ,KORL_C_CAST(f32, gfxContext->surfaceSize.y)}};
    const Korl_Math_V2f32    viewportSize = korl_math_aabb2f32_size(viewport);
    /* transform from world => camera => clip space */
    const Korl_Math_V4f32 worldPoint       = {worldPosition.x, worldPosition.y, worldPosition.z, 1};
    const Korl_Math_V4f32 cameraSpacePoint = korl_math_m4f32_multiplyV4f32(&view, &worldPoint);
    const Korl_Math_V4f32 clipSpacePoint   = korl_math_m4f32_multiplyV4f32(&projection, &cameraSpacePoint);
    if(korl_math_isNearlyZero(clipSpacePoint.w))
        return (Korl_Math_V2f32){korl_math_f32_nan(), korl_math_f32_nan()};
    /* calculate normalized-device-coordinate-space 
        y is inverted here because screen-space y axis is flipped! */
    const Korl_Math_V3f32 ndcSpacePoint = { clipSpacePoint.x /  clipSpacePoint.w
                                          , clipSpacePoint.y / -clipSpacePoint.w
                                          , clipSpacePoint.z /  clipSpacePoint.w };
    /* Calculate screen-space.  GLSL formula: ((ndcSpacePoint.xy + 1.0) / 2.0) * viewSize + viewOffset */
    const Korl_Math_V2f32 result = { ((ndcSpacePoint.x + 1.f) / 2.f) * viewportSize.x + viewport.min.x
                                   , ((ndcSpacePoint.y + 1.f) / 2.f) * viewportSize.y + viewport.min.y };
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_getSurfaceSize(korl_gfx_getSurfaceSize)
{
    return _korl_gfx_context->surfaceSize;
}
korl_internal KORL_FUNCTION_korl_gfx_setClearColor(korl_gfx_setClearColor)
{
    korl_vulkan_setSurfaceClearColor((f32[]){KORL_C_CAST(f32,   red) / KORL_C_CAST(f32, KORL_U8_MAX)
                                            ,KORL_C_CAST(f32, green) / KORL_C_CAST(f32, KORL_U8_MAX)
                                            ,KORL_C_CAST(f32,  blue) / KORL_C_CAST(f32, KORL_U8_MAX)});
}
korl_internal KORL_FUNCTION_korl_gfx_draw(korl_gfx_draw)
{
    if(materials)
        korl_assert(materialsSize);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_PushConstantData, pushConstantData);
    *KORL_C_CAST(Korl_Math_M4f32*, pushConstantData.vertex) = korl_math_transform3d_m4f32(&context->transform);
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.pushConstantData = &pushConstantData;
    korl_gfx_setDrawState(&drawState);
    switch(context->type)
    {
    case KORL_GFX_DRAWABLE_TYPE_RUNTIME:{
        /* configure the renderer draw state */
        Korl_Gfx_Material materialLocal;
        if(materials)
        {
            korl_assert(materialsSize == 1);
            materialLocal                      = materials[0];
            materialLocal.modes.primitiveType  = context->subType.runtime.overrides.primitiveType;
            materialLocal.modes.flags         |= context->subType.runtime.overrides.materialModeFlags;
        }
        else
            materialLocal = korl_gfx_material_defaultUnlit(context->subType.runtime.overrides.primitiveType, context->subType.runtime.overrides.materialModeFlags, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
        if(context->subType.runtime.overrides.materialMapBase)
            materialLocal.maps.resourceHandleTextureBase = context->subType.runtime.overrides.materialMapBase;
        if(context->subType.runtime.overrides.shaderVertex)
            materialLocal.shaders.resourceHandleShaderVertex = context->subType.runtime.overrides.shaderVertex;
        else if(!materialLocal.shaders.resourceHandleShaderVertex)
            materialLocal.shaders.resourceHandleShaderVertex = korl_gfx_getBuiltInShaderVertex(&context->subType.runtime.vertexStagingMeta);
        if(context->subType.runtime.overrides.shaderFragment)
            materialLocal.shaders.resourceHandleShaderFragment = context->subType.runtime.overrides.shaderFragment;
        else if(!materialLocal.shaders.resourceHandleShaderFragment)
            materialLocal.shaders.resourceHandleShaderFragment = korl_gfx_getBuiltInShaderFragment(&materialLocal);
        KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
        storageBuffers.resourceHandleVertex = context->subType.runtime.overrides.storageBufferVertex;
        drawState = KORL_STRUCT_INITIALIZE_ZERO(Korl_Gfx_DrawState);
        if(context->subType.runtime.overrides.storageBufferVertex)
            drawState.storageBuffers = &storageBuffers;
        drawState.material         = &materialLocal;
        drawState.pushConstantData = context->subType.runtime.overrides.pushConstantData;
        korl_vulkan_setDrawState(&drawState);
        switch(context->subType.runtime.type)
        {
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
            korl_gfx_drawStagingAllocation(&context->subType.runtime.subType.singleFrame.stagingAllocation, &context->subType.runtime.vertexStagingMeta);
            break;
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:{
            korl_gfx_drawVertexBuffer(context->subType.runtime.subType.multiFrame.resourceHandleBuffer, 0, &context->subType.runtime.vertexStagingMeta);
            break;}
        }
        break;}
    case KORL_GFX_DRAWABLE_TYPE_MESH:{
        const acu8 utf8MeshName = (acu8){.size = context->subType.mesh.rawUtf8Scene3dMeshNameSize
                                        ,.data = context->subType.mesh.rawUtf8Scene3dMeshName};
        u32                                       meshPrimitiveCount       = 0;
        Korl_Vulkan_DeviceMemory_AllocationHandle meshPrimitiveBuffer      = 0;
        const Korl_Gfx_VertexStagingMeta*         meshPrimitiveVertexMetas = NULL;
        const Korl_Gfx_Material*                  meshMaterials            = NULL;
        korl_resource_scene3d_getMeshDrawData(context->subType.mesh.resourceHandleScene3d, utf8MeshName
                                             ,&meshPrimitiveCount
                                             ,&meshPrimitiveBuffer
                                             ,&meshPrimitiveVertexMetas
                                             ,&meshMaterials);
        Korl_Gfx_Material materialLocal;
        for(u32 mp = 0; mp < meshPrimitiveCount; mp++)
        {
            drawState = KORL_STRUCT_INITIALIZE_ZERO(Korl_Gfx_DrawState);
            if(materials && mp < materialsSize)
            {
                materialLocal                      = materials[mp];
                materialLocal.modes.primitiveType  = meshMaterials[mp].modes.primitiveType;
                materialLocal.modes.flags         |= meshMaterials[mp].modes.flags;
                drawState.material = &materialLocal;
            }
            else
                drawState.material = meshMaterials + mp;
            korl_vulkan_setDrawState(&drawState);
            korl_vulkan_drawVertexBuffer(meshPrimitiveBuffer, 0, meshPrimitiveVertexMetas + mp);
        }
        break;}
    }
}
korl_internal KORL_FUNCTION_korl_gfx_setDrawState(korl_gfx_setDrawState)
{
    Korl_Gfx_DrawState drawState2 = *drawState;
    Korl_Gfx_DrawState_Lighting lighting;// leave uninitialized unless we need to flush light data
    if(!KORL_MEMORY_POOL_ISEMPTY(_korl_gfx_context->pendingLights))
    {
        korl_memory_zero(&lighting, sizeof(lighting));
        lighting.lightsCount = KORL_MEMORY_POOL_SIZE(_korl_gfx_context->pendingLights);
        lighting.lights      = _korl_gfx_context->pendingLights;
        drawState2.lighting = &lighting;
        _korl_gfx_context->pendingLights_korlMemoryPoolSize = 0;// does not destroy current lighting data, which is exactly what we need for the remainder of this stack!
    }
    korl_vulkan_setDrawState(&drawState2);
}
korl_internal KORL_FUNCTION_korl_gfx_stagingAllocate(korl_gfx_stagingAllocate)
{
    return korl_vulkan_stagingAllocate(stagingMeta);
}
korl_internal KORL_FUNCTION_korl_gfx_stagingReallocate(korl_gfx_stagingReallocate)
{
    return korl_vulkan_stagingReallocate(stagingMeta, stagingAllocation);
}
korl_internal KORL_FUNCTION_korl_gfx_drawStagingAllocation(korl_gfx_drawStagingAllocation)
{
    korl_vulkan_drawStagingAllocation(stagingAllocation, stagingMeta);
}
korl_internal KORL_FUNCTION_korl_gfx_drawVertexBuffer(korl_gfx_drawVertexBuffer)
{
    const Korl_Vulkan_DeviceMemory_AllocationHandle bufferDeviceMemoryAllocationHandle = korl_resource_gfxBuffer_getVulkanDeviceMemoryAllocationHandle(resourceHandleBuffer);
    korl_vulkan_drawVertexBuffer(bufferDeviceMemoryAllocationHandle, bufferByteOffset, stagingMeta);
}
korl_internal KORL_FUNCTION_korl_gfx_getBuiltInShaderVertex(korl_gfx_getBuiltInShaderVertex)
{
    if(vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize == 2)
        if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
           && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex2dColor;
        else if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
                && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex2dUv;
        else
            return _korl_gfx_context->resourceShaderKorlVertex2d;
    else if(vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize == 3)
        if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
           && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex3dColor;
        else if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
                && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex3dUv;
        else
            return _korl_gfx_context->resourceShaderKorlVertex3d;
    korl_log(ERROR, "unsupported built-in vertex shader configuration");
    return 0;
}
korl_internal KORL_FUNCTION_korl_gfx_getBuiltInShaderFragment(korl_gfx_getBuiltInShaderFragment)
{
    if(material->maps.resourceHandleTextureBase)
        return _korl_gfx_context->resourceShaderKorlFragmentColorTexture;
    return _korl_gfx_context->resourceShaderKorlFragmentColor;
}
korl_internal KORL_FUNCTION_korl_gfx_light_use(korl_gfx_light_use)
{
    for(u$ i = 0; i < lightsSize; i++)
    {
        Korl_Gfx_Light*const newLight = KORL_MEMORY_POOL_ADD(_korl_gfx_context->pendingLights);
        *newLight = lights[i];
    }
}
korl_internal KORL_FUNCTION_korl_gfx_getBlankTexture(korl_gfx_getBlankTexture)
{
    return _korl_gfx_context->blankTexture;
}
korl_internal void korl_gfx_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(!korl_memory_allocator_isFragmented(_korl_gfx_context->allocatorHandle))
        return;
    Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 16);
    KORL_MEMORY_STB_DA_DEFRAGMENT                (stackAllocator, stbDaDefragmentPointers, _korl_gfx_context);
    korl_stringPool_collectDefragmentPointers(_korl_gfx_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_gfx_context);
    korl_memory_allocator_defragment(_korl_gfx_context->allocatorHandle, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
}
korl_internal u32 korl_gfx_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.data = KORL_C_CAST(u8*, &_korl_gfx_context), .size = sizeof(_korl_gfx_context)});
    return byteOffset;
}
korl_internal void korl_gfx_memoryStateRead(const u8* memoryState)
{
    _korl_gfx_context = *KORL_C_CAST(_Korl_Gfx_Context**, memoryState);
}
#undef _LOCAL_STRING_POOL_POINTER
