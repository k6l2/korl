#include "korl-gui.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-gfx.h"
#include "korl-gui-internal-common.h"
#include "korl-time.h"
#include "korl-stb-ds.h"
typedef struct _Korl_Gui_UsedWidget
{
    _Korl_Gui_Widget* widget;
    // @TODO: add transient DAG meta-data, such as childCount, rootParent, etc...
} _Korl_Gui_UsedWidget;
#define SORT_NAME _korl_gui_usedWidget
#define SORT_TYPE _Korl_Gui_UsedWidget
#define SORT_CMP(x, y) ((x).widget->orderIndex < (y).widget->orderIndex ? -1 : ((x).widget->orderIndex > (y).widget->orderIndex ? 1 : 0))
#ifndef SORT_CHECK_CAST_INT_TO_SIZET
    #define SORT_CHECK_CAST_INT_TO_SIZET(x) korl_checkCast_i$_to_u$(x)
#endif
#ifndef SORT_CHECK_CAST_SIZET_TO_INT
    #define SORT_CHECK_CAST_SIZET_TO_INT(x) korl_checkCast_u$_to_i32(x)
#endif
#include "sort.h"
#if KORL_DEBUG
    // #define _KORL_GUI_DEBUG_DRAW_COORDINATE_FRAMES
#endif
#if defined(_LOCAL_STRING_POOL_POINTER)
    #undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&_korl_gui_context.stringPool)
korl_shared_const wchar_t _KORL_GUI_ORPHAN_WIDGET_WINDOW_TITLE_BAR_TEXT[] = L"DEBUG";
korl_shared_const u64     _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH          = KORL_U64_MAX;
#if 0//@TODO: recycle
#define SORT_NAME _korl_gui_widget
#define SORT_TYPE _Korl_Gui_Widget
#define SORT_CMP(x, y) ((x).orderIndex < (y).orderIndex ? -1 : ((x).orderIndex > (y).orderIndex ? 1 : 0))
#ifndef SORT_CHECK_CAST_INT_TO_SIZET
    #define SORT_CHECK_CAST_INT_TO_SIZET(x) korl_checkCast_i$_to_u$(x)
#endif
#ifndef SORT_CHECK_CAST_SIZET_TO_INT
    #define SORT_CHECK_CAST_SIZET_TO_INT(x) korl_checkCast_u$_to_i32(x)
#endif
#include "sort.h"
typedef struct _Korl_Gui_CodepointTestData_Log
{
    u8 trailingMetaTagCodepoints;
    u8 metaTagComponent;// 0=>log level, 1=>time stamp, 2=>line, 3=>file, 4=>function
    const u16* pCodepointMetaTagStart;
} _Korl_Gui_CodepointTestData_Log;
korl_internal KORL_GFX_TEXT_CODEPOINT_TEST(_korl_gui_codepointTest_log)
{
    // log meta data example:
    //╟INFO   ┆11:48'00"525┆   58┆korl-vulkan.c┆_korl_vulkan_debugUtilsMessengerCallback╢ 
    _Korl_Gui_CodepointTestData_Log*const data = KORL_C_CAST(_Korl_Gui_CodepointTestData_Log*, userData);
    if(data->pCodepointMetaTagStart)
    {
        bool endOfMetaTagComponent = false;
        if(*pCodepoint == L'╢')
        {
            data->trailingMetaTagCodepoints = 2;
            data->pCodepointMetaTagStart    = NULL;
            endOfMetaTagComponent = true;
        }
        else if(*pCodepoint == L'┆')
            endOfMetaTagComponent = true;
        if(endOfMetaTagComponent)
        {
            switch(data->metaTagComponent)
            {
            case 0:{// log level
                korl_assert(currentLineColor);
                switch(*data->pCodepointMetaTagStart)
                {
                case L'A':{// ASSERT
                    currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){1, 0, 0};// red
                    break;}
                case L'E':{// ERROR
                    currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){1, 0, 0};// red
                    break;}
                case L'W':{// WARNING
                    currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){1, 1, 0};// yellow
                    break;}
                case L'I':{// INFO
                    // do nothing; the line color defaults to white!
                    break;}
                case L'V':{// VERBOSE
                    currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){0, 1, 1};// cyan
                    break;}
                }
                break;}
            case 1:{// time
                break;}
            case 2:{// line #
                break;}
            case 3:{// file
                break;}
            case 4:{// function
                break;}
            }
            data->metaTagComponent++;
        }
    }
    else if(*pCodepoint == L'╟')
    {
        data->pCodepointMetaTagStart = pCodepoint + 1;
        data->metaTagComponent       = 0;
    }
    if(data->trailingMetaTagCodepoints)
        data->trailingMetaTagCodepoints--;
    if(data->pCodepointMetaTagStart || data->trailingMetaTagCodepoints)
        return false;
    return true;
}
korl_internal _Korl_Gui_Widget* _korl_gui_getWidget(u64 identifierHash, u$ widgetType, bool* out_newAllocation)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    /* combine the widget-specific identifierHash with the context loop index */
    u64 identifierHashComponents[2];
    identifierHashComponents[0] = identifierHash;
    identifierHashComponents[1] = context->loopIndex;
    const u64 identifierHashPrime = korl_memory_acu16_hash(KORL_STRUCT_INITIALIZE(acu16){.data = KORL_C_CAST(u16*, &(identifierHashComponents[0]))
                                                                                        ,.size = sizeof(identifierHashComponents) / sizeof(u16)});
    /* if there is no current active window, then we should just default to use 
        an internal "debug" window to allow the user to just create widgets at 
        any time without worrying about creating a window first */
    if(context->currentWindowIndex < 0)
        korl_gui_windowBegin(_KORL_GUI_ORPHAN_WIDGET_WINDOW_ID, NULL, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
    /* check to see if this widget's identifier set is already registered */
    u$ widgetIndex = arrcap(context->stbDaWidgets);
    for(u$ w = 0; w < arrlenu(context->stbDaWidgets); ++w)
    {
        if(    context->stbDaWidgets[w].parentWindowIdentifierHash == context->stbDaWindows[context->currentWindowIndex].identifierHash
            && context->stbDaWidgets[w].identifierHash             == identifierHashPrime)
        {
            widgetIndex = w;
            *out_newAllocation = false;
            goto widgetIndexValid;
        }
    }
    /* otherwise, allocate a new widget */
    widgetIndex = arrlenu(context->stbDaWidgets);
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaWidgets, (_Korl_Gui_Widget){0});
    _Korl_Gui_Widget* widget = &context->stbDaWidgets[widgetIndex];
    korl_memory_zero(widget, sizeof(*widget));
    widget->identifierHash             = identifierHashPrime;
    widget->parentWindowIdentifierHash = context->stbDaWindows[context->currentWindowIndex].identifierHash;
    widget->type                       = widgetType;
    *out_newAllocation = true;
widgetIndexValid:
    widget = &context->stbDaWidgets[widgetIndex];
    korl_assert(widget->type == widgetType);
    widget->usedThisFrame = true;
    widget->realignY      = false;
    widget->orderIndex    = context->stbDaWindows[context->currentWindowIndex].widgets++;
    context->currentWidgetIndex = korl_checkCast_u$_to_i16(widgetIndex);
    return widget;
}
/**
 * When execution of this function completes, the AABB of the window's widgets 
 * will be cached in the \c window parameter's \c cachedAabb field.
 */
korl_internal void _korl_gui_processWidgetGraphics(_Korl_Gui_Window*const window, bool batchGraphics, f32 contentCursorOffsetX, f32 contentCursorOffsetY)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    if(window->isContentHidden)
        return;
    Korl_Math_V2f32 widgetCursor = window->position;
    if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        widgetCursor.y -= context->style.windowTitleBarPixelSizeY;
    widgetCursor.x -= contentCursorOffsetX;
    widgetCursor.y += contentCursorOffsetY;
    window->cachedContentAabb.min = window->cachedContentAabb.max = widgetCursor;
    f32 currentWidgetRowHeight = 0;// used to accumulate the AABB Y-size of a row of widgets
    for(u$ j = 0; j < arrlenu(context->stbDaWidgets); ++j)
    {
        _Korl_Gui_Widget*const widget = &context->stbDaWidgets[j];
        if(widget->parentWindowIdentifierHash != window->identifierHash)
            continue;
        widgetCursor.y -= context->style.widgetSpacingY;
        switch(widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_TEXT:{
            korl_time_probeStart(widget_text);
            if(widget->subType.text.gfxText)
            {
                korl_assert(!widget->subType.text.displayText);
                widget->cachedAabb = widget->subType.text.gfxText->_modelAabb;
                korl_math_v2f32_assignAdd(&(widget->cachedAabb.min), widgetCursor);
                korl_math_v2f32_assignAdd(&(widget->cachedAabb.max), widgetCursor);
                if(batchGraphics)
                {
                    widget->subType.text.gfxText->modelTranslate.xy = widgetCursor;
                    korl_gfx_text_draw(widget->subType.text.gfxText, korl_math_aabb2f32_fromPoints(window->position.x, window->position.y, window->position.x + window->size.x, window->position.y - window->size.y));
                }
            }
            else if(widget->subType.text.displayText)
            {
                korl_assert(!widget->subType.text.gfxText);
                Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, widget->subType.text.displayText, context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
                const Korl_Math_Aabb2f32 batchTextAabb = korl_gfx_batchTextGetAabb(batchText);
                const Korl_Math_V2f32 batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
                widget->cachedAabb.min = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){               0.f, batchTextAabbSize.y});
                widget->cachedAabb.max = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){batchTextAabbSize.x,                0.f});
                widget->cachedIsInteractive = false;
                if(batchGraphics)
                {
                    //KORL-ISSUE-000-000-008: instead of using the AABB of this text batch, we should be using the font's metrics!  Probably??  Different text batches of the same font will yield different sizes here, which will cause widget sizes to vary...
                    korl_gfx_batchSetPosition2d(batchText, widgetCursor.x, widgetCursor.y);
                    korl_time_probeStart(gfx_batch);
                    korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                    korl_time_probeStop(gfx_batch);
                }
            }
            korl_time_probeStop(widget_text);
            break;}
        case KORL_GUI_WIDGET_TYPE_BUTTON:{
#if 0// @TODO: use special flags to render special built-in button graphics?
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                {
                    korl_time_probeStart(button_close);
                    const Korl_Math_Aabb2f32 buttonAabb = korl_math_aabb2f32_fromPoints(titlebarButtonCursor.x, titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY, 
                                                                                        titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y);
                    korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
                    Korl_Vulkan_Color4u8 colorTitleBarButton = context->style.colorTitleBar;
                    if(    context->identifierHashMouseHoveredWindow == window->identifierHash
                        && korl_math_aabb2f32_containsV2f32(buttonAabb, context->mouseHoverPosition))
                        if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                            colorTitleBarButton = context->style.colorButtonPressed;
                        else
                            colorTitleBarButton = context->style.colorButtonWindowCloseActive;
                    korl_gfx_batchRectangleSetColor(batchWindowPanel, colorTitleBarButton);
                    korl_gfx_batchSetPosition2dV2f32(batchWindowPanel, titlebarButtonCursor);
                    korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                    Korl_Gfx_Batch*const batchWindowTitleCloseIconPiece = 
                        korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                             (Korl_Math_V2f32){0.1f*context->style.windowTitleBarPixelSizeY
                                                                              ,     context->style.windowTitleBarPixelSizeY}, 
                                                             (Korl_Math_V2f32){0.5f, 0.5f}, 
                                                             context->style.colorButtonWindowTitleBarIcons);
                    korl_gfx_batchSetPosition2d(batchWindowTitleCloseIconPiece, 
                                                titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY/2.f, 
                                                titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY/2.f);
                    korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z,  KORL_PI32*0.25f);
                    korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                    korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z, -KORL_PI32*0.25f);
                    korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                    titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
                    korl_time_probeStop(button_close);
                }//window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                {
                    korl_time_probeStart(button_hide);
                    const Korl_Math_Aabb2f32 buttonAabb = korl_math_aabb2f32_fromPoints(titlebarButtonCursor.x, titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY, 
                                                                                        titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y);
                    korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
                    Korl_Vulkan_Color4u8 colorTitleBarButton = context->style.colorTitleBar;
                    if(    context->identifierHashMouseHoveredWindow == window->identifierHash
                        && korl_math_aabb2f32_containsV2f32(buttonAabb, context->mouseHoverPosition))
                        if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                            colorTitleBarButton = context->style.colorButtonPressed;
                        else
                            colorTitleBarButton = context->style.colorTitleBarActive;
                    korl_gfx_batchRectangleSetColor(batchWindowPanel, colorTitleBarButton);
                    korl_gfx_batchSetPosition2dV2f32(batchWindowPanel, titlebarButtonCursor);
                    korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                    Korl_Gfx_Batch*const batchWindowTitleIconPiece = 
                        korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                             (Korl_Math_V2f32){     context->style.windowTitleBarPixelSizeY
                                                                              ,0.1f*context->style.windowTitleBarPixelSizeY}, 
                                                             (Korl_Math_V2f32){0.5f, 0.5f}, 
                                                             context->style.colorButtonWindowTitleBarIcons);
                    korl_gfx_batchSetPosition2d(batchWindowTitleIconPiece, 
                                                titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY/2.f, 
                                                titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY/2.f);
                    korl_gfx_batch(batchWindowTitleIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                    titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
                    korl_time_probeStop(button_hide);
                }//window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE
#endif
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, widget->subType.button.displayText, context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
            const Korl_Math_Aabb2f32 batchTextAabb = korl_gfx_batchTextGetAabb(batchText);
            const Korl_Math_V2f32 batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
            const f32 buttonAabbSizeX = batchTextAabbSize.x + context->style.widgetButtonLabelMargin * 2.f;
            const f32 buttonAabbSizeY = batchTextAabbSize.y + context->style.widgetButtonLabelMargin * 2.f;
            widget->cachedAabb.min = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){            0.f, buttonAabbSizeY});
            widget->cachedAabb.max = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){buttonAabbSizeX,             0.f});
            widget->cachedIsInteractive = true;
            if(batchGraphics)
            {
                Korl_Gfx_Batch*const batchButton = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                                        (Korl_Math_V2f32){ buttonAabbSizeX
                                                                                                         , buttonAabbSizeY}, 
                                                                                        (Korl_Math_V2f32){0.f, 1.f}, 
                                                                                        context->style.colorButtonInactive);
                korl_gfx_batchSetPosition2dV2f32(batchButton, widgetCursor);
                if(korl_math_aabb2f32_containsV2f32(widget->cachedAabb, context->mouseHoverPosition))
                {
                    if(context->isMouseDown && !context->isWindowDragged && context->identifierHashMouseDownWidget == widget->identifierHash)
                    {
                        korl_gfx_batchRectangleSetColor(batchButton, context->style.colorButtonPressed);
                    }
                    else if(context->isMouseHovering && context->identifierHashMouseHoveredWidget == widget->identifierHash)
                    {
                        korl_gfx_batchRectangleSetColor(batchButton, context->style.colorButtonActive);
                    }
                }
                korl_gfx_batch(batchButton, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                //KORL-ISSUE-000-000-008: instead of using the AABB of this text batch, we should be using the font's metrics!  Probably??  Different text batches of the same font will yield different sizes here, which will cause widget sizes to vary...
                korl_gfx_batchSetPosition2d(batchText, 
                                            widgetCursor.x + context->style.widgetButtonLabelMargin, 
                                            widgetCursor.y - context->style.widgetButtonLabelMargin);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            }
            break;}
        }
        window->cachedContentAabb = korl_math_aabb2f32_union(window->cachedContentAabb, widget->cachedAabb);
        KORL_MATH_ASSIGN_CLAMP_MIN(currentWidgetRowHeight, widget->cachedAabb.max.y - widget->cachedAabb.min.y);
        if(widget->realignY)
        {
            /* do nothing to the widgetCursor Y position, but advance the X position */
            widgetCursor.x += widget->cachedAabb.max.x - widget->cachedAabb.min.x;
        }
        else
        {
            widgetCursor.y -= currentWidgetRowHeight;                   // advance to the next Y position below this widget
            widgetCursor.x  = window->position.x - contentCursorOffsetX;// restore the X position to the value before this loop
            currentWidgetRowHeight = 0;// we're starting a new horizontal row of widgets; reset the AABB.y size accumulator
        }
    }
}
#endif
korl_internal void _korl_gui_widget_destroy(_Korl_Gui_Widget*const widget)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    switch(widget->type)
    {
    case KORL_GUI_WIDGET_TYPE_TEXT:{
        if(widget->subType.text.gfxText)
            korl_gfx_text_destroy(widget->subType.text.gfxText);
        break;}
    case KORL_GUI_WIDGET_TYPE_BUTTON:{
        break;}
    default:{
        korl_log(ERROR, "invalid widget type: %i", widget->type);
        break;}
    }
}
/** Prepare a new GUI batch for the current application frame.  The user _must_ 
 * call \c korl_gui_frameEnd after each call to \c _korl_gui_frameBegin . */
korl_internal void _korl_gui_frameBegin(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    context->currentWindowIndex = -1;
    korl_memory_allocator_empty(context->allocatorHandleStack);
#if 0//@TODO: recycle
    context->currentWidgetIndex = -1;
#endif
}
korl_internal void korl_gui_initialize(void)
{
    korl_memory_zero(&_korl_gui_context, sizeof(_korl_gui_context));
    _korl_gui_context.allocatorHandleHeap                  = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(1), L"korl-gui-heap" , KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL/*let platform choose address*/);
    _korl_gui_context.allocatorHandleStack                 = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR , korl_math_megabytes(8), L"korl-gui-stack", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL/*let platform choose address*/);
    _korl_gui_context.stringPool                           = korl_stringPool_create(_korl_gui_context.allocatorHandleHeap);
    _korl_gui_context.style.colorWindow                    = (Korl_Vulkan_Color4u8){ 16,  16,  16, 200};
    _korl_gui_context.style.colorWindowActive              = (Korl_Vulkan_Color4u8){ 24,  24,  24, 230};
    _korl_gui_context.style.colorWindowBorder              = (Korl_Vulkan_Color4u8){  0,   0,   0, 230};
    _korl_gui_context.style.colorWindowBorderHovered       = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    _korl_gui_context.style.colorWindowBorderResize        = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    _korl_gui_context.style.colorWindowBorderActive        = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    _korl_gui_context.style.colorTitleBar                  = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    _korl_gui_context.style.colorTitleBarActive            = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    _korl_gui_context.style.colorButtonInactive            = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    _korl_gui_context.style.colorButtonActive              = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    _korl_gui_context.style.colorButtonPressed             = (Korl_Vulkan_Color4u8){  0,   8,   0, 255};
    _korl_gui_context.style.colorButtonWindowTitleBarIcons = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    _korl_gui_context.style.colorButtonWindowCloseActive   = (Korl_Vulkan_Color4u8){255,   0,   0, 255};
    _korl_gui_context.style.colorScrollBar                 = (Korl_Vulkan_Color4u8){  8,   8,   8, 255};
    _korl_gui_context.style.colorScrollBarActive           = (Korl_Vulkan_Color4u8){ 32,  32,  32, 255};
    _korl_gui_context.style.colorScrollBarPressed          = (Korl_Vulkan_Color4u8){  0,   0,   0, 255};
    _korl_gui_context.style.colorText                      = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    _korl_gui_context.style.colorTextOutline               = (Korl_Vulkan_Color4u8){  0,   5,   0, 255};
    _korl_gui_context.style.textOutlinePixelSize           = 0.f;
    _korl_gui_context.style.fontWindowText                 = string_newEmptyUtf16(0);
    _korl_gui_context.style.windowTextPixelSizeY           = 24.f;
    _korl_gui_context.style.windowTitleBarPixelSizeY       = 20.f;
    _korl_gui_context.style.widgetSpacingY                 =  0.f;
    _korl_gui_context.style.widgetButtonLabelMargin        =  4.f;
    _korl_gui_context.style.windowScrollBarPixelWidth      = 12.f;
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_gui_context.allocatorHandleHeap), _korl_gui_context.stbDaWidgets, 64);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_gui_context.allocatorHandleHeap), _korl_gui_context.stbDaUsedWidgets, 64);
#if 0//@TODO: recycle
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_gui_context.allocatorHandleHeap), _korl_gui_context.stbDaWindows, 64);
#endif
    /* kick-start the first GUI frame as soon as initialization of this module is complete */
    _korl_gui_frameBegin();
}
korl_internal void korl_gui_onMouseEvent(const _Korl_Gui_MouseEvent* mouseEvent)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    const _Korl_Gui_UsedWidget*const stbDaUsedWidgetsEnd = context->stbDaUsedWidgets + arrlen(context->stbDaUsedWidgets);
    switch(mouseEvent->type)
    {
    case _KORL_GUI_MOUSE_EVENT_TYPE_MOVE:{
        _Korl_Gui_Widget* draggedWidget = NULL;
        if(context->identifierHashWidgetDragged)
        {
            for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
                if(usedWidget->widget->identifierHash == context->identifierHashWidgetDragged)
                {
                    draggedWidget = usedWidget->widget;
                    break;
                }
            if(!draggedWidget)
                context->identifierHashWidgetDragged = 0;
        }
        if(draggedWidget)
            draggedWidget->position = korl_math_v2f32_add(mouseEvent->subType.move.position, context->mouseDownWidgetOffset);
        break;}
    case _KORL_GUI_MOUSE_EVENT_TYPE_BUTTON:{
        context->identifierHashWidgetMouseDown = 0;
        context->identifierHashWidgetDragged   = 0;
        if(mouseEvent->subType.button.pressed)
        {
            context->isTopLevelWindowActive = false;
            /* iterate over all widgets from front=>back */
            for(_Korl_Gui_UsedWidget* usedWidget = KORL_C_CAST(_Korl_Gui_UsedWidget*, stbDaUsedWidgetsEnd - 1); usedWidget >= context->stbDaUsedWidgets; usedWidget--)
            {
                _Korl_Gui_Widget*const widget = usedWidget->widget;
                korl_assert(!widget->identifierHashParent);//@TODO: for now, let's just assume there are no child widgets for simplicity
                const Korl_Math_Aabb2f32 widgetAabb = korl_math_aabb2f32_fromPoints(widget->position.x, widget->position.y
                                                                                   ,widget->position.x + widget->size.x, widget->position.y -/*- because widget origin is the upper-left corner, & +Y is UP*/ widget->size.y);
                if(korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.button.position))
                {
                    context->isTopLevelWindowActive        = true;
                    context->identifierHashWidgetMouseDown = widget->identifierHash;
                    context->identifierHashWidgetDragged   = widget->identifierHash;
                    context->mouseDownWidgetOffset         = korl_math_v2f32_subtract(widget->position, mouseEvent->subType.button.position);
                    widget->orderIndex = ++context->rootWidgetOrderIndexHighest;/* set widget's order to be in front of all other widgets */
                    korl_assert(context->rootWidgetOrderIndexHighest);// check integer overflow
                    /* because we have changed the order of widgets, we need to re-sort since we are likely to process more events */
                    _korl_gui_usedWidget_quick_sort(context->stbDaUsedWidgets, arrlenu(context->stbDaUsedWidgets));
                    break;
                }
            }
        }
        break;}
    case _KORL_GUI_MOUSE_EVENT_TYPE_WHEEL:{
        break;}
    }
}
korl_internal KORL_FUNCTION_korl_gui_setFontAsset(korl_gui_setFontAsset)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    string_free(context->style.fontWindowText);
    if(fontAssetName)
        context->style.fontWindowText = string_newUtf16(fontAssetName);
}
korl_internal KORL_FUNCTION_korl_gui_windowBegin(korl_gui_windowBegin)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    /* assemble the window identifier hash; composed of the string hash of the 
        titleBarText, as well as the context loop index */
    u64 identifierHashComponents[2];
    identifierHashComponents[0] = korl_memory_acu16_hash(KORL_RAW_CONST_UTF16(titleBarText));
    identifierHashComponents[1] = context->loopIndex;
    const u64 identifierHash = titleBarText == _KORL_GUI_ORPHAN_WIDGET_WINDOW_TITLE_BAR_TEXT 
        ? _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH
        : korl_memory_acu16_hash(KORL_STRUCT_INITIALIZE(acu16){.data = KORL_C_CAST(u16*, &(identifierHashComponents[0]))
                                                              ,.size = sizeof(identifierHashComponents) / sizeof(u16)});
    // sanity checks //
    korl_assert(identifierHash);
    if(titleBarText != _KORL_GUI_ORPHAN_WIDGET_WINDOW_TITLE_BAR_TEXT)
        korl_assert(identifierHash != _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH);
    /* The only time that the current window index is allowed to be valid is 
        when the user was spawning orphan widgets, which are sent to a default 
        internal "debug" window.  Otherwise, we are in an invalid state. */
    if(context->currentWindowIndex >= 0)
        korl_assert(context->stbDaWidgets[context->currentWindowIndex].identifierHash == _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH);
    /* check to see if this identifier is already registered */
    const _Korl_Gui_Widget*const widgetsEnd = context->stbDaWidgets + arrlen(context->stbDaWidgets);
    for(_Korl_Gui_Widget* widget = context->stbDaWidgets; widget < widgetsEnd; widget++)
    {
        if(widget->identifierHash == identifierHash)
        {
            korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
            if(!widget->subType.window.isOpen && out_isOpen && *out_isOpen)
            {
                widget->subType.window.isFirstFrame = true;
                widget->isContentHidden             = false;
            }
            context->currentWindowIndex = korl_checkCast_i$_to_i16(widget - context->stbDaWidgets);
            goto done_currentWindowIndexValid;
        }
    }
    /* we are forced to allocate a new window in the memory pool */
    const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize();
    Korl_Math_V2f32 nextWindowPosition   = {surfaceSize.x*0.25f, surfaceSize.y*0.5f};
    u16             nextWindowOrderIndex = 0;
    /* if there are other window widgets in play already, the next window 
        position should be offset relative to the window with the highest order 
        index (the current top-level window) */
    i$ windowOrderIndexHighest = -1;
    for(_Korl_Gui_Widget* widget = context->stbDaWidgets; widget < widgetsEnd; widget++)
    {
        if(widget->identifierHashParent)
            continue;
        korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
        if(widget->orderIndex > windowOrderIndexHighest)
        {
            windowOrderIndexHighest = widget->orderIndex;
            nextWindowPosition   = korl_math_v2f32_add(widget->position, (Korl_Math_V2f32){ 32.f, 32.f });
            nextWindowOrderIndex = widget->orderIndex + 1;
            korl_assert(nextWindowOrderIndex);// sanity check integer overflow
        }
    }
    context->currentWindowIndex = korl_checkCast_u$_to_i16(arrlenu(context->stbDaWidgets));
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaWidgets, (_Korl_Gui_Widget){0});
    _Korl_Gui_Widget* newWindow = &context->stbDaWidgets[context->currentWindowIndex];
    newWindow->identifierHash              = identifierHash;
    newWindow->position                    = nextWindowPosition;
    newWindow->orderIndex                  = nextWindowOrderIndex;
    newWindow->size                        = (Korl_Math_V2f32){ 128.f, 128.f };
    newWindow->subType.window.titleBarText = string_newUtf16(titleBarText);
    newWindow->subType.window.isFirstFrame = true;
    newWindow->subType.window.isOpen       = true;
done_currentWindowIndexValid:
    newWindow = &context->stbDaWidgets[context->currentWindowIndex];
    newWindow->usedThisFrame             = true;
    newWindow->subType.window.styleFlags = styleFlags;
#if 0//@TODO: recycle
    newWindow->specialWidgetFlags  = KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE;
    if(out_isOpen)
    {
        newWindow->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE;
        newWindow->isOpen = *out_isOpen;
        if(newWindow->specialWidgetFlagsPressed & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
            newWindow->isOpen = false;
        *out_isOpen = newWindow->isOpen;
    }
    else
        newWindow->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE;
    newWindow->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE;
    if(newWindow->specialWidgetFlagsPressed & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
    {
        newWindow->isContentHidden = !newWindow->isContentHidden;
        if(newWindow->isContentHidden)
            newWindow->hiddenContentPreviousSizeY = newWindow->size.y;
        else
            newWindow->size.y = newWindow->hiddenContentPreviousSizeY;
    }
    newWindow->specialWidgetFlagsPressed = KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE;
    context->currentWidgetIndex = -1;
#endif
}
korl_internal KORL_FUNCTION_korl_gui_windowEnd(korl_gui_windowEnd)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->currentWindowIndex >= 0);
    context->currentWindowIndex = -1;
}
korl_internal KORL_FUNCTION_korl_gui_windowSetPosition(korl_gui_windowSetPosition)
{
#if 0//@TODO: recycle
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex >= 0);
    _Korl_Gui_Window*const window = &context->stbDaWindows[context->currentWindowIndex];
    const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize();
    window->position = (Korl_Math_V2f32){positionX - anchorX*window->size.x, -KORL_C_CAST(f32, surfaceSize.y) + positionY + anchorY*window->size.y};
#endif
}
korl_internal KORL_FUNCTION_korl_gui_windowSetSize(korl_gui_windowSetSize)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
#if 0//@TODO: recycle
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex >= 0);
    _Korl_Gui_Window*const window = &context->stbDaWindows[context->currentWindowIndex];
    window->isFirstFrame = false;
    window->size         = (Korl_Math_V2f32){sizeX, sizeY};
#endif
}
korl_internal void korl_gui_frameEnd(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    /* Once again, the only time the current window index is allowed to be set 
        to a valid id at this point is if the user is making orphan widgets. */
    if(context->currentWindowIndex >= 0)
    {
        korl_assert(context->stbDaWidgets[context->currentWindowIndex].identifierHash == _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH);
        korl_gui_windowEnd();
    }
    /* Initial loop over _all_ widgets to perform the following tasks: 
        - nullify widgets which weren't used this frame, _excluding_ root/window widgets!
        - gather a list of all widgets used this frame in preparation to perform transient topological/order sorting
        - figure out which root widget is "highest"/"top-level" (last to draw) */
    mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaUsedWidgets);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaUsedWidgets, arrlen(context->stbDaWidgets));
    u$ widgetsRemaining = 0;
    korl_time_probeStart(nullify_unused_widgets);
    const _Korl_Gui_Widget*const widgetsEnd = context->stbDaWidgets + arrlen(context->stbDaWidgets);
    for(_Korl_Gui_Widget* widget = context->stbDaWidgets; widget < widgetsEnd; widget++)
    {
        const u$ i = widget - context->stbDaWidgets;
        korl_assert(widget->identifierHash);
        if(widget->usedThisFrame || !widget->identifierHashParent)
        {
            if(widgetsRemaining < i)
                context->stbDaWidgets[widgetsRemaining] = *widget;
            KORL_ZERO_STACK(_Korl_Gui_UsedWidget, newUsedWidget);
            newUsedWidget.widget = context->stbDaWidgets + widgetsRemaining++;
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaUsedWidgets, newUsedWidget);
        }
        else
        {
            _korl_gui_widget_destroy(widget);
            continue;
        }
    }
    korl_assert(widgetsRemaining <= arrlenu(context->stbDaWidgets));//sanity-check to make sure we aren't about to invalidate _Widget pointers we just accumulated
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaWidgets, widgetsRemaining);
    korl_time_probeStop(nullify_unused_widgets);
    /* sort the list of widgets based on their orderIndex, such that widgets 
        that are to be drawn behind other widgets (drawn first) occupy lower 
        indices in the list */
    _korl_gui_usedWidget_quick_sort(context->stbDaUsedWidgets, arrlenu(context->stbDaUsedWidgets));
    /* sanity-check the sorted list of used widgets to make sure that for each 
        widget sub tree, all child orderIndex values are unique */
    //@TODO
    /* prepare the view/projection graphics state */
    const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize();
    Korl_Gfx_Camera cameraOrthographic = korl_gfx_createCameraOrtho(1.f/*clipDepth*/);
    cameraOrthographic.position.xy = 
    cameraOrthographic.target.xy   = (Korl_Math_V2f32){surfaceSize.x/2.f, surfaceSize.y/2.f};
    korl_gfx_useCamera(cameraOrthographic);
    /* process _all_ sorted in-use widgets for this frame */
    korl_shared_const Korl_Math_V2f32 ORIGIN_RATIO_UPPER_LEFT = {0, 1};// remember, +Y is UP!
    #ifdef _KORL_GUI_DEBUG_DRAW_COORDINATE_FRAMES
        Korl_Gfx_Batch*const batchLinesOrigin2d = korl_gfx_createBatchLines(context->allocatorHandleStack, 2);
        korl_gfx_batchSetLine(batchLinesOrigin2d, 0, KORL_MATH_V2F32_ZERO.elements, KORL_MATH_V2F32_X.elements, 2, KORL_COLOR4U8_RED);
        korl_gfx_batchSetLine(batchLinesOrigin2d, 1, KORL_MATH_V2F32_ZERO.elements, KORL_MATH_V2F32_Y.elements, 2, KORL_COLOR4U8_GREEN);
    #endif
    u16 rootWidgetCount = 0;// used to normalize root widget orderIndex values, since it is possible to fragment these values at any time
    const _Korl_Gui_UsedWidget*const usedWidgetsEnd = context->stbDaUsedWidgets + arrlen(context->stbDaUsedWidgets);
    for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
    {
        _Korl_Gui_Widget*const widget = usedWidget->widget;
        korl_time_probeStart(setup_camera);
        /* prepare to draw all the widget's contents by cutting out a scissor 
            region the size of the widget, preventing us from accidentally 
            render any pixels outside the widget */
        korl_gfx_cameraSetScissor(&cameraOrthographic
                                 ,widget->position.x
                                 ,surfaceSize.y - widget->position.y/*inverted, because remember: korl-gui window-space uses _correct_ y-axis direction (+y is UP)*/
                                 ,widget->size.x
                                 ,widget->size.y);
        korl_gfx_useCamera(cameraOrthographic);
        korl_time_probeStop(setup_camera);
        switch(widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_WINDOW:{
            /* draw the window panel */
            Korl_Vulkan_Color4u8 windowColor   = context->style.colorWindow;
            Korl_Vulkan_Color4u8 titleBarColor = context->style.colorTitleBar;
            if(context->isTopLevelWindowActive && !widget->identifierHashParent && widget->orderIndex == context->rootWidgetOrderIndexHighest)
            {
                windowColor   = context->style.colorWindowActive;
                titleBarColor = context->style.colorTitleBarActive;
            }
            korl_time_probeStart(draw_window_panel);
            Korl_Gfx_Batch*const batchWindowPanel = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, widget->size, ORIGIN_RATIO_UPPER_LEFT, windowColor);
            korl_gfx_batchSetPosition2dV2f32(batchWindowPanel, widget->position);
            korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            if(widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            {
                korl_time_probeStart(title_bar);
                /* draw the window title bar */
                korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){widget->size.x, context->style.windowTitleBarPixelSizeY});
                korl_gfx_batchRectangleSetColor(batchWindowPanel, titleBarColor);// conditionally highlight the title bar color
                korl_gfx_batchSetVertexColor(batchWindowPanel, 0, context->style.colorTitleBar);// keep the bottom two vertices the default title bar color
                korl_gfx_batchSetVertexColor(batchWindowPanel, 1, context->style.colorTitleBar);// keep the bottom two vertices the default title bar color
                korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                /* draw the window title bar text */
                Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorHandleStack
                                                                                    ,string_getRawUtf16(context->style.fontWindowText)
                                                                                    ,string_getRawUtf16(widget->subType.window.titleBarText)
                                                                                    ,context->style.windowTextPixelSizeY
                                                                                    ,context->style.colorText
                                                                                    ,context->style.textOutlinePixelSize
                                                                                    ,context->style.colorTextOutline);
                const Korl_Math_V2f32 batchTextSize = korl_math_aabb2f32_size(korl_gfx_batchTextGetAabb(batchWindowTitleText));
                korl_gfx_batchSetPosition2d(batchWindowTitleText
                                           ,widget->position.x
                                           ,widget->position.y - (context->style.windowTitleBarPixelSizeY - batchTextSize.y) / 2.f);
                korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                /**/
                korl_time_probeStop(title_bar);
            }//window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR
            korl_time_probeStop(draw_window_panel);
            break;}
        default:{
            korl_log(ERROR, "unhandled widget type: %i", widget->type);
            break;}
        }
        widget->usedThisFrame = false;// reset this value for the next frame; this can be done at any time during this loop
        /* normalize root widget orderIndex values; _must_ be done _after_ processing widget logic! */
        if(!widget->identifierHashParent)
        {
            widget->orderIndex = rootWidgetCount++;
            korl_assert(rootWidgetCount);// check integer overflow
        }
    }
    context->rootWidgetOrderIndexHighest = rootWidgetCount ? rootWidgetCount - 1 : 0;
#if 0//@TODO: recycle
    /* for each of the windows that we ended up using for this frame, generate 
        the necessary draw commands for them */
    for(u$ i = 0; i < arrlenu(context->stbDaWindows); ++i)
    {
        _Korl_Gui_Window*const window = &context->stbDaWindows[i];
        korl_assert(window->identifierHash);
        /* if the window wasn't used this frame, skip it and prepare to remove 
            it from the memory pool */
        if(window->usedThisFrame)
        {
            window->usedThisFrame = false;
            if(windowsRemaining < i)
                context->stbDaWindows[windowsRemaining] = *window;
            windowsRemaining++;
        }
        else
            continue;
        /**/
        if(!window->isOpen)
            continue;
        if(window->isFirstFrame || (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE))
        {
            korl_time_probeStart(calculate_window_AABB);
            window->isFirstFrame = false;
            /* iterate over all this window's widgets, obtaining their AABBs & 
                accumulating their geometry to determine how big the window 
                needs to be */
            //KORL-PERFORMANCE-000-000-040: gui: MEDIUM-MAJOR; attempt to completely remove the necessity to call _korl_gui_processWidgetGraphics two times inside korl_gui_frameEnd, and remove the second parameter for that matter
            _korl_gui_processWidgetGraphics(window, false, 0.f, 0.f);
            Korl_Math_Aabb2f32 windowTotalAabb = window->cachedContentAabb;
            /* take the AABB of the window's title bar into account as well */
            if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            {
                Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, window->titleBarText, context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
                const Korl_Math_V2f32 batchTextSize = korl_math_aabb2f32_size(korl_gfx_batchTextGetAabb(batchWindowTitleText));
                f32 titleBarOptimalSizeX = batchTextSize.x;
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                    titleBarOptimalSizeX += context->style.windowTitleBarPixelSizeY;
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                    titleBarOptimalSizeX += context->style.windowTitleBarPixelSizeY;
                windowTotalAabb.max.x = KORL_MATH_MAX(windowTotalAabb.max.x, window->position.x + titleBarOptimalSizeX);
                windowTotalAabb.max.y += context->style.windowTitleBarPixelSizeY;
            }
            /* size the window based on the above metrics */
            window->size = korl_math_aabb2f32_size(windowTotalAabb);
            /* prevent the window from being too small */
            KORL_MATH_ASSIGN_CLAMP_MIN(window->size.x, context->style.windowTitleBarPixelSizeY);
            KORL_MATH_ASSIGN_CLAMP_MIN(window->size.y, context->style.windowTitleBarPixelSizeY);
            if(window->size.y < context->style.windowTitleBarPixelSizeY)
                window->size.y = context->style.windowTitleBarPixelSizeY;
            if(window->size.x < context->style.windowTitleBarPixelSizeY)
                window->size.x = context->style.windowTitleBarPixelSizeY;
            korl_time_probeStop(calculate_window_AABB);
        }
        if(window->isContentHidden)
            window->size.y = context->style.windowTitleBarPixelSizeY;
        /* bind the windows to the bounds of the swapchain, such that there will 
            always be a square of grabable geometry on the window whose 
            dimensions equal the height of the window title bar style at minimum */
        KORL_MATH_ASSIGN_CLAMP(window->position.x, 
                               -window->size.x + context->style.windowTitleBarPixelSizeY, 
                               surfaceSize.x - context->style.windowTitleBarPixelSizeY);
        KORL_MATH_ASSIGN_CLAMP(window->position.y, 
                               -KORL_C_CAST(f32, surfaceSize.y) + context->style.windowTitleBarPixelSizeY, 
                               0);
        /* render all this window's widgets within the window panel */
        korl_time_probeStart(draw_widgets);
        // calculate how much area there is for the window's widgets to occupy & 
        //  how much visible space is available in the window //
        Korl_Math_V2f32 availableContentArea = window->size;
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            availableContentArea.y -= context->style.windowTitleBarPixelSizeY;
        const Korl_Math_V2f32 contentSize = korl_math_aabb2f32_size(window->cachedContentAabb);
        // determine which scrollbars we need to use to view all the content //
        window->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
        window->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
        if(contentSize.x > availableContentArea.x)
        {
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
            availableContentArea.y -= context->style.windowScrollBarPixelWidth;
        }
        if(contentSize.y > availableContentArea.y)
        {
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
            availableContentArea.x -= context->style.windowScrollBarPixelWidth;
        }
        // re-evaluate the presence of each scrollbar, only modifying the 
        //  available content area if the scrollbar wasn't already taken into 
        //  account, since the presence of one scrollbar can add the requirement 
        //  of another //
        if(contentSize.x > availableContentArea.x)
        {
            if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X))
                availableContentArea.y -= context->style.windowScrollBarPixelWidth;
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
        }
        if(contentSize.y > availableContentArea.y)
        {
            if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y))
                availableContentArea.x -= context->style.windowScrollBarPixelWidth;
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
        }
        window->cachedAvailableContentSize = availableContentArea;
        // do scroll bar logic to determine the adjusted widget cursor position //
        korl_time_probeStart(scroll_bar_logic);
        korl_shared_const f32 SCROLLBAR_LENGTH_MIN = 8.f;
        Korl_Gfx_Batch* batchScrollBarX = NULL;
        Korl_Gfx_Batch* batchScrollBarY = NULL;
        if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
        {
            korl_time_probeStart(scroll_bar_x);
            /* determine scroll bar length */
            window->cachedScrollBarLengthX = availableContentArea.x * availableContentArea.x / contentSize.x;
            window->cachedScrollBarLengthX = KORL_MATH_MAX(window->cachedScrollBarLengthX, SCROLLBAR_LENGTH_MIN);
            /* bind the scroll bar to the current possible range */
            const f32 scrollBarRangeX = availableContentArea.x - window->cachedScrollBarLengthX;
            window->scrollBarPositionX = KORL_MATH_CLAMP(window->scrollBarPositionX, 0.f, scrollBarRangeX);
            const f32 scrollBarPositionX = window->position.x + window->scrollBarPositionX;
            /* batch scroll bar graphics */
            batchScrollBarX = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                   (Korl_Math_V2f32){window->cachedScrollBarLengthX, context->style.windowScrollBarPixelWidth}, 
                                                                   (Korl_Math_V2f32){0.f, 0.f}, 
                                                                   context->style.colorScrollBar);
            if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
                korl_gfx_batchRectangleSetColor(batchScrollBarX, context->style.colorScrollBarPressed);
            else if(context->identifierHashMouseHoveredWindow == window->identifierHash)
            {
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(scrollBarPositionX                                 , window->position.y - window->size.y, 
                                                                                       scrollBarPositionX + window->cachedScrollBarLengthX, window->position.y - window->size.y + context->style.windowScrollBarPixelWidth);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, context->mouseHoverPosition))
                    korl_gfx_batchRectangleSetColor(batchScrollBarX, context->style.colorScrollBarActive);
            }
            korl_gfx_batchSetPosition2d(batchScrollBarX, scrollBarPositionX, window->position.y - window->size.y);
            korl_time_probeStop(scroll_bar_x);
        }
        if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y)
        {
            korl_time_probeStart(scroll_bar_y);
            /* determine scroll bar length */
            window->cachedScrollBarLengthY = availableContentArea.y * availableContentArea.y / contentSize.y;
            window->cachedScrollBarLengthY = KORL_MATH_MAX(window->cachedScrollBarLengthY, SCROLLBAR_LENGTH_MIN);
            /* bind the scroll bar to the current possible range */
            const f32 scrollBarRangeY = ((window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR) ? window->size.y - context->style.windowTitleBarPixelSizeY : window->size.y)
                                      - window->cachedScrollBarLengthY;
            window->scrollBarPositionY = KORL_MATH_CLAMP(window->scrollBarPositionY, 0.f, scrollBarRangeY);
            f32 scrollBarPositionY = window->position.y - window->scrollBarPositionY;
            if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
                scrollBarPositionY -= context->style.windowTitleBarPixelSizeY;
            /* batch scroll bar graphics */
            batchScrollBarY = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                   (Korl_Math_V2f32){context->style.windowScrollBarPixelWidth, window->cachedScrollBarLengthY}, 
                                                                   (Korl_Math_V2f32){1.f, 1.f}, 
                                                                   context->style.colorScrollBar);
            if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y)
                korl_gfx_batchRectangleSetColor(batchScrollBarY, context->style.colorScrollBarPressed);
            else if(context->identifierHashMouseHoveredWindow == window->identifierHash)
            {
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(window->position.x + window->size.x                                           , scrollBarPositionY, 
                                                                                       window->position.x + window->size.x - context->style.windowScrollBarPixelWidth, scrollBarPositionY - window->cachedScrollBarLengthY);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, context->mouseHoverPosition))
                    korl_gfx_batchRectangleSetColor(batchScrollBarY, context->style.colorScrollBarActive);
            }
            korl_gfx_batchSetPosition2d(batchScrollBarY, window->position.x + window->size.x, scrollBarPositionY);
            korl_time_probeStop(scroll_bar_y);
        }
        if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X))
            window->scrollBarPositionX = 0;
        if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y))
            window->scrollBarPositionY = 0;
        // convert scroll bar positions to content cursor offset //
        const f32 scrollBarRangeX = availableContentArea.x - window->cachedScrollBarLengthX;
        const f32 scrollBarRangeY = ((window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR) ? window->size.y - context->style.windowTitleBarPixelSizeY : window->size.y)
                                  - window->cachedScrollBarLengthY;
        const f32 scrollBarRatioX = korl_math_isNearlyZero(scrollBarRangeX) ? 0 : window->scrollBarPositionX / scrollBarRangeX;
        const f32 scrollBarRatioY = korl_math_isNearlyZero(scrollBarRangeY) ? 0 : window->scrollBarPositionY / scrollBarRangeY;
        const f32 contentCursorOffsetX = scrollBarRatioX * (contentSize.x - availableContentArea.x);
        const f32 contentCursorOffsetY = scrollBarRatioY * (contentSize.y - availableContentArea.y);
        korl_time_probeStop(scroll_bar_logic);
        // before drawing the window contents, we need to make sure it will not 
        //  draw on top of the title bar if there is one present //
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            korl_gfx_cameraSetScissor(&guiCamera, 
                                      window->position.x, 
                                      /*natural +Y=>UP space => swap chain space conversion*/-window->position.y + context->style.windowTitleBarPixelSizeY, 
                                      window->size.x, 
                                      window->size.y - context->style.windowTitleBarPixelSizeY);
            korl_gfx_useCamera(guiCamera);
        }
        korl_time_probeStart(process_widget_gfx);
        _korl_gui_processWidgetGraphics(window, true, contentCursorOffsetX, contentCursorOffsetY);
        korl_time_probeStop(process_widget_gfx);
        /* draw scroll bars, if they are needed */
        korl_time_probeStart(batch_scroll_bars);
        if(batchScrollBarX)
            korl_gfx_batch(batchScrollBarX, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        if(batchScrollBarY)
            korl_gfx_batch(batchScrollBarY, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_time_probeStop(batch_scroll_bars);
        korl_time_probeStop(draw_widgets);
        korl_time_probeStart(draw_window_border);
        /* batch window border AFTER contents are drawn */
        Korl_Vulkan_Color4u8 colorBorder = context->style.colorWindowBorder;
        if(context->isTopLevelWindowActive && i == arrlenu(context->stbDaWindows) - 1)
            if(context->isMouseHovering 
                && context->identifierHashMouseHoveredWindow == window->identifierHash
                && context->mouseHoverWindowEdgeFlags == KORL_GUI_MOUSE_HOVER_FLAGS_NONE)
                colorBorder = context->style.colorWindowBorderResize;
            else
                colorBorder = context->style.colorWindowBorderActive;
        else if(context->isMouseHovering && context->identifierHashMouseHoveredWindow == window->identifierHash)
            colorBorder = context->style.colorWindowBorderHovered;
        const Korl_Vulkan_Color4u8 colorBorderUp    = (context->identifierHashMouseHoveredWindow == window->identifierHash && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_UP   )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color4u8 colorBorderRight = (context->identifierHashMouseHoveredWindow == window->identifierHash && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_RIGHT)) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color4u8 colorBorderDown  = (context->identifierHashMouseHoveredWindow == window->identifierHash && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_DOWN )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color4u8 colorBorderLeft  = (context->identifierHashMouseHoveredWindow == window->identifierHash && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_LEFT )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Math_Aabb2f32 windowAabb = korl_math_aabb2f32_fromPoints(window->position.x                 , window->position.y - window->size.y, 
                                                                            window->position.x + window->size.x, window->position.y                  );
        Korl_Gfx_Batch*const batchWindowBorder = korl_gfx_createBatchLines(context->allocatorHandleStack, 4);
        //KORL-ISSUE-000-000-009
        korl_gfx_batchSetLine(batchWindowBorder, 0, (Korl_Math_V3f32){windowAabb.min.x - 0.5f, windowAabb.max.y}.elements, (Korl_Math_V3f32){windowAabb.max.x, windowAabb.max.y}.elements, 2, colorBorderUp);
        korl_gfx_batchSetLine(batchWindowBorder, 1, (Korl_Math_V3f32){windowAabb.max.x       , windowAabb.max.y}.elements, (Korl_Math_V3f32){windowAabb.max.x, windowAabb.min.y}.elements, 2, colorBorderRight);
        korl_gfx_batchSetLine(batchWindowBorder, 2, (Korl_Math_V3f32){windowAabb.max.x       , windowAabb.min.y}.elements, (Korl_Math_V3f32){windowAabb.min.x, windowAabb.min.y}.elements, 2, colorBorderDown);
        korl_gfx_batchSetLine(batchWindowBorder, 3, (Korl_Math_V3f32){windowAabb.min.x       , windowAabb.min.y}.elements, (Korl_Math_V3f32){windowAabb.min.x, windowAabb.max.y}.elements, 2, colorBorderLeft);
        korl_gfx_cameraSetScissorPercent(&guiCamera, 0,0, 1,1);
        korl_gfx_useCamera(guiCamera);
        korl_gfx_batch(batchWindowBorder, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_time_probeStop(draw_window_border);
        /* reset transient window properties in preparation for the next frame */
        window->widgets = 0;
    }
    korl_time_probeStop(generate_draw_commands);
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaWindows, windowsRemaining);
#endif
#ifdef _KORL_GUI_DEBUG_DRAW_COORDINATE_FRAMES
    {
        korl_gfx_cameraSetScissorPercent(&cameraOrthographic, 0,0, 1,1);
        korl_gfx_useCamera(cameraOrthographic);
        korl_gfx_batchSetScale(batchLinesOrigin2d, (Korl_Math_V3f32){100,100,100});
        korl_gfx_batchSetPosition2d(batchLinesOrigin2d, 1.f, 1.f);
        korl_gfx_batch(batchLinesOrigin2d, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        for(_Korl_Gui_UsedWidget* usedWidget = stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
        {
            _Korl_Gui_Widget*const widget = usedWidget->widget;
            korl_gfx_batchSetPosition2dV2f32(batchLinesOrigin2d, widget->position);
            korl_gfx_batchSetScale(batchLinesOrigin2d, (Korl_Math_V3f32){32,32,32});
            korl_gfx_batch(batchLinesOrigin2d, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        }
    }
#endif
    /* begin the next frame as soon as this frame has ended */
    _korl_gui_frameBegin();
}
korl_internal KORL_FUNCTION_korl_gui_setLoopIndex(korl_gui_setLoopIndex)
{
#if 0//@TODO: recycle
    _Korl_Gui_Context*const context = &_korl_gui_context;
    context->loopIndex = loopIndex;
#endif
}
korl_internal KORL_FUNCTION_korl_gui_realignY(korl_gui_realignY)
{
#if 0//@TODO: recycle
    _Korl_Gui_Context*const context = &_korl_gui_context;
    if(   context->currentWidgetIndex < 0
       || context->currentWidgetIndex >= korl_checkCast_u$_to_i$(arrlenu(context->stbDaWidgets)))
        return;// silently do nothing if user has not created a widget yet for the current window
    context->stbDaWidgets[context->currentWidgetIndex].realignY = true;
#endif
}
korl_internal KORL_FUNCTION_korl_gui_widgetTextFormat(korl_gui_widgetTextFormat)
{
#if 0//@TODO: recycle
    _Korl_Gui_Context*const context = &_korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(textFormat), KORL_GUI_WIDGET_TYPE_TEXT, &newAllocation);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.text.displayText = korl_memory_stringFormatVaList(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
#endif
}
korl_internal KORL_FUNCTION_korl_gui_widgetText(korl_gui_widgetText)
{
#if 0//@TODO: recycle
    _Korl_Gui_Context*const context = &_korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(identifier), KORL_GUI_WIDGET_TYPE_TEXT, &newAllocation);
    if(newAllocation)
    {
        korl_assert(korl_memory_isNull(&widget->subType.text, sizeof(widget->subType.text)));
        widget->subType.text.gfxText = korl_gfx_text_create(context->allocatorHandleHeap, (acu16){.data = context->style.fontWindowText, .size = korl_memory_stringSize(context->style.fontWindowText)}, context->style.windowTextPixelSizeY);
    }
    /* at this point, we either have a new gfxText on the widget, or we're using 
        an already existing widget's gfxText member; in either case, we need to 
        now append the provided newText to the gfxText member of the text widget */
    if(newText.size)
    {
        KORL_ZERO_STACK(_Korl_Gui_CodepointTestData_Log, codepointTestDataLog);
        if(flags & KORL_GUI_WIDGET_TEXT_FLAG_LOG)
        {
            codepointTest         = _korl_gui_codepointTest_log;
            codepointTestUserData = &codepointTestDataLog;
            // it should be possible to nest the internal log codepointTest with the user-provided one instead of completely overriding it; 
            //  maybe at some point in the future if that becomes useful behavior...
        }
        korl_gfx_text_fifoAdd(widget->subType.text.gfxText, newText, context->allocatorHandleStack, codepointTest, codepointTestUserData);
    }
    const u$ textLines = arrlenu(widget->subType.text.gfxText->stbDaLines);
    if(textLines > maxLineCount)
        korl_gfx_text_fifoRemove(widget->subType.text.gfxText, textLines - maxLineCount);
#endif
}
korl_internal KORL_FUNCTION_korl_gui_widgetButtonFormat(korl_gui_widgetButtonFormat)
{
#if 0//@TODO: recycle
    _Korl_Gui_Context*const context = &_korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(textFormat), KORL_GUI_WIDGET_TYPE_BUTTON, &newAllocation);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.button.displayText = korl_memory_stringFormatVaList(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
    const u8 resultActuationCount = widget->subType.button.actuationCount;
    widget->subType.button.actuationCount = 0;
    return resultActuationCount;
#endif
    return 0;///@TODO: delete
}
korl_internal void korl_gui_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
#if 0//@TODO: recycle
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_gui_context, sizeof(_korl_gui_context));
#endif
}
korl_internal bool korl_gui_saveStateRead(HANDLE hFile)
{
#if 0//@TODO: recycle
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    if(!ReadFile(hFile, &_korl_gui_context, sizeof(_korl_gui_context), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
#endif
    return true;
}
