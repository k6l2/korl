#include "utility/korl-utility-gfx.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-checkCast.h"
#include "utility/korl-utility-resource.h"
#include "utility/korl-utility-stb-ds.h"
#include "korl-interface-platform.h"
korl_global_const Korl_Math_V2f32 _KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP[4] = {{{0,1}}, {{0,0}}, {{1,1}}, {{1,0}}};
/* Since we expect that all KORL renderer code requires right-handed 
    triangle normals (counter-clockwise vertex winding), all tri quads have 
    the following formation:
    [3]-[2]
     | / | 
    [0]-[1] */
korl_global_const u16 _KORL_UTILITY_GFX_TRI_QUAD_INDICES[] = 
    { 0, 1, 2
    , 2, 3, 0 };
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
    const u$ bytesRequired     = sizeof(Korl_Gfx_Text);
    Korl_Gfx_Text*const result = KORL_C_CAST(Korl_Gfx_Text*, korl_allocate(allocator, bytesRequired));
    result->allocator                = allocator;
    result->textPixelHeight          = textPixelHeight;
    result->resourceHandleFont       = resourceHandleFont;
    result->transform                = korl_math_transform3d_identity();
    result->resourceHandleBufferText = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER), &createInfoBufferText, false);
    // defer adding the vertex indices until _just_ before we draw the text, as this will allow us to shift the entire buffer to effectively delete lines of text @korl-gfx-text-defer-index-buffer
    result->vertexStagingMeta.indexCount = korl_arraySize(_KORL_UTILITY_GFX_TRI_QUAD_INDICES);
    result->vertexStagingMeta.indexType  = KORL_GFX_VERTEX_INDEX_TYPE_U16; korl_assert(sizeof(*_KORL_UTILITY_GFX_TRI_QUAD_INDICES) == sizeof(u16));
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = offsetof(_Korl_Gfx_Text_GlyphInstance, position);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(_Korl_Gfx_Text_GlyphInstance);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteOffsetBuffer = offsetof(_Korl_Gfx_Text_GlyphInstance, meshIndex);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteStride       = sizeof(_Korl_Gfx_Text_GlyphInstance);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE;
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
        const acu16 utf16Substring = {.size = korl_checkCast_i$_to_u$(utf16It->_currentRawUtf16 - utf16Text.data - *io_substringStartIndex)
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
            _Korl_Gfx_Text_GlyphInstance*const glyphInstanceUpdateBuffer = KORL_C_CAST(_Korl_Gfx_Text_GlyphInstance*, korl_resource_getUpdateBuffer(context->resourceHandleBufferText, context->totalVisibleGlyphs * sizeof(_Korl_Gfx_Text_GlyphInstance), &bytesRequested_bytesAvailable));
            korl_resource_font_generateUtf16(context->resourceHandleFont, context->textPixelHeight, utf16Substring, korl_math_v2f32_make(currentLine->aabbSizeX, 0)
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
    if(!korl_resource_isLoaded(context->resourceHandleFont))
    {
        korl_log(ERROR, "malformed Text; font resource not loaded");
        return;
    }
    const Korl_Resource_Font_Metrics   fontMetrics         = korl_resource_font_getMetrics(context->resourceHandleFont, context->textPixelHeight);
    const Korl_Math_V4f32              defaultLineColor    = korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE);// default all line colors to white
    /* iterate over utf16Text until we hit a line break; upon reaching a line break, we can add another _Korl_Gfx_Text_Line to the context */
    _Korl_Gfx_Text_Line*               currentLine         = NULL;// the current working Line of text; we will accumulate substrings of codepoints which pass codepointTest (if it's provided) into this Line, until we reach a line-break ('\n') character
    Korl_Math_V4f32                    currentLineColor    = defaultLineColor;
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
                KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabbSize.x, currentLine->aabbSizeX);
                /* we're done with this Line now; nullify currentLine reference */
                currentLine = NULL;
                currentLineColor = defaultLineColor;
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
        KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabbSize.x, currentLine->aabbSizeX);
    }
    /* we can easily calculate the height of the Text object's current AABB 
        using the font's cached metrics */
    const f32 lineDeltaY = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    context->_modelAabbSize.y = KORL_C_CAST(f32, arrlenu(context->stbDaLines)) * lineDeltaY;
}
korl_internal void korl_gfx_text_fifoRemove(Korl_Gfx_Text* context, u$ lineCount)
{
    if(!korl_resource_isLoaded(context->resourceHandleFont))
    {
        korl_log(ERROR, "malformed Text; font resource not loaded");
        return;
    }
    const u$ linesToRemove = KORL_MATH_MIN(lineCount, arrlenu(context->stbDaLines));
    context->_modelAabbSize.x = 0;
    u$ glyphsToRemove = 0;
    for(u$ l = 0; l < arrlenu(context->stbDaLines); l++)
    {
        if(l < linesToRemove)
        {
            glyphsToRemove += context->stbDaLines[l].visibleCharacters;
            continue;
        }
        const _Korl_Gfx_Text_Line*const line = &(context->stbDaLines[l]);
        KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabbSize.x, line->aabbSizeX);
    }
    arrdeln(context->stbDaLines, 0, linesToRemove);
    context->totalVisibleGlyphs -= korl_checkCast_u$_to_u32(glyphsToRemove);
    korl_resource_shift(context->resourceHandleBufferText, -1/*shift to the left; remove the glyphs instances at the beginning of the buffer*/ * korl_checkCast_u$_to_i$(glyphsToRemove * sizeof(_Korl_Gfx_Text_GlyphInstance)));
    const Korl_Resource_Font_Metrics fontMetrics = korl_resource_font_getMetrics(context->resourceHandleFont, context->textPixelHeight);
    const f32 lineDeltaY = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    context->_modelAabbSize.y = KORL_C_CAST(f32, arrlenu(context->stbDaLines)) * lineDeltaY;
}
korl_internal void korl_gfx_text_draw(Korl_Gfx_Text* context, Korl_Math_Aabb2f32 visibleRegion)
{
    if(!korl_resource_isLoaded(context->resourceHandleFont))
        return;
    const Korl_Resource_Font_Metrics fontMetrics = korl_resource_font_getMetrics(context->resourceHandleFont, context->textPixelHeight);
    const f32 sizeToSupportedSizeRatio = context->textPixelHeight / fontMetrics.nearestSupportedPixelHeight;// korl-resource-text will only bake glyphs at discrete pixel heights (for complexity/memory reasons); it is up to us to scale the final text mesh by the appropriate ratio to actually draw text at `textPixelHeight`
    const f32 lineDeltaY = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    /* we can now send the vertex index data to the buffer, which will be used 
        for all subsequent text line draw calls @korl-gfx-text-defer-index-buffer */
    const u$ glyphInstanceBufferSize = context->totalVisibleGlyphs * sizeof(_Korl_Gfx_Text_GlyphInstance);
    {
        const u32 indexBufferBytes   = sizeof(_KORL_UTILITY_GFX_TRI_QUAD_INDICES);
        const u$  currentBufferBytes = korl_resource_getByteSize(context->resourceHandleBufferText);
        if(currentBufferBytes < glyphInstanceBufferSize + indexBufferBytes)
            korl_resource_resize(context->resourceHandleBufferText, glyphInstanceBufferSize + indexBufferBytes);
        korl_resource_update(context->resourceHandleBufferText, _KORL_UTILITY_GFX_TRI_QUAD_INDICES, sizeof(_KORL_UTILITY_GFX_TRI_QUAD_INDICES), glyphInstanceBufferSize);
        // context->vertexStagingMeta.indexByteOffsetBuffer = glyphInstanceBufferSize;//can't do this yet, since this is relative to the buffer offset byte we choose per line!
    }
    /* configure the renderer draw state */
    const Korl_Resource_Font_Resources fontResources = korl_resource_font_getResources(context->resourceHandleFont, context->textPixelHeight);
    Korl_Gfx_Material material = korl_gfx_material_defaultUnlitFlags(KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_BLEND);
    material.maps.resourceHandleTextureBase       = fontResources.resourceHandleTexture;
    material.shaders.resourceHandleShaderVertex   = korl_gfx_getBuiltInShaderVertex(&context->vertexStagingMeta);
    material.shaders.resourceHandleShaderFragment = korl_gfx_getBuiltInShaderFragment(&material);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
    storageBuffers.resourceHandleVertex = fontResources.resourceHandleSsboGlyphMeshVertices;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.storageBuffers = &storageBuffers;
    korl_assert(korl_gfx_setDrawState(&drawState));
    /* now we can iterate over each text line and conditionally draw them using line-specific draw state */
    KORL_ZERO_STACK(Korl_Gfx_DrawState_PushConstantData, pushConstantData);
    Korl_Math_M4f32*const pushConstantModelM4f32 = KORL_C_CAST(Korl_Math_M4f32*, pushConstantData.vertex);
    Korl_Math_V3f32 modelTranslation = context->transform._position;// the default origin position for each Line is the upper-left corner of the TextMetrics AABB, which is exactly where we want to start drawing the first line relative to our transform._position
    u$ currentVisibleGlyphOffset = 0;// used to determine the byte (transform required) offset into the Text object's text buffer resource
    const _Korl_Gfx_Text_Line*const linesEnd = context->stbDaLines + arrlen(context->stbDaLines);
    for(const _Korl_Gfx_Text_Line* line = context->stbDaLines; line < linesEnd; line++)
    {
        if(modelTranslation.y < visibleRegion.min.y - lineDeltaY)
            break;
        if(modelTranslation.y <= visibleRegion.max.y + lineDeltaY)
        {
            material.fragmentShaderUniform.factorColorBase = line->color;
            *pushConstantModelM4f32                        = korl_math_makeM4f32_rotateScaleTranslate(context->transform._versor, korl_math_v3f32_multiply(context->transform._scale, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = {sizeToSupportedSizeRatio, sizeToSupportedSizeRatio}}), modelTranslation);
            KORL_ZERO_STACK(Korl_Gfx_DrawState, drawStateLine);
            drawStateLine.material         = &material;
            drawStateLine.pushConstantData = &pushConstantData;
            if(korl_gfx_setDrawState(&drawStateLine))
            {
                const u$ textLineByteOffset = currentVisibleGlyphOffset * sizeof(_Korl_Gfx_Text_GlyphInstance);
                context->vertexStagingMeta.indexByteOffsetBuffer = korl_checkCast_u$_to_u32(glyphInstanceBufferSize - textLineByteOffset);
                context->vertexStagingMeta.instanceCount         = line->visibleCharacters;
                korl_gfx_drawVertexBuffer(context->resourceHandleBufferText, textLineByteOffset, &context->vertexStagingMeta, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES);
            }
        }
        modelTranslation.y        -= lineDeltaY;
        currentVisibleGlyphOffset += line->visibleCharacters;
    }
}
korl_internal Korl_Math_V2f32 korl_gfx_text_getModelAabbSize(const Korl_Gfx_Text* context)
{
    return context->_modelAabbSize;
}
korl_internal u8 korl_gfx_indexBytes(Korl_Gfx_VertexIndexType vertexIndexType)
{
    switch(vertexIndexType)
    {
    case KORL_GFX_VERTEX_INDEX_TYPE_U16    : return sizeof(u16);
    case KORL_GFX_VERTEX_INDEX_TYPE_U32    : return sizeof(u32);
    case KORL_GFX_VERTEX_INDEX_TYPE_INVALID: return 0;
    }
}
korl_internal Korl_Math_V4f32 korl_gfx_color_toLinear(Korl_Gfx_Color4u8 color)
{
    return korl_math_v4f32_make(KORL_C_CAST(f32, color.r) / KORL_C_CAST(f32, KORL_U8_MAX)
                               ,KORL_C_CAST(f32, color.g) / KORL_C_CAST(f32, KORL_U8_MAX)
                               ,KORL_C_CAST(f32, color.b) / KORL_C_CAST(f32, KORL_U8_MAX)
                               ,KORL_C_CAST(f32, color.a) / KORL_C_CAST(f32, KORL_U8_MAX));
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultUnlit(void)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         = KORL_GFX_MATERIAL_MODE_FLAGS_NONE}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = KORL_MATH_V4F32_ONE
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 0}
                                                    ,.maps = {.resourceHandleTextureBase     = 0
                                                             ,.resourceHandleTextureSpecular = 0
                                                             ,.resourceHandleTextureEmissive = 0}
                                                    ,.shaders = {.resourceHandleShaderVertex   = 0
                                                                ,.resourceHandleShaderFragment = 0}};
}
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultUnlitFlags(Korl_Gfx_Material_Mode_Flags flags)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         = flags}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = KORL_MATH_V4F32_ONE
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 0}
                                                    ,.maps = {.resourceHandleTextureBase     = 0
                                                             ,.resourceHandleTextureSpecular = 0
                                                             ,.resourceHandleTextureEmissive = 0}
                                                    ,.shaders = {.resourceHandleShaderVertex   = 0
                                                                ,.resourceHandleShaderFragment = 0}};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultUnlitFlagsColorbase(Korl_Gfx_Material_Mode_Flags flags, Korl_Math_V4f32 colorLinear4Base)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         = flags}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = colorLinear4Base
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 0}
                                                    ,.maps = {.resourceHandleTextureBase     = 0
                                                             ,.resourceHandleTextureSpecular = 0
                                                             ,.resourceHandleTextureEmissive = 0}
                                                    ,.shaders = {.resourceHandleShaderVertex   = 0
                                                                ,.resourceHandleShaderFragment = 0}};
}
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultSolid(void)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         =  KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                                                               | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = KORL_MATH_V4F32_ONE
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 32}
                                                    ,.maps = {.resourceHandleTextureBase     = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureSpecular = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureEmissive = korl_gfx_getBlankTexture()}
                                                    ,.shaders = {.resourceHandleShaderVertex   = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                ,.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit-solid.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)}};
}
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultSolidColorbase(Korl_Math_V4f32 colorLinear4Base)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         =  KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                                                               | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = colorLinear4Base
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 32}
                                                    ,.maps = {.resourceHandleTextureBase     = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureSpecular = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureEmissive = korl_gfx_getBlankTexture()}
                                                    ,.shaders = {.resourceHandleShaderVertex   = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                ,.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit-solid.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)}};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultLit(void)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         =  KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                                                               | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = KORL_MATH_V4F32_ONE
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 32}
                                                    ,.maps = {.resourceHandleTextureBase     = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureSpecular = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureEmissive = korl_gfx_getBlankTexture()}
                                                    ,.shaders = {.resourceHandleShaderVertex   = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                ,.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)}};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultLitColorbase(Korl_Math_V4f32 colorLinear4Base)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         =  KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                                                               | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = colorLinear4Base
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 32}
                                                    ,.maps = {.resourceHandleTextureBase     = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureSpecular = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureEmissive = korl_gfx_getBlankTexture()}
                                                    ,.shaders = {.resourceHandleShaderVertex   = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                ,.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)}};
}
korl_internal Korl_Gfx_Material korl_gfx_material_flatSolidTriangleWireframe(Korl_Math_V4f32 colorLinear4Base, Korl_Math_V4f32 colorLinear4Wireframe)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = colorLinear4Base.w == 0 ? KORL_GFX_MATERIAL_CULL_MODE_NONE : KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         =  KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                                                               | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = colorLinear4Base
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = colorLinear4Wireframe
                                                                              ,.shininess           = 32}
                                                    ,.maps = {.resourceHandleTextureBase     = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureSpecular = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureEmissive = korl_gfx_getBlankTexture()}
                                                    ,.shaders = {.resourceHandleShaderVertex   = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-flat-passThrough.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                ,.resourceHandleShaderGeometry = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-flat-wireframe.geom.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                ,.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit-solid-wireframe.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)}};
}
korl_internal Korl_Gfx_Camera korl_gfx_camera_createFov(f32 fovVerticalDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 normalForward, Korl_Math_V3f32 normalUp)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.position                                 = position;
    result.normalForward                            = normalForward;
    result.normalUp                                 = normalUp;
    result._viewportScissorPosition                 = korl_math_v2f32_make(0, 0);
    result._viewportScissorSize                     = korl_math_v2f32_make(1, 1);
    result.subCamera.perspective.clipNear           = clipNear;
    result.subCamera.perspective.clipFar            = clipFar;
    result.subCamera.perspective.fovVerticalDegrees = fovVerticalDegrees;
    return result;
}
korl_internal Korl_Gfx_Camera korl_gfx_camera_createOrtho(f32 clipDepth)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC;
    result.position                            = KORL_MATH_V3F32_ZERO;
    result.normalForward                       = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result.normalUp                            = KORL_MATH_V3F32_Y;
    result._viewportScissorPosition            = korl_math_v2f32_make(0, 0);
    result._viewportScissorSize                = korl_math_v2f32_make(1, 1);
    result.subCamera.orthographic.clipDepth    = clipDepth;
    result.subCamera.orthographic.originAnchor = korl_math_v2f32_make(0.5f, 0.5f);
    return result;
}
korl_internal Korl_Gfx_Camera korl_gfx_camera_createOrthoFixedHeight(f32 fixedHeight, f32 clipDepth)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT;
    result.position                            = KORL_MATH_V3F32_ZERO;
    result.normalForward                       = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result.normalUp                            = KORL_MATH_V3F32_Y;
    result._viewportScissorPosition            = korl_math_v2f32_make(0, 0);
    result._viewportScissorSize                = korl_math_v2f32_make(1, 1);
    result.subCamera.orthographic.fixedHeight  = fixedHeight;
    result.subCamera.orthographic.clipDepth    = clipDepth;
    result.subCamera.orthographic.originAnchor = korl_math_v2f32_make(0.5f, 0.5f);
    return result;
}
korl_internal void korl_gfx_camera_fovRotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians, Korl_Math_V3f32 target)
{
    korl_assert(context->type == KORL_GFX_CAMERA_TYPE_PERSPECTIVE);
    const Korl_Math_Quaternion q32Rotation = korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false);
    const Korl_Math_V3f32 targetToNewPosition = korl_math_quaternion_transformV3f32(q32Rotation
                                                                                   ,korl_math_v3f32_subtract(context->position, target)
                                                                                   ,true);
    context->position      = korl_math_v3f32_add(target, targetToNewPosition);
    context->normalForward = korl_math_quaternion_transformV3f32(q32Rotation, context->normalForward, true);
    context->normalUp      = korl_math_quaternion_transformV3f32(q32Rotation, context->normalUp     , true);
}
korl_internal void korl_gfx_camera_setScissor(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY)
{
    f32 x2 = x + sizeX;
    f32 y2 = y + sizeY;
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x2 < 0) x2 = 0;
    if(y2 < 0) y2 = 0;
    context->_viewportScissorPosition.x = x;
    context->_viewportScissorPosition.y = y;
    context->_viewportScissorSize.x     = x2 - x;
    context->_viewportScissorSize.y     = y2 - y;
    context->_scissorType               = KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE;
}
korl_internal void korl_gfx_camera_setScissorPercent(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight)
{
    f32 x2 = viewportRatioX + viewportRatioWidth;
    f32 y2 = viewportRatioY + viewportRatioHeight;
    if(viewportRatioX < 0) viewportRatioX = 0;
    if(viewportRatioY < 0) viewportRatioY = 0;
    if(x2 < 0) x2 = 0;
    if(y2 < 0) y2 = 0;
    context->_viewportScissorPosition.x = viewportRatioX;
    context->_viewportScissorPosition.y = viewportRatioY;
    context->_viewportScissorSize.x     = x2 - viewportRatioX;
    context->_viewportScissorSize.y     = y2 - viewportRatioY;
    context->_scissorType               = KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO;
}
korl_internal void korl_gfx_camera_orthoSetOriginAnchor(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY)
{
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        korl_assert(!"origin anchor not supported for perspective camera");
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        context->subCamera.orthographic.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographic.originAnchor.y = swapchainSizeRatioOriginY;
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        context->subCamera.orthographic.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographic.originAnchor.y = swapchainSizeRatioOriginY;
        break;}
    }
}
korl_internal Korl_Math_M4f32 korl_gfx_camera_projection(const Korl_Gfx_Camera*const context)
{
    const Korl_Math_V2u32 surfaceSize = korl_gfx_getSurfaceSize();
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        const f32 viewportWidthOverHeight = surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, surfaceSize.x)
             / KORL_C_CAST(f32, surfaceSize.y);
        return korl_math_m4f32_projectionFov(context->subCamera.perspective.fovVerticalDegrees
                                            ,viewportWidthOverHeight
                                            ,context->subCamera.perspective.clipNear
                                            ,context->subCamera.perspective.clipFar);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        const f32 left   = 0.f - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, surfaceSize.x);
        const f32 bottom = 0.f - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, surfaceSize.y);
        const f32 right  = KORL_C_CAST(f32, surfaceSize.x) - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, surfaceSize.x);
        const f32 top    = KORL_C_CAST(f32, surfaceSize.y) - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, surfaceSize.y);
        const f32 far    = -context->subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        return korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, surfaceSize.x) 
             / KORL_C_CAST(f32, surfaceSize.y);
        /* w / fixedHeight == windowAspectRatio */
        const f32 width  = context->subCamera.orthographic.fixedHeight * viewportWidthOverHeight;
        const f32 left   = 0.f - context->subCamera.orthographic.originAnchor.x*width;
        const f32 bottom = 0.f - context->subCamera.orthographic.originAnchor.y*context->subCamera.orthographic.fixedHeight;
        const f32 right  = width       - context->subCamera.orthographic.originAnchor.x*width;
        const f32 top    = context->subCamera.orthographic.fixedHeight - context->subCamera.orthographic.originAnchor.y*context->subCamera.orthographic.fixedHeight;
        const f32 far    = -context->subCamera.orthographic.clipDepth;
        const f32 near   = 1e-7f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        return korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);}
    default:{
        korl_log(ERROR, "invalid camera type: %i", context->type);
        return KORL_STRUCT_INITIALIZE_ZERO(Korl_Math_M4f32);}
    }
}
korl_internal Korl_Math_M4f32 korl_gfx_camera_view(const Korl_Gfx_Camera*const context)
{
    const Korl_Math_V3f32 cameraTarget = korl_math_v3f32_add(context->position, context->normalForward);
    return korl_math_m4f32_lookAt(&context->position, &cameraTarget, &context->normalUp);
}
korl_internal void korl_gfx_camera_drawFrustum(const Korl_Gfx_Camera*const context, const Korl_Gfx_Material* material)
{
    /* calculate the world-space rays for each corner of the render surface */
    const Korl_Math_V2u32 surfaceSize = korl_gfx_getSurfaceSize();
    Korl_Gfx_ResultRay3d cameraWorldCorners[4];
    /* camera coordinates in window-space are CCW, starting from the lower-left:
        {lower-left, lower-right, upper-right, upper-left} */
    cameraWorldCorners[0] = korl_gfx_camera_windowToWorld(context, korl_math_v2i32_make(0, 0));
    cameraWorldCorners[1] = korl_gfx_camera_windowToWorld(context, korl_math_v2i32_make(korl_checkCast_u$_to_i32(surfaceSize.x), 0));
    cameraWorldCorners[2] = korl_gfx_camera_windowToWorld(context, korl_math_v2i32_make(korl_checkCast_u$_to_i32(surfaceSize.x)
                                                                                       ,korl_checkCast_u$_to_i32(surfaceSize.y)));
    cameraWorldCorners[3] = korl_gfx_camera_windowToWorld(context, korl_math_v2i32_make(0, korl_checkCast_u$_to_i32(surfaceSize.y)));
    Korl_Math_V3f32 cameraWorldCornersFar[4];
    for(u8 i = 0; i < 4; i++)
    {
        /* sanity check the results */
        if(korl_math_f32_isNan(cameraWorldCorners[i].position.x))
            korl_log(ERROR, "window=>world translation failed for cameraAabbEyeRays[%hhu]", i);
        /* compute the world-space positions of the far-plane eye ray intersections */
        cameraWorldCornersFar[i] = korl_math_v3f32_add(cameraWorldCorners[i].position, korl_math_v3f32_multiplyScalar(cameraWorldCorners[i].direction, cameraWorldCorners[i].segmentDistance));
    }
    /* draw the frustum by composing a set of 3 world-space staple-shaped lines per corner */
    Korl_Math_V3f32* positions;
    korl_gfx_drawLines3d(KORL_MATH_V3F32_ZERO, KORL_MATH_QUATERNION_IDENTITY, 3 * korl_arraySize(cameraWorldCorners), material, &positions, NULL);
    for(u8 i = 0; i < korl_arraySize(cameraWorldCorners); i++)
    {
        const u$ iNext = (i + 1) % korl_arraySize(cameraWorldCorners);
        /*line for the near-plane quad face*/
        positions[2 * (3 * i + 0) + 0] = cameraWorldCorners[i].position;
        positions[2 * (3 * i + 0) + 1] = cameraWorldCorners[iNext].position;
        /*line for the far-plane quad face*/
        positions[2 * (3 * i + 1) + 0] = cameraWorldCornersFar[i];
        positions[2 * (3 * i + 1) + 1] = cameraWorldCornersFar[iNext];
        /*line connecting the near-plane to the far-plane on the current corner*/
        positions[2 * (3 * i + 2) + 0] = cameraWorldCorners[i].position;
        positions[2 * (3 * i + 2) + 1] = cameraWorldCornersFar[i];
    }
}
korl_internal Korl_Math_V2f32 korl_gfx_camera_orthoGetSize(const Korl_Gfx_Camera*const context)
{
    const Korl_Math_V2u32 surfaceSize = korl_gfx_getSurfaceSize();
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        return korl_math_v2f32_make(KORL_C_CAST(f32, surfaceSize.x)
                                   ,KORL_C_CAST(f32, surfaceSize.y));}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = surfaceSize.y == 0 
            ? 1.f 
            :   KORL_C_CAST(f32, surfaceSize.x) 
              / KORL_C_CAST(f32, surfaceSize.y);
        return korl_math_v2f32_make(context->subCamera.orthographic.fixedHeight * viewportWidthOverHeight// w / fixedHeight == windowAspectRatio
                                   ,context->subCamera.orthographic.fixedHeight);}
    default:{
        korl_log(ERROR, "invalid camera type: %i", context->type);
        return korl_math_v2f32_make(korl_math_f32_quietNan(), korl_math_f32_quietNan());}
    }
}
korl_internal Korl_Gfx_ResultRay3d korl_gfx_camera_windowToWorld(const Korl_Gfx_Camera*const context, Korl_Math_V2i32 windowPosition)
{
    const Korl_Math_V2u32 surfaceSize = korl_gfx_getSurfaceSize();
    Korl_Gfx_ResultRay3d result = {.position  = {korl_math_f32_quietNan(), korl_math_f32_quietNan(), korl_math_f32_quietNan()}
                                  ,.direction = {korl_math_f32_quietNan(), korl_math_f32_quietNan(), korl_math_f32_quietNan()}};
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
    const Korl_Math_V2f32 v2f32WindowPos = korl_math_v2f32_make(KORL_C_CAST(f32, windowPosition.x)
                                                               ,KORL_C_CAST(f32, windowPosition.y));
    /* viewport-space => normalized-device-space */
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_V2f32 eyeRayNds = korl_math_v2f32_make(  2.f * v2f32WindowPos.x / KORL_C_CAST(f32, surfaceSize.x) - 1.f
                                                          , -2.f * v2f32WindowPos.y / KORL_C_CAST(f32, surfaceSize.y) + 1.f);
    /* normalized-device-space => homogeneous-clip-space */
    /* here the eye ray becomes two coordinates, since the eye ray pierces the 
        clip-space box, which can be described as a line segment defined by the 
        eye ray's intersection w/ the near & far planes; the near & far clip 
        planes defined by korl-vulkan are 1 & 0 respectively, since Vulkan 
        (with no extensions, which I don't want to use) requires a normalized 
        depth buffer, and korl-vulkan uses a "reverse" depth buffer */
    //KORL-ISSUE-000-000-102: gfx: testing required; ASSUMPTION: right-handed HC-space coordinate system that spans [0,1]; need to actually test to see if this works
    const Korl_Math_V4f32 eyeRayHcsFar  = korl_math_v4f32_make(eyeRayNds.x, eyeRayNds.y, 0.f, 1.f);
    const Korl_Math_V4f32 eyeRayHcsNear = korl_math_v4f32_make(eyeRayNds.x, eyeRayNds.y, 1.f, 1.f);
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
korl_internal Korl_Math_V2f32 korl_gfx_camera_worldToWindow(const Korl_Gfx_Camera*const context, Korl_Math_V3f32 worldPosition)
{
    const Korl_Math_V2u32 surfaceSize = korl_gfx_getSurfaceSize();
    //KORL-PERFORMANCE-000-000-041: gfx: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view       = korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection = korl_gfx_camera_projection(context);
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_Aabb2f32 viewport     = {.min={0,0}
                                            ,.max={KORL_C_CAST(f32, surfaceSize.x)
                                                  ,KORL_C_CAST(f32, surfaceSize.y)}};
    const Korl_Math_V2f32    viewportSize = korl_math_aabb2f32_size(viewport);
    /* transform from world => camera => clip space */
    const Korl_Math_V4f32 worldPoint       = korl_math_v4f32_make(worldPosition.x, worldPosition.y, worldPosition.z, 1);
    const Korl_Math_V4f32 cameraSpacePoint = korl_math_m4f32_multiplyV4f32(&view, &worldPoint);
    const Korl_Math_V4f32 clipSpacePoint   = korl_math_m4f32_multiplyV4f32(&projection, &cameraSpacePoint);
    if(korl_math_isNearlyZero(clipSpacePoint.w))
        return korl_math_v2f32_make(korl_math_f32_quietNan(), korl_math_f32_quietNan());
    /* calculate normalized-device-coordinate-space 
        y is inverted here because screen-space y axis is flipped! */
    const Korl_Math_V3f32 ndcSpacePoint = korl_math_v3f32_make( clipSpacePoint.x /  clipSpacePoint.w
                                                              , clipSpacePoint.y / -clipSpacePoint.w
                                                              , clipSpacePoint.z /  clipSpacePoint.w );
    /* Calculate screen-space.  GLSL formula: ((ndcSpacePoint.xy + 1.0) / 2.0) * viewSize + viewOffset */
    const Korl_Math_V2f32 result = korl_math_v2f32_make( ((ndcSpacePoint.x + 1.f) / 2.f) * viewportSize.x + viewport.min.x
                                                       , ((ndcSpacePoint.y + 1.f) / 2.f) * viewportSize.y + viewport.min.y );
    return result;
}
korl_internal u32 _korl_gfx_vertexIndexType_byteStride(Korl_Gfx_VertexIndexType indexType)
{
    switch(indexType)
    {
    case KORL_GFX_VERTEX_INDEX_TYPE_INVALID: return 0;
    case KORL_GFX_VERTEX_INDEX_TYPE_U16    : return sizeof(u16);
    case KORL_GFX_VERTEX_INDEX_TYPE_U32    : return sizeof(u32);
    }
    korl_log(ERROR, "invalid indexType: %i", indexType);
    return 0;
}
korl_internal u32 _korl_gfx_attributeDatatype_byteStride(Korl_Gfx_RuntimeDrawableAttributeDatatype attributeDatatype)
{
    switch(attributeDatatype)
    {
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_INVALID: return 0;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_U32    : return sizeof(u32);
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32  : return sizeof(Korl_Math_V2f32);
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32  : return sizeof(Korl_Math_V3f32);
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8   : return sizeof(Korl_Math_V4u8);
    }
    korl_log(ERROR, "invalid attributeDatatype: %i", attributeDatatype);
    return 0;
}
korl_internal Korl_Gfx_VertexAttributeElementType _korl_gfx_attributeDatatype_elementType(Korl_Gfx_RuntimeDrawableAttributeDatatype attributeDatatype)
{
    switch(attributeDatatype)
    {
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_INVALID: return KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_U32    : return KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32  : return KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32  : return KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8   : return KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
    }
    korl_log(ERROR, "invalid attributeDatatype: %i", attributeDatatype);
    return KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID;
}
korl_internal u8 _korl_gfx_attributeDatatype_vectorSize(Korl_Gfx_RuntimeDrawableAttributeDatatype attributeDatatype)
{
    switch(attributeDatatype)
    {
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_INVALID: return 0;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_U32    : return 1;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32  : return 2;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32  : return 3;
    case KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8   : return 4;
    }
    korl_log(ERROR, "invalid attributeDatatype: %i", attributeDatatype);
    return 0;
}
korl_internal Korl_Gfx_Drawable _korl_gfx_runtimeDrawable(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, Korl_Gfx_Material_PrimitiveType primitiveType, u32 indexCount, Korl_Gfx_VertexAttributeInputRate attributeInputRate, u32 attributeCount)
{
    KORL_ZERO_STACK(Korl_Gfx_Drawable, result);
    result.type                                    = KORL_GFX_DRAWABLE_TYPE_RUNTIME;
    result.subType.runtime.type                    = createInfo->type;
    result.subType.runtime.interleavedAttributes   = createInfo->interleavedAttributes;
    result.transform                               = korl_math_transform3d_identity();
    result.subType.runtime.primitiveType           = primitiveType;
    if(createInfo->vertexIndexType != KORL_GFX_VERTEX_INDEX_TYPE_INVALID)
    {
        result.subType.runtime.vertexStagingMeta.indexType  = createInfo->vertexIndexType;
        result.subType.runtime.vertexStagingMeta.indexCount = indexCount;
    }
    switch(attributeInputRate)
    {
    case KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX  : result.subType.runtime.vertexStagingMeta.vertexCount   = attributeCount; break;
    case KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE: result.subType.runtime.vertexStagingMeta.instanceCount = attributeCount; break;
    }
    u32 byteOffsetBuffer = 0;
    if(createInfo->vertexIndexType != KORL_GFX_VERTEX_INDEX_TYPE_INVALID)
    {
        result.subType.runtime.vertexStagingMeta.indexCount            = indexCount;
        result.subType.runtime.vertexStagingMeta.indexType             = createInfo->vertexIndexType;
        result.subType.runtime.vertexStagingMeta.indexByteOffsetBuffer = byteOffsetBuffer;
        byteOffsetBuffer += indexCount * _korl_gfx_vertexIndexType_byteStride(createInfo->vertexIndexType);
    }
    u32 interleavedByteStride = 0;
    for(u32 a = 0; a < KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT; a++)
    {
        if(createInfo->attributeDatatypes[a] == KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_INVALID)
            continue;
        result.subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[a].byteOffsetBuffer = byteOffsetBuffer;
        result.subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[a].elementType      = _korl_gfx_attributeDatatype_elementType(createInfo->attributeDatatypes[a]);
        result.subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[a].inputRate        = attributeInputRate;
        result.subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[a].vectorSize       = _korl_gfx_attributeDatatype_vectorSize(createInfo->attributeDatatypes[a]);
        if(createInfo->interleavedAttributes)
        {
            interleavedByteStride += _korl_gfx_attributeDatatype_byteStride(createInfo->attributeDatatypes[a]);
            byteOffsetBuffer      += _korl_gfx_attributeDatatype_byteStride(createInfo->attributeDatatypes[a]);
        }
        else
        {
            result.subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[a].byteStride = _korl_gfx_attributeDatatype_byteStride(createInfo->attributeDatatypes[a]);
            byteOffsetBuffer += attributeCount * _korl_gfx_attributeDatatype_byteStride(createInfo->attributeDatatypes[a]);
        }
    }
    if(createInfo->interleavedAttributes)
        // we don't know what value to give attribute byteStrides until we've looped over all possible attributes, 
        //  so for interleaved vertex attributes we need to loop over all the attributes a second time & assign the uniform stride
        for(u32 a = 0; a < KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT; a++)
        {
            if(createInfo->attributeDatatypes[a] == KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_INVALID)
                continue;
            result.subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[a].byteStride = interleavedByteStride;
        }
    switch(createInfo->type)
    {
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
        result.subType.runtime.subType.singleFrame.stagingAllocation = korl_gfx_stagingAllocate(&result.subType.runtime.vertexStagingMeta);
        break;
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:
        KORL_ZERO_STACK(Korl_Resource_GfxBuffer_CreateInfo, createInfoBuffer);
        createInfoBuffer.bytes      = createInfo->interleavedAttributes ? attributeCount * interleavedByteStride : byteOffsetBuffer;
        createInfoBuffer.usageFlags = KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_VERTEX;
        result.subType.runtime.subType.multiFrame.resourceHandleBuffer = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER), &createInfoBuffer, false);
        break;
    }
    return result;
}
korl_internal void korl_gfx_drawable_destroy(Korl_Gfx_Drawable* context)
{
    switch(context->type)
    {
    case KORL_GFX_DRAWABLE_TYPE_INVALID:
        // silently do nothing
        break;
    case KORL_GFX_DRAWABLE_TYPE_RUNTIME:{
        switch(context->subType.runtime.type)
        {
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:{
            // nothing to do here
            break;}
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:{
            korl_resource_destroy(context->subType.runtime.subType.multiFrame.resourceHandleBuffer);
            context->subType.runtime.subType.multiFrame.resourceHandleBuffer = 0;
            break;}
        }
        break;}
    case KORL_GFX_DRAWABLE_TYPE_MESH:{
        if(context->subType.mesh.skin)
            /* the skin is just a single dynamic allocation now, so there's no 
                real need for a special function to destroy it */
            korl_free(context->subType.mesh.skin->allocator, context->subType.mesh.skin);
        break;}
    }
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableLines(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 lineCount)
{
    return _korl_gfx_runtimeDrawable(createInfo, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINES, 0, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, 2 * lineCount);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableLineStrip(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 vertexCount)
{
    korl_assert(vertexCount >= 2);
    return _korl_gfx_runtimeDrawable(createInfo, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINE_STRIP, 0, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, vertexCount);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTriangles(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 triangleCount)
{
    return _korl_gfx_runtimeDrawable(createInfo, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, 0, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, 3 * triangleCount);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTrianglesIndexed(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 triangleCount, u32 vertexCount)
{
    return _korl_gfx_runtimeDrawable(createInfo, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, 3 * triangleCount, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, vertexCount);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTriangleFan(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 vertexCount)
{
    korl_assert(vertexCount >= 3);
    return _korl_gfx_runtimeDrawable(createInfo, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN, 0, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, vertexCount);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableTriangleStrip(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, u32 vertexCount)
{
    korl_assert(vertexCount >= 3);
    return _korl_gfx_runtimeDrawable(createInfo, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_STRIP, 0, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, vertexCount);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableRectangle(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size)
{
    Korl_Gfx_CreateInfoRuntimeDrawable createInfoOverride = *createInfo;
    createInfoOverride.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    Korl_Gfx_Drawable result = _korl_gfx_runtimeDrawable(&createInfoOverride, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_STRIP, 0, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, korl_arraySize(_KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP));
    Korl_Math_V2f32*const positions = korl_gfx_drawable_attributeV2f32(&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    Korl_Math_V2f32*const uvs       = korl_gfx_drawable_attributeV2f32(&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV      , 0);
    for(u32 v = 0; v < korl_arraySize(_KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP); v++)
    {
        positions[v] = korl_math_v2f32_make(_KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP[v].x * size.x - anchorRatio.x * size.x
                                           ,_KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP[v].y * size.y - anchorRatio.y * size.y);
        if(uvs)
            uvs[v] = korl_math_v2f32_make(      _KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP[v].x
                                         ,1.f - _KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP[v].y);
    }
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableCircle(const Korl_Gfx_CreateInfoRuntimeDrawable* createInfo, Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices)
{
    korl_assert(circumferenceVertices >= 3);
    const u32 vertexCount = 1/*center vertex*/ + circumferenceVertices + 1/*repeat circumference start vertex*/;
    Korl_Gfx_CreateInfoRuntimeDrawable createInfoOverride = *createInfo;
    createInfoOverride.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    Korl_Gfx_Drawable result = _korl_gfx_runtimeDrawable(&createInfoOverride, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN, 0, KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX, vertexCount);
    Korl_Math_V2f32*const   positions = korl_gfx_drawable_attributeV2f32(&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    Korl_Math_V2f32*const   uvs       = korl_gfx_drawable_attributeV2f32(&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV      , 0);
    Korl_Gfx_Color4u8*const colors    = korl_gfx_drawable_attributeV4u8 (&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR   , 0);
    positions[0] = korl_math_v2f32_make(radius - anchorRatio.x * 2 * radius
                                       ,radius - anchorRatio.y * 2 * radius);
    if(uvs)
        uvs[0] = korl_math_v2f32_make(0.5f, 0.5f);
    const f32 radiansPerVertex = KORL_TAU32 / KORL_C_CAST(f32, circumferenceVertices);
    for(u32 v = 0; v < circumferenceVertices + 1; v++)
    {
        const Korl_Math_V2f32 spoke = korl_math_v2f32_fromRotationZ(1, KORL_C_CAST(f32, v % circumferenceVertices) * radiansPerVertex);
        positions[1 + v] = korl_math_v2f32_make(radius + radius * spoke.x - anchorRatio.x * 2 * radius
                                               ,radius + radius * spoke.y - anchorRatio.y * 2 * radius);
        if(uvs)
            uvs[1 + v] = korl_math_v2f32_make(       0.5f + 0.5f * spoke.x
                                             ,1.f - (0.5f + 0.5f * spoke.y));
    }
    return result;
}
korl_internal Korl_Gfx_Drawable _korl_gfx_immediateUtf(Korl_Gfx_Drawable_Runtime_Type type, Korl_Math_V2f32 anchorRatio, const void* utfText, const u8 utfTextEncoding, u$ utfTextSize, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    /* determine how many glyphs need to be drawn from utf8Text, as well as the 
        AABB size of the text so we can calculate position offset based on anchorRatio */
    Korl_Resource_Font_TextMetrics textMetrics;
    switch(utfTextEncoding)
    {
    case  8: textMetrics = korl_resource_font_getUtf8Metrics (resourceHandleFont, textPixelHeight, KORL_STRUCT_INITIALIZE(acu8 ){.size = utfTextSize, .data = KORL_C_CAST(const u8* , utfText)}); break;
    case 16: textMetrics = korl_resource_font_getUtf16Metrics(resourceHandleFont, textPixelHeight, KORL_STRUCT_INITIALIZE(acu16){.size = utfTextSize, .data = KORL_C_CAST(const u16*, utfText)}); break;
    default:
        textMetrics = KORL_STRUCT_INITIALIZE_ZERO(Korl_Resource_Font_TextMetrics);
        korl_log(ERROR, "unsupported UTF encoding: %hhu", utfTextEncoding);
        break;
    }
    if(o_textMetrics)
        *o_textMetrics = textMetrics;
    /* create the Drawable & its backing memory allocations */
    /* we don't use `korl_gfx_drawableTriangles` because that API doesn't support vertex indices; 
        `korl_gfx_drawableTriangles` doesn't support vertex indices because 
        that requires knowledge of triangle topology, which I'm not quite sure 
        how to translate into a general API (yet at least) */
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type            = type;
    createInfoDrawable.vertexIndexType = KORL_GFX_VERTEX_INDEX_TYPE_U16;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0 ] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_U32;
    Korl_Gfx_Drawable result = _korl_gfx_runtimeDrawable(&createInfoDrawable, KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, korl_arraySize(_KORL_UTILITY_GFX_TRI_QUAD_INDICES), KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE, textMetrics.visibleGlyphCount);
    u16*const             indices                = korl_gfx_drawable_indexU16      (&result, 0);
    Korl_Math_V2f32*const glyphInstancePositions = korl_gfx_drawable_attributeV2f32(&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    u32*const             glyphIndices           = korl_gfx_drawable_attributeU32  (&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0, 0);
    /* generate the text vertex data */
    korl_memory_copy(indices, _KORL_UTILITY_GFX_TRI_QUAD_INDICES, sizeof(_KORL_UTILITY_GFX_TRI_QUAD_INDICES));
    switch(utfTextEncoding)
    {
    case  8:
        korl_resource_font_generateUtf8(resourceHandleFont, textPixelHeight, KORL_STRUCT_INITIALIZE(acu8){.size = utfTextSize, .data = KORL_C_CAST(const u8*, utfText)}
                                       ,korl_math_v2f32_make(-anchorRatio.x * textMetrics.aabbSize.x
                                                            ,-anchorRatio.y * textMetrics.aabbSize.y + textMetrics.aabbSize.y)
                                       ,glyphInstancePositions, sizeof(*glyphInstancePositions), glyphIndices, sizeof(*glyphIndices));
        break;
    case 16:
        korl_resource_font_generateUtf16(resourceHandleFont, textPixelHeight, KORL_STRUCT_INITIALIZE(acu16){.size = utfTextSize, .data = KORL_C_CAST(const u16*, utfText)}
                                        ,korl_math_v2f32_make(-anchorRatio.x * textMetrics.aabbSize.x
                                                             ,-anchorRatio.y * textMetrics.aabbSize.y + textMetrics.aabbSize.y)
                                        ,glyphInstancePositions, sizeof(*glyphInstancePositions), glyphIndices, sizeof(*glyphIndices));
        break;
    }
    /* setup text-specific material/draw-state overrides */
    if(!korl_resource_isLoaded(resourceHandleFont))
        switch(type)
        {
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
            return result;
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:
            korl_log(ERROR, "malformed multi-frame drawable; font resource was not loaded on construction");
            break;
        }
    const Korl_Resource_Font_Metrics fontMetrics = korl_resource_font_getMetrics(resourceHandleFont, textPixelHeight);
    const f32 sizeToSupportedSizeRatio = textPixelHeight / fontMetrics.nearestSupportedPixelHeight;// korl-resource-text will only bake glyphs at discrete pixel heights (for complexity/memory reasons); it is up to us to scale the final text mesh by the appropriate ratio to actually draw text at `textPixelHeight`
    korl_math_transform3d_setScale(&result.transform, korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_ONE, sizeToSupportedSizeRatio));
    const Korl_Resource_Font_Resources fontResources = korl_resource_font_getResources(resourceHandleFont, textPixelHeight);
    result.subType.runtime.overrides.shaderVertex        = korl_gfx_getBuiltInShaderVertex(&result.subType.runtime.vertexStagingMeta);
    result.subType.runtime.overrides.storageBufferVertex = fontResources.resourceHandleSsboGlyphMeshVertices;
    result.subType.runtime.overrides.materialMapBase     = fontResources.resourceHandleTexture;
    /**/
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableUtf8(Korl_Gfx_Drawable_Runtime_Type type, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    return _korl_gfx_immediateUtf(type, anchorRatio, utf8Text.data, 8, utf8Text.size, resourceHandleFont, textPixelHeight, o_textMetrics);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableUtf16(Korl_Gfx_Drawable_Runtime_Type type, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    return _korl_gfx_immediateUtf(type, anchorRatio, utf16Text.data, 16, utf16Text.size, resourceHandleFont, textPixelHeight, o_textMetrics);
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawableAxisNormalLines(Korl_Gfx_Drawable_Runtime_Type type)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = type;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR   ] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    Korl_Gfx_Drawable result = korl_gfx_drawableLines(&createInfoDrawable, 3);
    result.subType.runtime.overrides.materialModeFlags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE 
                                                        | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST;
    Korl_Math_V3f32*const   positions = korl_gfx_drawable_attributeV3f32(&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    Korl_Gfx_Color4u8*const colors    = korl_gfx_drawable_attributeV4u8 (&result, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR   , 0);
    positions[0] = KORL_MATH_V3F32_ZERO; positions[1] = KORL_MATH_V3F32_X;
    positions[2] = KORL_MATH_V3F32_ZERO; positions[3] = KORL_MATH_V3F32_Y;
    positions[4] = KORL_MATH_V3F32_ZERO; positions[5] = KORL_MATH_V3F32_Z;
    colors[0] = colors[1] = KORL_COLOR4U8_RED;
    colors[2] = colors[3] = KORL_COLOR4U8_GREEN;
    colors[4] = colors[5] = KORL_COLOR4U8_BLUE;
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_mesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName)
{
    KORL_ZERO_STACK(Korl_Gfx_Drawable, result);
    result.type                               = KORL_GFX_DRAWABLE_TYPE_MESH;
    result.transform                          = korl_math_transform3d_identity();
    result.subType.mesh.resourceHandleScene3d = resourceHandleScene3d;
    korl_assert(utf8MeshName.size < korl_arraySize(result.subType.mesh.rawUtf8Scene3dMeshName));
    korl_memory_copy(result.subType.mesh.rawUtf8Scene3dMeshName, utf8MeshName.data, utf8MeshName.size);
    result.subType.mesh.rawUtf8Scene3dMeshName[utf8MeshName.size] = '\0';
    result.subType.mesh.rawUtf8Scene3dMeshNameSize                = korl_checkCast_u$_to_u8(utf8MeshName.size);
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_meshIndexed(Korl_Resource_Handle resourceHandleScene3d, u32 meshIndex)
{
    KORL_ZERO_STACK(Korl_Gfx_Drawable, result);
    result.type                               = KORL_GFX_DRAWABLE_TYPE_MESH;
    result.transform                          = korl_math_transform3d_identity();
    result.subType.mesh.resourceHandleScene3d = resourceHandleScene3d;
    result.subType.mesh.meshIndex             = meshIndex;
    return result;
}
korl_internal void korl_gfx_drawable_addInterleavedVertices(Korl_Gfx_Drawable*const context, u32 vertexCount)
{
    korl_assert(context->type == KORL_GFX_DRAWABLE_TYPE_RUNTIME);
    korl_assert(context->subType.runtime.interleavedAttributes);// reallocation of disjoint attributes yields such bad performance, that I don't think we should ever support that functionality; see repository history in this code module for that old implementation if you're really curious, but just "trust me bro"™: create your RUNTIME Drawable with interleavedAttributes == true if you ever hit this assert
    context->subType.runtime.vertexStagingMeta.vertexCount += vertexCount;
    switch(context->subType.runtime.type)
    {
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
        context->subType.runtime.subType.singleFrame.stagingAllocation = korl_gfx_stagingReallocate(&context->subType.runtime.vertexStagingMeta, &context->subType.runtime.subType.singleFrame.stagingAllocation);
        break;
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:
        const u$ newBufferBytes = context->subType.runtime.vertexStagingMeta.indexCount  * _korl_gfx_vertexIndexType_byteStride(context->subType.runtime.vertexStagingMeta.indexType)
                                + context->subType.runtime.vertexStagingMeta.vertexCount * context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
        korl_resource_resize(context->subType.runtime.subType.multiFrame.resourceHandleBuffer, newBufferBytes);
        break;
    }
}
korl_internal void* _korl_gfx_drawable_index(const Korl_Gfx_Drawable*const context, u32 indicesIndex)
{
    void* updateBuffer = NULL;
    switch(context->subType.runtime.type)
    {
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
        updateBuffer = context->subType.runtime.subType.singleFrame.stagingAllocation.buffer;
        break;
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:
        u$ bytesRequested_bytesAvailable = KORL_U$_MAX;
        updateBuffer = korl_resource_getUpdateBuffer(context->subType.runtime.subType.multiFrame.resourceHandleBuffer, 0, &bytesRequested_bytesAvailable);
        break;
    }
    return KORL_C_CAST(void*, KORL_C_CAST(u8*, updateBuffer) 
                              + context->subType.runtime.vertexStagingMeta.indexByteOffsetBuffer
                              + _korl_gfx_vertexIndexType_byteStride(context->subType.runtime.vertexStagingMeta.indexType) * indicesIndex);
}
korl_internal void* _korl_gfx_drawable_attribute(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex)
{
    void* updateBuffer = NULL;
    switch(context->subType.runtime.type)
    {
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
        updateBuffer = context->subType.runtime.subType.singleFrame.stagingAllocation.buffer;
        break;
    case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:
        u$ bytesRequested_bytesAvailable = KORL_U$_MAX;
        updateBuffer = korl_resource_getUpdateBuffer(context->subType.runtime.subType.multiFrame.resourceHandleBuffer, 0, &bytesRequested_bytesAvailable);
        break;
    }
    return KORL_C_CAST(void*, KORL_C_CAST(u8*, updateBuffer) 
                              + context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].byteOffsetBuffer
                              + context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].byteStride * attributeIndex);
}
korl_internal u16* korl_gfx_drawable_indexU16(const Korl_Gfx_Drawable*const context, u32 indicesIndex)
{
    if(context->subType.runtime.vertexStagingMeta.indexType == KORL_GFX_VERTEX_INDEX_TYPE_INVALID)
        return NULL;
    korl_assert(context->subType.runtime.vertexStagingMeta.indexType == KORL_GFX_VERTEX_INDEX_TYPE_U16);
    return KORL_C_CAST(u16*, _korl_gfx_drawable_index(context, indicesIndex));
}
korl_internal u32* korl_gfx_drawable_indexU32(const Korl_Gfx_Drawable*const context, u32 indicesIndex)
{
    if(context->subType.runtime.vertexStagingMeta.indexType == KORL_GFX_VERTEX_INDEX_TYPE_INVALID)
        return NULL;
    korl_assert(context->subType.runtime.vertexStagingMeta.indexType == KORL_GFX_VERTEX_INDEX_TYPE_U32);
    return KORL_C_CAST(u32*, _korl_gfx_drawable_index(context, indicesIndex));
}
korl_internal u32* korl_gfx_drawable_attributeU32(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex)
{
    if(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
        return NULL;
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32);
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize  == 1);
    return KORL_C_CAST(u32*, _korl_gfx_drawable_attribute(context, binding, attributeIndex));
}
korl_internal Korl_Math_V2f32* korl_gfx_drawable_attributeV2f32(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex)
{
    if(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
        return NULL;
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32);
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize  == 2);
    return KORL_C_CAST(Korl_Math_V2f32*, _korl_gfx_drawable_attribute(context, binding, attributeIndex));
}
korl_internal Korl_Math_V3f32* korl_gfx_drawable_attributeV3f32(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex)
{
    if(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
        return NULL;
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32);
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize  == 3);
    return KORL_C_CAST(Korl_Math_V3f32*, _korl_gfx_drawable_attribute(context, binding, attributeIndex));
}
korl_internal Korl_Math_V4u8* korl_gfx_drawable_attributeV4u8(const Korl_Gfx_Drawable*const context, Korl_Gfx_VertexAttributeBinding binding, u32 attributeIndex)
{
    if(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
        return NULL;
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8);
    korl_assert(context->subType.runtime.vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize  == 4);
    return KORL_C_CAST(Korl_Math_V4u8*, _korl_gfx_drawable_attribute(context, binding, attributeIndex));
}
korl_internal Korl_Gfx_Drawable korl_gfx_drawSphere(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, f32 radius, u32 latitudeSegments, u32 longitudeSegments, const Korl_Gfx_Material* material)
{
    const bool generateUvs = material && material->maps.resourceHandleTextureBase;
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    if(generateUvs)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableTriangles(&createInfoDrawable, korl_checkCast_u$_to_u32(korl_math_generateMeshSphereVertexCount(latitudeSegments, longitudeSegments) / 3));
    immediate.subType.runtime.overrides.materialModeFlags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE 
                                                           | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST;
    Korl_Math_V3f32*const positions = korl_gfx_drawable_attributeV3f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    Korl_Math_V2f32*const uvs       = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV      , 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_math_generateMeshSphere(radius, latitudeSegments, longitudeSegments, positions, sizeof(*positions), uvs, sizeof(*uvs));
    korl_gfx_draw(&immediate, material, 1);
    return immediate;
}
korl_internal void _korl_gfx_drawRectangle(Korl_Gfx_Material_Mode_Flags materialModeFlagOverrides, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    korl_shared_const Korl_Math_V2f32 QUAD_POSITION_NORMALS_LOOP[4] = {{{0,0}}, {{1,0}}, {{1,1}}, {{0,1}}};
    if(material)
    {
        const bool generateUvs = o_uvs || 0 != material->maps.resourceHandleTextureBase;
        KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
        createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
        if(generateUvs)
            createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
        if(o_colors)
            createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
        Korl_Gfx_Drawable immediate = korl_gfx_drawableRectangle(&createInfoDrawable, anchorRatio, size);
        if(o_uvs)
            *o_uvs = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV, 0);
        if(o_colors)
            *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
        immediate.transform                                   = korl_math_transform3d_rotateTranslate(versor, position);
        immediate.subType.runtime.overrides.materialModeFlags |= materialModeFlagOverrides;
        korl_gfx_draw(&immediate, material, 1);
    }
    if(materialOutline)
    {
        KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
        createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
        Korl_Gfx_Drawable immediateOutline = outlineThickness == 0 
                                             ? korl_gfx_drawableLineStrip    (&createInfoDrawable,      korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1 )
                                             : korl_gfx_drawableTriangleStrip(&createInfoDrawable, 2 * (korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1));
        Korl_Math_V2f32*const outlinePositions = korl_gfx_drawable_attributeV2f32(&immediateOutline, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
        immediateOutline.transform                                   = korl_math_transform3d_rotateTranslate(versor, position);
        immediateOutline.subType.runtime.overrides.materialModeFlags |= materialModeFlagOverrides;
        const Korl_Math_V2f32 centerOfMass = korl_math_v2f32_subtract(korl_math_v2f32_multiplyScalar(size, 0.5f), korl_math_v2f32_multiply(anchorRatio, size));
        for(u32 v = 0; v < korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1; v++)
        {
            const u32 cornerIndex = v % korl_arraySize(QUAD_POSITION_NORMALS_LOOP);
            if(outlineThickness == 0)
                outlinePositions[v] = korl_math_v2f32_make(QUAD_POSITION_NORMALS_LOOP[cornerIndex].x * size.x - anchorRatio.x * size.x
                                                          ,QUAD_POSITION_NORMALS_LOOP[cornerIndex].y * size.y - anchorRatio.y * size.y);
            else
            {
                outlinePositions[2 * v + 0] = korl_math_v2f32_make(QUAD_POSITION_NORMALS_LOOP[cornerIndex].x * size.x - anchorRatio.x * size.x
                                                                  ,QUAD_POSITION_NORMALS_LOOP[cornerIndex].y * size.y - anchorRatio.y * size.y);
                const Korl_Math_V2f32 fromCenter      = korl_math_v2f32_subtract(outlinePositions[2 * v + 0], centerOfMass);
                const Korl_Math_V2f32 fromCenterNorms = korl_math_v2f32_make(fromCenter.x / korl_math_f32_positive(fromCenter.x)
                                                                            ,fromCenter.y / korl_math_f32_positive(fromCenter.y));
                outlinePositions[2 * v + 1] = korl_math_v2f32_add(outlinePositions[2 * v + 0], korl_math_v2f32_multiplyScalar(fromCenterNorms, outlineThickness));
            }
        }
        korl_gfx_draw(&immediateOutline, materialOutline, 1);
    }
}
korl_internal void korl_gfx_drawRectangle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* materialInner, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_innerColors, Korl_Math_V2f32** o_innerUvs)
{
    _korl_gfx_drawRectangle(KORL_GFX_MATERIAL_MODE_FLAGS_NONE, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, size, outlineThickness, materialInner, materialOutline, o_innerColors, o_innerUvs);
}
korl_internal void korl_gfx_drawRectangle3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* materialInner, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_innerColors, Korl_Math_V2f32** o_innerUvs)
{
    _korl_gfx_drawRectangle(KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE, position, versor, anchorRatio, size, outlineThickness, materialInner, materialOutline, o_innerColors, o_innerUvs);
}
korl_internal void _korl_gfx_drawCircle(Korl_Gfx_Material_Mode_Flags materialModeFlagOverrides, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors)
{
    if(material)
    {
        const bool generateUvs = 0 != material->maps.resourceHandleTextureBase;
        KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
        createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
        if(generateUvs)
            createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
        if(o_colors)
            createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
        Korl_Gfx_Drawable immediate = korl_gfx_drawableCircle(&createInfoDrawable, anchorRatio, radius, circumferenceVertices);
        // if(o_uvs)
        //     *o_uvs = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV, 0);
        if(o_colors)
            *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
        immediate.transform                                   = korl_math_transform3d_rotateTranslate(versor, position);
        immediate.subType.runtime.overrides.materialModeFlags |= materialModeFlagOverrides;
        korl_gfx_draw(&immediate, material, 1);
    }
    if(materialOutline)
    {
        KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
        createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
        Korl_Gfx_Drawable immediateOutline = outlineThickness == 0 
                                             ? korl_gfx_drawableLineStrip    (&createInfoDrawable,      circumferenceVertices + 1 )
                                             : korl_gfx_drawableTriangleStrip(&createInfoDrawable, 2 * (circumferenceVertices + 1));
        Korl_Math_V2f32*const outlinePositions = korl_gfx_drawable_attributeV2f32(&immediateOutline, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
        immediateOutline.transform                                   = korl_math_transform3d_rotateTranslate(versor, position);
        immediateOutline.subType.runtime.overrides.materialModeFlags |= materialModeFlagOverrides;
        const f32 radiansPerVertex = KORL_TAU32 / KORL_C_CAST(f32, circumferenceVertices);
        for(u32 v = 0; v < circumferenceVertices + 1; v++)
        {
            const u32             vMod    = v % circumferenceVertices;
            const f32             radians = KORL_C_CAST(f32, vMod) * radiansPerVertex;
            const Korl_Math_V2f32 spoke   = korl_math_v2f32_fromRotationZ(1, radians);
            if(outlineThickness == 0)
                outlinePositions[v] = korl_math_v2f32_make(radius + radius * spoke.x - anchorRatio.x * 2 * radius
                                                          ,radius + radius * spoke.y - anchorRatio.y * 2 * radius);
            else
            {
                outlinePositions[2 * v + 0] = korl_math_v2f32_make(radius +   radius                     * spoke.x  - anchorRatio.x * 2 * radius
                                                                  ,radius +   radius                     * spoke.y  - anchorRatio.y * 2 * radius);
                outlinePositions[2 * v + 1] = korl_math_v2f32_make(radius + ((radius + outlineThickness) * spoke.x) - anchorRatio.x * 2 * radius
                                                                  ,radius + ((radius + outlineThickness) * spoke.y) - anchorRatio.y * 2 * radius);
            }
        }
        korl_gfx_draw(&immediateOutline, materialOutline, 1);
    }
}
korl_internal void korl_gfx_drawCircle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices, f32 outlineThickness, const Korl_Gfx_Material* materialInner, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_innerColors)
{
    _korl_gfx_drawCircle(KORL_GFX_MATERIAL_MODE_FLAGS_NONE, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, radius, circumferenceVertices, outlineThickness, materialInner, materialOutline, o_innerColors);
}
korl_internal void korl_gfx_drawLines2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableLines(&createInfoDrawable, lineCount);
    *o_positions = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawLines3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableLines(&createInfoDrawable, lineCount);
    *o_positions = korl_gfx_drawable_attributeV3f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    immediate.subType.runtime.overrides.materialModeFlags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                                           | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE;
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawLineStrip2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableLineStrip(&createInfoDrawable, vertexCount);
    *o_positions = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawLineStrip3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableLineStrip(&createInfoDrawable, vertexCount);
    *o_positions = korl_gfx_drawable_attributeV3f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTriangles2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    if(o_uvs)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableTriangles(&createInfoDrawable, triangleCount);
    *o_positions = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    if(o_uvs)
        *o_uvs = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTriangles3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    if(o_uvs)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableTriangles(&createInfoDrawable, triangleCount);
    *o_positions = korl_gfx_drawable_attributeV3f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    if(o_uvs)
        *o_uvs = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTrianglesIndexed3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 triangleCount, u32 vertexCount, const Korl_Gfx_Material* material, u32** o_indices, Korl_Math_V3f32** o_positions, Korl_Math_V3f32** o_normals, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type                                                           = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.vertexIndexType                                                = KORL_GFX_VERTEX_INDEX_TYPE_U32;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    if(o_normals)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_NORMAL] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    if(o_uvs)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableTrianglesIndexed(&createInfoDrawable, triangleCount, vertexCount);
    *o_indices   = korl_gfx_drawable_indexU32(&immediate, 0);
    *o_positions = korl_gfx_drawable_attributeV3f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_normals)
        *o_normals = korl_gfx_drawable_attributeV3f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_NORMAL, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    if(o_uvs)
        *o_uvs = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTriangleFan2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V2F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableTriangleFan(&createInfoDrawable, vertexCount);
    *o_positions = korl_gfx_drawable_attributeV2f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTriangleFan3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_CreateInfoRuntimeDrawable, createInfoDrawable);
    createInfoDrawable.type = KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME;
    createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V3F32;
    if(o_colors)
        createInfoDrawable.attributeDatatypes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR] = KORL_GFX_RUNTIME_DRAWABLE_ATTRIBUTE_DATATYPE_V4U8;
    Korl_Gfx_Drawable immediate = korl_gfx_drawableTriangleFan(&createInfoDrawable, vertexCount);
    *o_positions = korl_gfx_drawable_attributeV3f32(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION, 0);
    if(o_colors)
        *o_colors = korl_gfx_drawable_attributeV4u8(&immediate, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR, 0);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    immediate.subType.runtime.overrides.materialModeFlags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                                           | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE;
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void _korl_gfx_drawUtf(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, const void* utfText, const u8 utfTextEncoding, u$ utfTextSize, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, bool enableDepthTest, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    Korl_Gfx_Material materialOverride;// the base color map of the material will always be set to the translucency mask texture containing the baked rasterized glyphs, so we need to override the material
    if(material)
        materialOverride = *material;
    else
        materialOverride = korl_gfx_material_defaultUnlit();
    materialOverride.modes.flags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_BLEND;// _all_ text drawn this way _must_ be translucent
    if(enableDepthTest)
        materialOverride.modes.flags |=   KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                                        | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE;
    Korl_Gfx_Drawable text = _korl_gfx_immediateUtf(KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME, anchorRatio, utfText, utfTextEncoding, utfTextSize, resourceHandleFont, textPixelHeight, o_textMetrics);
    text.transform = korl_math_transform3d_rotateScaleTranslate(versor, text.transform._scale, position);
    korl_gfx_draw(&text, &materialOverride, 1);
}
korl_internal void korl_gfx_drawUtf82d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    _korl_gfx_drawUtf(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, utf8Text.data, 8, utf8Text.size, resourceHandleFont, textPixelHeight, outlineSize, material, materialOutline, false/* if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway*/, o_textMetrics);
}
korl_internal void korl_gfx_drawUtf83d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    _korl_gfx_drawUtf(position, versor, anchorRatio, utf8Text.data, 8, utf8Text.size, resourceHandleFont, textPixelHeight, outlineSize, material, materialOutline, true, o_textMetrics);
}
korl_internal void korl_gfx_drawUtf162d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    _korl_gfx_drawUtf(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, utf16Text.data, 16, utf16Text.size, resourceHandleFont, textPixelHeight, outlineSize, material, materialOutline, false/* if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway*/, o_textMetrics);
}
korl_internal void korl_gfx_drawUtf163d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, Korl_Resource_Handle resourceHandleFont, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Resource_Font_TextMetrics* o_textMetrics)
{
    _korl_gfx_drawUtf(position, versor, anchorRatio, utf16Text.data, 16, utf16Text.size, resourceHandleFont, textPixelHeight, outlineSize, material, materialOutline, true, o_textMetrics);
}
korl_internal void korl_gfx_drawAabb3(Korl_Math_Aabb3f32 aabb, const Korl_Gfx_Material* material)
{
    const Korl_Math_V3f32 aabbSize = korl_math_aabb3f32_size(aabb);
    Korl_Math_V3f32* vertexPositions;
    korl_gfx_drawLines3d(KORL_MATH_V3F32_ZERO, KORL_MATH_QUATERNION_IDENTITY, 12, material, &vertexPositions, NULL);
    /* draw a 2D box intersecting the minimum point */
    vertexPositions[0] = aabb.min;
    vertexPositions[1] = korl_math_v3f32_make(aabb.min.x + aabbSize.x, aabb.min.y, aabb.min.z);
    vertexPositions[2] = korl_math_v3f32_make(aabb.min.x + aabbSize.x, aabb.min.y, aabb.min.z);
    vertexPositions[3] = korl_math_v3f32_make(aabb.min.x + aabbSize.x, aabb.min.y + aabbSize.y, aabb.min.z);
    vertexPositions[4] = korl_math_v3f32_make(aabb.min.x + aabbSize.x, aabb.min.y + aabbSize.y, aabb.min.z);
    vertexPositions[5] = korl_math_v3f32_make(aabb.min.x, aabb.min.y + aabbSize.y, aabb.min.z);
    vertexPositions[6] = korl_math_v3f32_make(aabb.min.x, aabb.min.y + aabbSize.y, aabb.min.z);
    vertexPositions[7] = aabb.min;
    /* draw a 2D box intersecting the maximum point */
    for(u8 i = 0; i < 8; i++)
    {
        vertexPositions[8 + i]    = vertexPositions[i];
        vertexPositions[8 + i].z += aabbSize.z;
    }
    /* draw 4 lines connecting the two boxes */
    for(u8 i = 0; i < 4; i++)
    {
        vertexPositions[16 + 2 * i + 0] = vertexPositions[2 * i];
        vertexPositions[16 + 2 * i + 1] = vertexPositions[2 * i + 8];
    }
}
korl_internal void korl_gfx_drawMesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material *materials, u8 materialsSize, Korl_Resource_Scene3d_Skin* skin)
{
    Korl_Gfx_Drawable mesh = korl_gfx_mesh(resourceHandleScene3d, utf8MeshName);
    mesh.transform         = korl_math_transform3d_rotateScaleTranslate(versor, scale, position);
    mesh.subType.mesh.skin = skin;
    korl_gfx_draw(&mesh, materials, materialsSize);
}
korl_internal void korl_gfx_drawMeshIndexed(Korl_Resource_Handle resourceHandleScene3d, u32 meshIndex, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material *materials, u8 materialsSize, Korl_Resource_Scene3d_Skin* skin)
{
    Korl_Gfx_Drawable mesh = korl_gfx_meshIndexed(resourceHandleScene3d, meshIndex);
    mesh.transform         = korl_math_transform3d_rotateScaleTranslate(versor, scale, position);
    mesh.subType.mesh.skin = skin;
    korl_gfx_draw(&mesh, materials, materialsSize);
}
korl_internal void korl_gfx_drawAxisNormalLines(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale)
{
    Korl_Gfx_Drawable drawable = korl_gfx_drawableAxisNormalLines(KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME);
    drawable.transform = korl_math_transform3d_rotateScaleTranslate(versor, scale, position);
    Korl_Gfx_Material material = korl_gfx_material_defaultUnlit();
    korl_gfx_draw(&drawable, &material, 1);
}
korl_internal void korl_gfx_drawTriangleMesh(Korl_Math_TriangleMesh* context, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, const Korl_Gfx_Material *material)
{
    if(arrlenu(context->stbDaIndices))
        korl_assert(arrlenu(context->stbDaIndices) % 3 == 0);
    u32*             indices   = NULL;
    Korl_Math_V3f32* positions = NULL;
    Korl_Math_V3f32* normals   = NULL;
    Korl_Math_V2f32* uvs       = NULL;
    korl_gfx_drawTrianglesIndexed3d(position, versor, korl_checkCast_u$_to_u32(arrlenu(context->stbDaIndices) / 3), korl_checkCast_u$_to_u32(arrlenu(context->stbDaVertices)), material, &indices, &positions, &normals, NULL, &uvs);
    korl_memory_copy(indices  , context->stbDaIndices , arrlenu(context->stbDaIndices)  * sizeof(*indices));
    korl_memory_copy(positions, context->stbDaVertices, arrlenu(context->stbDaVertices) * sizeof(*positions));
    korl_memory_zero(uvs, arrlenu(context->stbDaVertices) * sizeof(*uvs));
    const u32*const indicesEnd = context->stbDaIndices + arrlen(context->stbDaIndices);
    for(const u32* index = context->stbDaIndices; index < indicesEnd; index += 3)
    {
        const Korl_Math_V3f32 normal = korl_math_v3f32_normal(korl_math_v3f32_cross(korl_math_v3f32_subtract(context->stbDaVertices[index[1]], context->stbDaVertices[index[0]])
                                                                                   ,korl_math_v3f32_subtract(context->stbDaVertices[index[2]], context->stbDaVertices[index[1]])));
        normals[index[0]] = normal;
        normals[index[1]] = normal;
        normals[index[2]] = normal;
    }
}
korl_internal void korl_gfx_setRectangleUvAabb(Korl_Math_V2f32* uvs, const Korl_Math_Aabb2f32 aabb)
{
    const Korl_Math_V2f32 aabbSize = korl_math_aabb2f32_size(aabb);
    for(u8 i = 0; i < korl_arraySize(_KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP); i++)
        uvs[i] = korl_math_v2f32_add(aabb.min, korl_math_v2f32_multiply(_KORL_UTILITY_GFX_QUAD_POSITION_NORMALS_TRI_STRIP[i], aabbSize));
}
