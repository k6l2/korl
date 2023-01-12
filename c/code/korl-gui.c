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
    struct
    {
        bool visited;// set to true when we have visited this node; this struct is simply a wrapper around _Korl_Gui_Widget so we can sort all widgets from back=>front
        /* all values below here are invalid until the visited flag is set */
        u16 depth;// 0 root node; +1 for each successive depth in the hierarchy
    } dagMetaData;
    struct
    {
        Korl_Math_V2f32 childWidgetCursor;// used to determine where the next child widget will be placed with respect to this widget's placement
        Korl_Math_V2f32 childWidgetCursorOrigin;// offset relative to this widget's origin
        f32 currentWidgetRowHeight;
        Korl_Math_Aabb2f32 aabb;// used for collision detection at the top of the next frame; we need to do this because the widget's AABB must first be calculated based on its contents, and then it must be clipped to all parent widget AABBs so the user can't click on it "out of bounds"; IMPORTANT: do _NOT_ use this value for drawing widget content, as it will not accurately represent where the real widget placement is in world-space (this AABB is only useful for collision-detection purposes); IMPORTANT: do _NOT_ calculate the next frame's widget placement with this value except in very specific scenarios (like WINDOW widgets), as this value is subject to clipping applied by all the widget's parents recursively
        Korl_Math_Aabb2f32 aabbContent;// an accumulation of all content within this widget, including all content that lies outside of `aabb`; this is very important for performing auto-resize logic
        Korl_Math_Aabb2f32 aabbChildren;// an accumulation of all content AABBs _only_ of children of this widget, including all content that lies outside of `aabb`; useful for SCROLL_AREA widgets to clamp the child content AABB to our visible AABB
        bool isFirstFrame;// storing this value during widget logic at the end of frame allows us to properly resize window widgets recursively without having to do an extra separate loop to clear all the widget flags that hold the same value
    } transient;// this data is only used/valid at the end of the frame, when we need to process the list of all UsedWidgets that have been accumulated during the frame
} _Korl_Gui_UsedWidget;
typedef struct _Korl_Gui_WidgetMap
{
    u64 key;  // the widget's id hash value
    u$  value;// the index of the _Korl_Gui_UsedWidget in the context's stbDaUsedWidgets member
} _Korl_Gui_WidgetMap;
/* Sort used widgets by depth first, then by orderIndex */
#define SORT_NAME _korl_gui_usedWidget
#define SORT_TYPE _Korl_Gui_UsedWidget
#define SORT_CMP(x, y) ((x).dagMetaData.depth < (y).dagMetaData.depth \
                        ? -1 \
                        : (x).dagMetaData.depth > (y).dagMetaData.depth \
                          ? 1 \
                          : (x).widget->orderIndex < (y).widget->orderIndex \
                            ? -1 \
                            : ((x).widget->orderIndex > (y).widget->orderIndex \
                              ? 1 \
                              : 0))
#ifndef SORT_CHECK_CAST_INT_TO_SIZET
    #define SORT_CHECK_CAST_INT_TO_SIZET(x) korl_checkCast_i$_to_u$(x)
#endif
#ifndef SORT_CHECK_CAST_SIZET_TO_INT
    #define SORT_CHECK_CAST_SIZET_TO_INT(x) korl_checkCast_u$_to_i32(x)
#endif
#include "sort.h"
/**/
#if KORL_DEBUG
    // #define _KORL_GUI_DEBUG_DRAW_COORDINATE_FRAMES
    // #define _KORL_GUI_DEBUG_DRAW_SCROLL_AREA
#endif
#if defined(_LOCAL_STRING_POOL_POINTER)
    #undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&_korl_gui_context.stringPool)
korl_shared_const wchar_t _KORL_GUI_ORPHAN_WIDGET_WINDOW_TITLE_BAR_TEXT[] = L"DEBUG";
korl_shared_const u64     _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH          = KORL_U64_MAX;
korl_shared_const wchar_t _KORL_GUI_WIDGET_BUTTON_WINDOW_CLOSE[]          = L"X";// special internal button string to allow button widget to draw special graphics
korl_shared_const wchar_t _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZE[]       = L"-";// special internal button string to allow button widget to draw special graphics
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
    }
}
#endif
korl_internal void _korl_gui_findUsedWidgetDepthRecursive(_Korl_Gui_UsedWidget* usedWidget, _Korl_Gui_WidgetMap** stbHmWidgetMap)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    if(usedWidget->dagMetaData.visited)
        return;
    usedWidget->dagMetaData.visited = true;
    if(usedWidget->widget->identifierHashParent)
    {
        const i$ hmIndexParent = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), *stbHmWidgetMap, usedWidget->widget->identifierHashParent);
        korl_assert(hmIndexParent >= 0);
        _Korl_Gui_UsedWidget*const usedWidgetParent = context->stbDaUsedWidgets + hmIndexParent;
        _korl_gui_findUsedWidgetDepthRecursive(usedWidgetParent, stbHmWidgetMap);
        usedWidget->dagMetaData.depth = usedWidgetParent->dagMetaData.depth + 1;
    }
    else
        usedWidget->dagMetaData.depth = 0;
}
korl_internal void _korl_gui_resetTransientNextWidgetModifiers(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    context->transientNextWidgetModifiers.size         = (Korl_Math_V2f32){korl_math_nanf32(), korl_math_nanf32()};
    context->transientNextWidgetModifiers.parentAnchor = (Korl_Math_V2f32){korl_math_nanf32(), korl_math_nanf32()};
    context->transientNextWidgetModifiers.parentOffset = (Korl_Math_V2f32){korl_math_nanf32(), korl_math_nanf32()};
    context->transientNextWidgetModifiers.orderIndex   = -1;
}
korl_internal _Korl_Gui_Widget* _korl_gui_getWidget(u64 identifierHash, u$ widgetType, bool* out_newAllocation)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    /* if there is no current active window, then we should just default to use 
        an internal "debug" window to allow the user to just create widgets at 
        any time without worrying about creating a window first */
    if(arrlen(context->stbDaWidgetParentStack) <= 0)
        korl_gui_windowBegin(_KORL_GUI_ORPHAN_WIDGET_WINDOW_TITLE_BAR_TEXT, NULL, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
    korl_assert(arrlen(context->stbDaWidgetParentStack) > 0);
    _Korl_Gui_Widget*const widgetDirectParent = context->stbDaWidgets + arrlast(context->stbDaWidgetParentStack);
    /* each widget's final id hash is a convolution of the following values: 
        - the local given identifierHash
        - all parent widget hashes
        - the context loopIndex */
    u64 identifierHashComponents[2];
    identifierHashComponents[0] = identifierHash;
    const i16*const stbDaWidgetParentStackEnd = context->stbDaWidgetParentStack + arrlen(context->stbDaWidgetParentStack);
    for(const i16* parentIndex = context->stbDaWidgetParentStack; parentIndex < stbDaWidgetParentStackEnd; parentIndex++)
    {
        identifierHashComponents[1] = context->stbDaWidgets[*parentIndex].identifierHash;
        identifierHashComponents[0] = korl_memory_acu16_hash(KORL_STRUCT_INITIALIZE(acu16){.data = KORL_C_CAST(u16*, &(identifierHashComponents[0]))
                                                                                          ,.size = sizeof(identifierHashComponents) / sizeof(u16)});
    }
    identifierHashComponents[1] = context->loopIndex;
    const u64 identifierHashPrime = korl_memory_acu16_hash(KORL_STRUCT_INITIALIZE(acu16){.data = KORL_C_CAST(u16*, &(identifierHashComponents[0]))
                                                                                        ,.size = sizeof(identifierHashComponents) / sizeof(u16)});
    /* check to see if this widget's identifier set is already registered */
    u$ widgetIndex = KORL_U64_MAX;
    const _Korl_Gui_Widget*const stbDaWidgetsEnd = context->stbDaWidgets + arrlen(context->stbDaWidgets);
    for(_Korl_Gui_Widget* widget = context->stbDaWidgets; widget < stbDaWidgetsEnd; widget++)
    {
        const u$ w = widget - context->stbDaWidgets;
        if(widget->identifierHash == identifierHashPrime)
        {
            korl_assert(widget->identifierHashParent == widgetDirectParent->identifierHash);
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
    widget->identifierHash       = identifierHashPrime;
    widget->identifierHashParent = widgetDirectParent->identifierHash;
    widget->type                 = widgetType;
    *out_newAllocation = true;
widgetIndexValid:
    widget = &context->stbDaWidgets[widgetIndex];
    korl_assert(widget->type == widgetType);
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), context->stbDaWidgetParentStack, korl_checkCast_u$_to_i16(widgetIndex));
    widget->usedThisFrame = true;
    widget->realignY      = false;
    widget->orderIndex    = widgetDirectParent->transientChildCount++;
    context->currentWidgetIndex = korl_checkCast_u$_to_i16(widgetIndex);
    /* disable this widget if any parent widgets in this hierarchy is unused */
    for(const i16* parentIndex = context->stbDaWidgetParentStack; parentIndex < stbDaWidgetParentStackEnd; parentIndex++)
        if(!context->stbDaWidgets[*parentIndex].usedThisFrame)
        {
            widget->usedThisFrame = false;
            break;
        }
    /* apply transient next widget modifiers */
    if(   !korl_math_isNanf32(context->transientNextWidgetModifiers.size.x)
       && !korl_math_isNanf32(context->transientNextWidgetModifiers.size.y))
        widget->size = context->transientNextWidgetModifiers.size;
        // do nothing otherwise, since the widget->size is transient & will potentially change due to widget logic at the end of each frame
    if(   !korl_math_isNanf32(context->transientNextWidgetModifiers.parentAnchor.x)
       && !korl_math_isNanf32(context->transientNextWidgetModifiers.parentAnchor.y))
        widget->parentAnchor = context->transientNextWidgetModifiers.parentAnchor;
    else
        widget->parentAnchor = KORL_MATH_V2F32_ZERO;
    if(   !korl_math_isNanf32(context->transientNextWidgetModifiers.parentOffset.x)
       && !korl_math_isNanf32(context->transientNextWidgetModifiers.parentOffset.y))
        widget->parentOffset = context->transientNextWidgetModifiers.parentOffset;
    else
        widget->parentOffset = (Korl_Math_V2f32){korl_math_nanf32(), korl_math_nanf32()};
    if(context->transientNextWidgetModifiers.orderIndex >= 0)
        widget->orderIndex = korl_checkCast_i$_to_u16(context->transientNextWidgetModifiers.orderIndex);
    _korl_gui_resetTransientNextWidgetModifiers();
    return widget;
}
korl_internal void _korl_gui_widget_destroy(_Korl_Gui_Widget*const widget)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    switch(widget->type)
    {
    case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
        break;}
    case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
        break;}
    case KORL_GUI_WIDGET_TYPE_TEXT:{
        // raw text is stored in the stack allocator each frame; no need to free it here
        if(widget->subType.text.gfxText)
            korl_gfx_text_destroy(widget->subType.text.gfxText);
        break;}
    case KORL_GUI_WIDGET_TYPE_BUTTON:{
        // raw text is stored in the stack allocator each frame; no need to free it here
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
    korl_memory_allocator_empty(context->allocatorHandleStack);
    context->stbDaWidgetParentStack = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), context->stbDaWidgetParentStack, 16);
    _korl_gui_resetTransientNextWidgetModifiers();
    context->currentWidgetIndex = -1;
}
korl_internal void _korl_gui_setNextWidgetSize(Korl_Math_V2f32 size)
{
    _korl_gui_context.transientNextWidgetModifiers.size = size;
}
korl_internal void _korl_gui_setNextWidgetParentAnchor(Korl_Math_V2f32 anchorRatioRelativeToParentTopLeft)
{
    _korl_gui_context.transientNextWidgetModifiers.parentAnchor = anchorRatioRelativeToParentTopLeft;
}
korl_internal void _korl_gui_setNextWidgetParentOffset(Korl_Math_V2f32 positionRelativeToAnchor)
{
    _korl_gui_context.transientNextWidgetModifiers.parentOffset = positionRelativeToAnchor;
}
korl_internal void _korl_gui_setNextWidgetOrderIndex(u16 orderIndex)
{
    _korl_gui_context.transientNextWidgetModifiers.orderIndex = orderIndex;
}
korl_internal void _korl_gui_widget_scrollBar_identifyMouseRegion(_Korl_Gui_Widget* widget, Korl_Math_V2f32 mousePosition)
{
    /* we need to re-calculate the scroll bar slider placement */
    // includes some copy-pasta from the SCROLL_BAR's frameEnd logic
    const Korl_Math_V2f32 sliderSize = widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y
                                       ? (Korl_Math_V2f32){widget->size.x, widget->size.y*widget->subType.scrollBar.visibleRegionRatio}
                                       : (Korl_Math_V2f32){widget->size.x*widget->subType.scrollBar.visibleRegionRatio, widget->size.y};
    const f32 sliderOffset = widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y
                             ? (widget->size.y - sliderSize.y)*widget->subType.scrollBar.scrollPositionRatio
                             : (widget->size.x - sliderSize.x)*widget->subType.scrollBar.scrollPositionRatio;
    const Korl_Math_V2f32 sliderPosition = widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y 
                                           ? (Korl_Math_V2f32){widget->position.x, widget->position.y - sliderOffset}
                                           : (Korl_Math_V2f32){widget->position.x + sliderOffset, widget->position.y};
    const Korl_Math_Aabb2f32 sliderAabb = korl_math_aabb2f32_fromPoints(sliderPosition.x, sliderPosition.y, sliderPosition.x + sliderSize.x, sliderPosition.y - sliderSize.y);
    /* calculate which region of the scroll bar the mouse clicked on (slider, pre-slider, or post-slider) */
    if(korl_math_aabb2f32_containsV2f32(sliderAabb, mousePosition))
        widget->subType.scrollBar.mouseDownRegion = KORL_GUI_SCROLL_BAR_REGION_SLIDER;
    else
        switch(widget->subType.scrollBar.axis)
        {
        case KORL_GUI_SCROLL_BAR_AXIS_X:{
            if(mousePosition.x < sliderAabb.min.x)
                widget->subType.scrollBar.mouseDownRegion = KORL_GUI_SCROLL_BAR_REGION_PRE_SLIDER;
            else
                widget->subType.scrollBar.mouseDownRegion = KORL_GUI_SCROLL_BAR_REGION_POST_SLIDER;
            break;}
        case KORL_GUI_SCROLL_BAR_AXIS_Y:{
            if(mousePosition.y > sliderAabb.max.y)
                widget->subType.scrollBar.mouseDownRegion = KORL_GUI_SCROLL_BAR_REGION_PRE_SLIDER;
            else
                widget->subType.scrollBar.mouseDownRegion = KORL_GUI_SCROLL_BAR_REGION_POST_SLIDER;
            break;}
        }
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
    _korl_gui_context.style.colorScrollBar                 = (Korl_Vulkan_Color4u8){  8,   8,   8, 230};
    _korl_gui_context.style.colorScrollBarActive           = (Korl_Vulkan_Color4u8){ 32,  32,  32, 250};
    _korl_gui_context.style.colorScrollBarPressed          = (Korl_Vulkan_Color4u8){  0,   0,   0, 250};
    _korl_gui_context.style.colorText                      = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    _korl_gui_context.style.colorTextOutline               = (Korl_Vulkan_Color4u8){  0,   5,   0, 255};
    _korl_gui_context.style.textOutlinePixelSize           = 0.f;
    _korl_gui_context.style.fontWindowText                 = string_newEmptyUtf16(0);
    _korl_gui_context.style.windowTextPixelSizeY           = 24.f;
    _korl_gui_context.style.windowTitleBarPixelSizeY       = 20.f;
    _korl_gui_context.style.widgetSpacingY                 =  0.f;
    _korl_gui_context.style.widgetButtonLabelMargin        =  4.f;
    _korl_gui_context.style.windowScrollBarPixelWidth      = 15.f;
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
        /* clear all widget hover flags */
        for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
            usedWidget->widget->isHovered = false;
        /**/
        _Korl_Gui_UsedWidget* draggedUsedWidget = NULL;
        _Korl_Gui_Widget* draggedWidget = NULL;///@TODO: delete in favor of UsedWidget
        if(context->identifierHashWidgetDragged)
        {
            for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
                if(usedWidget->widget->identifierHash == context->identifierHashWidgetDragged)
                {
                    draggedUsedWidget = usedWidget;
                    draggedWidget = usedWidget->widget;
                    break;
                }
            if(!draggedWidget)
                /* for whatever reason, we can't find the id hash of the dragged 
                    widget anymore, so let's just invalidate this id hash */
                context->identifierHashWidgetDragged = 0;
        }
        if(draggedWidget)
            if(context->mouseHoverWindowEdgeFlags)
            {
                korl_assert(draggedWidget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
                Korl_Math_Aabb2f32 aabb = draggedUsedWidget->transient.aabb;
                /* adjust the AABB values based on which edge flags we're controlling, 
                    & ensure that the final AABB is valid (not too small) */
                if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_LEFT)
                    aabb.min.x = KORL_MATH_MIN(mouseEvent->subType.move.position.x, aabb.max.x - context->style.windowTitleBarPixelSizeY);
                if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_RIGHT)
                    aabb.max.x = KORL_MATH_MAX(mouseEvent->subType.move.position.x, aabb.min.x + context->style.windowTitleBarPixelSizeY);
                if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_UP)
                    aabb.max.y = KORL_MATH_MAX(mouseEvent->subType.move.position.y, aabb.min.y + context->style.windowTitleBarPixelSizeY);
                if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_DOWN)
                    aabb.min.y = KORL_MATH_MIN(mouseEvent->subType.move.position.y, aabb.max.y - context->style.windowTitleBarPixelSizeY);
                /* set the window position/size based on the new AABB */
                draggedWidget->position.x = aabb.min.x;
                draggedWidget->position.y = aabb.max.y;
                draggedWidget->size.x     = aabb.max.x - aabb.min.x;
                draggedWidget->size.y     = aabb.max.y - aabb.min.y;
                draggedUsedWidget->transient.aabb = aabb;
            }
            else
            {
                draggedUsedWidget->transient.aabb.min.x = mouseEvent->subType.move.position.x + context->mouseDownWidgetOffset.x;
                draggedUsedWidget->transient.aabb.max.y = mouseEvent->subType.move.position.y + context->mouseDownWidgetOffset.y;
                draggedWidget->position = (Korl_Math_V2f32){draggedUsedWidget->transient.aabb.min.x, draggedUsedWidget->transient.aabb.max.y};
            }
        else
        {
            context->identifierHashWindowHovered = 0;
            context->mouseHoverWindowEdgeFlags   = 0;
        }
        // mouse hover logic //
        {
            /* iterate all widgets front=>back to detect hover events */
            bool continueRayCast = true;
            for(_Korl_Gui_UsedWidget* usedWidget = KORL_C_CAST(_Korl_Gui_UsedWidget*, stbDaUsedWidgetsEnd - 1); 
                usedWidget >= context->stbDaUsedWidgets && continueRayCast; usedWidget--)
            {
                _Korl_Gui_Widget*const widget = usedWidget->widget;
                const Korl_Math_Aabb2f32 widgetAabb = usedWidget->transient.aabb;
                switch(widget->type)
                {
                case KORL_GUI_WIDGET_TYPE_WINDOW:{
                    const bool isResizableWindow = widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE;
                    Korl_Math_Aabb2f32 widgetAabbExpanded = widgetAabb;
                    if(isResizableWindow)
                        korl_math_aabb2f32_expand(&widgetAabbExpanded, _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS);
                    if(korl_math_aabb2f32_containsV2f32(widgetAabbExpanded, mouseEvent->subType.move.position))
                    {
                        context->identifierHashWindowHovered = widget->identifierHash;
                        widget->isHovered = true;
                        if(   isResizableWindow 
                           && !context->identifierHashWidgetMouseDown
                           && !korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.move.position))
                        {
                            /* if the mouse isn't contained in the normal AABB (unexpanded) of the window, 
                                that means we _must_ be in a resize region along the edge of the window */
                            if(!widget->isContentHidden)// disable top/bottom resizing when window content is hidden
                            {
                                if(mouseEvent->subType.move.position.y >= widgetAabb.max.y)
                                    context->mouseHoverWindowEdgeFlags |= KORL_GUI_EDGE_FLAG_UP;
                                if(mouseEvent->subType.move.position.y <= widgetAabb.min.y)
                                    context->mouseHoverWindowEdgeFlags |= KORL_GUI_EDGE_FLAG_DOWN;
                            }
                            if(mouseEvent->subType.move.position.x >= widgetAabb.max.x)
                                context->mouseHoverWindowEdgeFlags |= KORL_GUI_EDGE_FLAG_RIGHT;
                            if(mouseEvent->subType.move.position.x <= widgetAabb.min.x)
                                context->mouseHoverWindowEdgeFlags |= KORL_GUI_EDGE_FLAG_LEFT;
                        }
                        continueRayCast = false;// windows are opaque, ergo we stop the hover ray cast
                    }
                    break;}
                case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
                    if(context->identifierHashWidgetMouseDown != widget->identifierHash)
                        _korl_gui_widget_scrollBar_identifyMouseRegion(widget, mouseEvent->subType.move.position);
                    /*fallthrough to default case*/}
                default:{
                    if(korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.move.position))
                        widget->isHovered = true;
                    break;}
                }
            }
        }
        break;}
    case _KORL_GUI_MOUSE_EVENT_TYPE_BUTTON:{
        if(mouseEvent->subType.button.pressed)
        {
            context->isTopLevelWindowActive        = false;
            context->identifierHashWidgetMouseDown = 0;
            /* iterate over all widgets from front=>back */
            for(_Korl_Gui_UsedWidget* usedWidget = KORL_C_CAST(_Korl_Gui_UsedWidget*, stbDaUsedWidgetsEnd - 1); usedWidget >= context->stbDaUsedWidgets; usedWidget--)
            {
                _Korl_Gui_Widget*const widget = usedWidget->widget;
                Korl_Math_Aabb2f32 widgetAabb = usedWidget->transient.aabb;
                if(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW && widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE)
                    korl_math_aabb2f32_expand(&widgetAabb, _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS);
                if(korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.button.position))
                {
                    bool widgetCanMouseDown = false;
                    bool eventCaptured      = false;
                    switch(widget->type)
                    {
                    case KORL_GUI_WIDGET_TYPE_WINDOW:{
                        korl_assert(!widget->identifierHashParent);// simplification; windows are always top-level (no child windows)
                        if(!context->identifierHashWidgetMouseDown)
                            context->identifierHashWidgetDragged = widget->identifierHash;// right now, _only_ windows can be dragged!
                        widget->orderIndex = ++context->rootWidgetOrderIndexHighest;/* set widget's order to be in front of all other widgets */
                        korl_assert(context->rootWidgetOrderIndexHighest);// check integer overflow
                        /* we _could_ just re-sort the entire UsedWidget list, but this is an expensive/complicated process that I 
                            would rather only do once per frame, so instead we can take advantage of a key assumption here: 
                            - we can safely assume that stbDaUsedWidgets is currently sorted topologically, such that each widget 
                              sub-tree is contiguous in memory!
                            With this knowledge, all we have to do to maintain the topological sort for the rest of the frame is 
                            just move this entire widget sub-tree to the end of stbDaUsedWidgets. */
                        // iterate over stbDaUsedWidgets (starting at usedWidget) until we encounter another top-level widget _or_ the end iterator;
                        //  this will give us the total # of UsedWidgets in this widget sub-tree
                        _Korl_Gui_UsedWidget* subTreeEnd = usedWidget + 1;
                        for(; subTreeEnd < stbDaUsedWidgetsEnd; subTreeEnd++)
                            if(subTreeEnd->dagMetaData.depth <= 0)
                                break;
                        if(subTreeEnd < stbDaUsedWidgetsEnd)// no need to do any of this logic if this sub-tree is already at the end of UsedWidgets
                        {
                            const u$ subTreeSize = subTreeEnd - usedWidget;
                            // copy these UsedWidgets to a temp buffer
                            _Korl_Gui_UsedWidget* stbDaTempSubTree = NULL;
                            mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaTempSubTree, subTreeSize);
                            korl_memory_copy(stbDaTempSubTree, usedWidget, subTreeSize*sizeof(*usedWidget));
                            // move all stbDaUsedWidgets that come after this sub-tree in memory down to usedWidget's position
                            korl_memory_move(usedWidget, subTreeEnd, (stbDaUsedWidgetsEnd - subTreeEnd)*sizeof(*usedWidget));
                            // copy the temp buffer UsedWidgets to the end of stbDaUsedWidgets
                            korl_memory_copy(subTreeEnd, stbDaTempSubTree, subTreeSize*sizeof(*usedWidget));
                        }
                        context->isTopLevelWindowActive = true;
                        eventCaptured      = true;
                        widgetCanMouseDown = true;
                        break;}
                    case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
                        break;}
                    case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
                        _korl_gui_widget_scrollBar_identifyMouseRegion(widget, mouseEvent->subType.button.position);
                        widgetCanMouseDown = true;
                        break;}
                    case KORL_GUI_WIDGET_TYPE_TEXT:{
                        break;}
                    case KORL_GUI_WIDGET_TYPE_BUTTON:{
                        widgetCanMouseDown = true;
                        break;}
                    default:{
                        korl_log(ERROR, "invalid widget type: %i", widget->type);
                        break;}
                    }
                    if(widgetCanMouseDown && !context->identifierHashWidgetMouseDown)// we mouse down on _only_ the first widget
                    {
                        context->identifierHashWidgetMouseDown = widget->identifierHash;
                        context->mouseDownWidgetOffset         = korl_math_v2f32_subtract(widget->position, mouseEvent->subType.button.position);
                    }
                    if(eventCaptured)
                        break;// stop processing widgets when the event is captured
                }
            }
        }
        else// mouse button released; on-click logic
        {
            /* in order to an "on-click" event to actuate, we must have already 
                have a "mouse-down" registered on the widget, _and_ the mouse 
                must still be in the bounds of the widget */
            _Korl_Gui_Widget*     clickedWidget     = NULL;///@TODO: cleanup; redundant
            _Korl_Gui_UsedWidget* clickedUsedWidget = NULL;
            if(context->identifierHashWidgetMouseDown)
                for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
                    if(usedWidget->widget->identifierHash == context->identifierHashWidgetMouseDown)
                    {
                        clickedWidget     = usedWidget->widget;
                        clickedUsedWidget = usedWidget;
                        break;
                    }
            if(clickedWidget)
            {
                const Korl_Math_Aabb2f32 widgetAabb = clickedUsedWidget->transient.aabb;
                if(korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.button.position))
                {
                    switch(clickedWidget->type)
                    {
                    case KORL_GUI_WIDGET_TYPE_WINDOW:{
                        break;}
                    case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
                        break;}
                    case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
                        break;}
                    case KORL_GUI_WIDGET_TYPE_TEXT:{
                        break;}
                    case KORL_GUI_WIDGET_TYPE_BUTTON:{
                        if(clickedWidget->subType.button.actuationCount < KORL_U8_MAX)// silently discard on-click events if we would integer overflow
                            clickedWidget->subType.button.actuationCount++;
                        break;}
                    default:{
                        korl_log(ERROR, "invalid widget type: %i", clickedWidget->type);
                        break;}
                    }
                }
            }
            /* reset mouse input state */
            context->identifierHashWidgetMouseDown = 0;
            context->identifierHashWidgetDragged   = 0;
        }
        break;}
    case _KORL_GUI_MOUSE_EVENT_TYPE_WHEEL:{
        _Korl_Gui_MouseEvent mouseEventPrime = *mouseEvent;// used for modifications to the mouseEvent based on non-captured widget event logic from previous widgets
        /* iterate over all widgets from front=>back */
        for(_Korl_Gui_UsedWidget* usedWidget = KORL_C_CAST(_Korl_Gui_UsedWidget*, stbDaUsedWidgetsEnd - 1); usedWidget >= context->stbDaUsedWidgets; usedWidget--)
        {
            _Korl_Gui_Widget*const widget = usedWidget->widget;
            Korl_Math_Aabb2f32 widgetAabb = usedWidget->transient.aabb;
            if(korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.button.position))
            {
                bool eventCaptured = false;// when this is set, we stop iterating over widgets & end the input event
                switch(widget->type)
                {
                case KORL_GUI_WIDGET_TYPE_WINDOW:{
                    eventCaptured = true;
                    break;}
                case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
                    switch(mouseEventPrime.subType.wheel.axis)
                    {
                    case _KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_X:{
                        widget->subType.scrollArea.contentOffset.x += mouseEvent->subType.wheel.value * context->style.windowTextPixelSizeY;
                        break;}
                    case _KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_Y:{
                        widget->subType.scrollArea.contentOffset.y -= mouseEvent->subType.wheel.value * context->style.windowTextPixelSizeY;
                        break;}
                    }
                    break;}
                case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
                    switch(widget->subType.scrollBar.axis)
                    {
                    case KORL_GUI_SCROLL_BAR_AXIS_X:{
                        mouseEventPrime.subType.wheel.axis = _KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_X;
                        break;}
                    case KORL_GUI_SCROLL_BAR_AXIS_Y:{
                        mouseEventPrime.subType.wheel.axis = _KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_Y;
                        break;}
                    }
                    break;}
                case KORL_GUI_WIDGET_TYPE_TEXT:{
                    break;}
                case KORL_GUI_WIDGET_TYPE_BUTTON:{
                    break;}
                default:{
                    korl_log(ERROR, "invalid widget type: %i", widget->type);
                    break;}
                }
                if(eventCaptured)
                    break;// stop processing widgets when the event is captured
            }
        }
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
    ///@TODO: there are a lot of similarities here to _getWidget; in an effort to generalize Window/Widget behavior, maybe I should just call _getWidget here???
    _Korl_Gui_Context*const context = &_korl_gui_context;
    const Korl_Math_V2f32 defaultMinimumSize = {2*/*@TODO: kinda hacky to fix auto-sizing issue for title bar buttons*/context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY};// default _and_ minimum window size
    i16 currentWindowIndex = -1;
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
    if(arrlen(context->stbDaWidgetParentStack) > 0)
    {
        korl_assert(arrlen(context->stbDaWidgetParentStack) > 0);
        const _Korl_Gui_Widget*const rootWidget = context->stbDaWidgets + context->stbDaWidgetParentStack[0];
        korl_assert(rootWidget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
        korl_assert(rootWidget->identifierHash == _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH);
        if(identifierHash != _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH)
            korl_gui_windowEnd();// end the orphan widget window again, if we are making a different window; this is obviously weird, but theoretically the orphan widget window's SCROLL_AREA widget will be obtained & the widget hierarchy will be correctly maintained since we are using the same exact memory location for the SCROLL_AREA's `label` parameter (hopefully)
    }
    /* check to see if this identifier is already registered */
    const _Korl_Gui_Widget*const widgetsEnd = context->stbDaWidgets + arrlen(context->stbDaWidgets);
    for(_Korl_Gui_Widget* widget = context->stbDaWidgets; widget < widgetsEnd; widget++)
    {
        if(widget->identifierHash == identifierHash)
        {
            korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
            if(!widget->subType.window.isOpen && out_isOpen && *out_isOpen)
            {
                widget->subType.window.isOpen = true;
                widget->isContentHidden       = false;
            }
            currentWindowIndex = korl_checkCast_i$_to_i16(widget - context->stbDaWidgets);
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
            nextWindowPosition   = korl_math_v2f32_add(widget->position, (Korl_Math_V2f32){ 32.f, 32.f });//offset from the current top-level window by this amount
            nextWindowOrderIndex = widget->orderIndex + 1;
            korl_assert(nextWindowOrderIndex);// sanity check integer overflow
        }
    }
    currentWindowIndex = korl_checkCast_u$_to_i16(arrlenu(context->stbDaWidgets));
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaWidgets, (_Korl_Gui_Widget){0});
    _Korl_Gui_Widget* newWindow = &context->stbDaWidgets[currentWindowIndex];
    newWindow->identifierHash              = identifierHash;
    newWindow->position                    = nextWindowPosition;
    newWindow->orderIndex                  = nextWindowOrderIndex;
    newWindow->size                        = defaultMinimumSize;
    newWindow->subType.window.titleBarText = string_newUtf16(titleBarText);
    newWindow->subType.window.isFirstFrame = true;
    newWindow->subType.window.isOpen       = true;
    done_currentWindowIndexValid:
        newWindow = &context->stbDaWidgets[currentWindowIndex];
        newWindow->subType.window.titleBarButtonCount = 0;
        if(arrlen(context->stbDaWidgetParentStack) == 0)
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), context->stbDaWidgetParentStack, currentWindowIndex);
        else// special case; we're getting the internal debug window again
        {
            korl_assert(arrlast(context->stbDaWidgetParentStack) == currentWindowIndex);
            korl_assert(newWindow->identifierHash == _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH);
        }
        newWindow->usedThisFrame             = newWindow->subType.window.isOpen;// if the window isn't open, this entire widget sub-tree is unused for this frame
        newWindow->subType.window.styleFlags = styleFlags;
        korl_shared_const Korl_Math_V2f32 TITLE_BAR_BUTTON_ANCHOR = {1, 0};// set title bar buttons relative to the window's upper-right corner
        Korl_Math_V2f32 titleBarButtonCursor = (Korl_Math_V2f32){-context->style.windowTitleBarPixelSizeY, 0.f};
        if(out_isOpen)
        {
            _korl_gui_setNextWidgetSize((Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
            _korl_gui_setNextWidgetParentAnchor(TITLE_BAR_BUTTON_ANCHOR);
            _korl_gui_setNextWidgetParentOffset(titleBarButtonCursor);
            korl_math_v2f32_assignAdd(&titleBarButtonCursor, (Korl_Math_V2f32){-context->style.windowTitleBarPixelSizeY, 0.f});
            if(korl_gui_widgetButtonFormat(_KORL_GUI_WIDGET_BUTTON_WINDOW_CLOSE))
                newWindow->subType.window.isOpen = false;
            *out_isOpen = newWindow->subType.window.isOpen;
            newWindow->subType.window.titleBarButtonCount++;
        }
        if(styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            _korl_gui_setNextWidgetSize((Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
            _korl_gui_setNextWidgetParentAnchor(TITLE_BAR_BUTTON_ANCHOR);
            _korl_gui_setNextWidgetParentOffset(titleBarButtonCursor);
            korl_math_v2f32_assignAdd(&titleBarButtonCursor, (Korl_Math_V2f32){-context->style.windowTitleBarPixelSizeY, 0.f});
            if(korl_gui_widgetButtonFormat(_KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZE))
            {
                newWindow->isContentHidden = !newWindow->isContentHidden;
                if(newWindow->isContentHidden)
                    newWindow->hiddenContentPreviousSizeY = newWindow->size.y;
                else
                    newWindow->size.y = newWindow->hiddenContentPreviousSizeY;
            }
            newWindow->subType.window.titleBarButtonCount++;
        }
        context->currentWidgetIndex = -1;
        /* add scroll area if this window allows it */
        if(styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE)
            korl_gui_widgetScrollAreaBegin(KORL_RAW_CONST_UTF16(L"KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE"));
}
korl_internal KORL_FUNCTION_korl_gui_windowEnd(korl_gui_windowEnd)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(arrlen(context->stbDaWidgetParentStack) >= 0);
    _Korl_Gui_Widget*const rootWidget = context->stbDaWidgets + context->stbDaWidgetParentStack[0];
    korl_assert(rootWidget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
    if(rootWidget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE)
        korl_gui_widgetScrollAreaEnd();
    korl_assert(arrlen(context->stbDaWidgetParentStack) == 1);
    arrpop(context->stbDaWidgetParentStack);
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
/** This function is called from within \c frameEnd as soon as we are in a 
 * state where we are absolutely sure that all of the children of \c usedWidget 
 * have been processed & drawn for the end of this frame. */
korl_internal void _korl_gui_frameEnd_onUsedWidgetChildrenProcessed(_Korl_Gui_UsedWidget* usedWidget)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    switch(usedWidget->widget->type)
    {
    case KORL_GUI_WIDGET_TYPE_WINDOW:{
        /* if a WINDOW widget just finished its first frame, auto-size it to fit all of its contents */
        if(usedWidget->transient.isFirstFrame)
            usedWidget->transient.aabb = usedWidget->transient.aabbContent;
        usedWidget->widget->size = korl_math_aabb2f32_size(usedWidget->transient.aabb);
        break;}
    case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
        /* bind the scroll region to the "maximum" planes, which are defined by 
            the difference between the contentAabb & the transient aabb */
        const Korl_Math_V2f32 size         = korl_math_aabb2f32_size(usedWidget->transient.aabb);
        const Korl_Math_V2f32 childrenSize = korl_math_aabb2f32_size(usedWidget->transient.aabbChildren);
        Korl_Math_V2f32 scrollSize = childrenSize;
        if(usedWidget->widget->subType.scrollArea.hasScrollBarX)
            scrollSize.y += context->style.windowScrollBarPixelWidth;
        if(usedWidget->widget->subType.scrollArea.hasScrollBarY)
            scrollSize.x += context->style.windowScrollBarPixelWidth;
        const Korl_Math_V2f32 clippedSize = korl_math_v2f32_subtract(scrollSize, size);
        KORL_MATH_ASSIGN_CLAMP_MIN(usedWidget->widget->subType.scrollArea.contentOffset.x, -clippedSize.x);
        KORL_MATH_ASSIGN_CLAMP_MAX(usedWidget->widget->subType.scrollArea.contentOffset.y,  clippedSize.y);
        /* bind the scroll region to the "minimum" planes on each axis */
        KORL_MATH_ASSIGN_CLAMP_MAX(usedWidget->widget->subType.scrollArea.contentOffset.x, 0);
        KORL_MATH_ASSIGN_CLAMP_MIN(usedWidget->widget->subType.scrollArea.contentOffset.y, 0);
        /* cache the size of aabbChildren for the top of next frame, when the 
            widget needs to decide whether or not it even needs to spawn 
            SCROLL_BAR widgets */
        usedWidget->widget->subType.scrollArea.aabbChildrenSize = childrenSize;
        break;}// widget->size is determined in frameEnd, when the SCROLL_AREA is sized to fill its parent's remaining content area
    default:{
        /* for all other types of widgets, use the updated content AABB to determine 
            the widget's up-to-date size; the size we obtain here will be used to 
            initialize the transient `aabb` at the end of the next frame */
        usedWidget->widget->size = korl_math_aabb2f32_size(usedWidget->transient.aabbContent);
        break;}
    }
}
korl_internal void korl_gui_frameEnd(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    /* Once again, the only time the current window index is allowed to be set 
        to a valid id at this point is if the user is making orphan widgets. */
    if(arrlen(context->stbDaWidgetParentStack) > 0)
    {
        korl_assert(arrlen(context->stbDaWidgetParentStack) > 0);
        const _Korl_Gui_Widget*const rootWidget = context->stbDaWidgets + context->stbDaWidgetParentStack[0];
        korl_assert(rootWidget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
        korl_assert(rootWidget->identifierHash == _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH);
        korl_gui_windowEnd();
    }
    /* Initial loop over _all_ widgets to perform the following tasks: 
        - nullify widgets which weren't used this frame, _excluding_ root/window widgets!
        - gather a list of all widgets used this frame in preparation to perform transient topological/order sorting
        - figure out which root widget is "highest"/"top-level" (last to draw)
        - build an acceleration structure to map widgetIdHash=>Widget
            - this can be used to ensure that all id hash values are unique
            - also useful for iterating quickly through the Widget DAG */
    mcarrfree  (KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaUsedWidgets);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaUsedWidgets, arrlen(context->stbDaWidgets));
    _Korl_Gui_WidgetMap* stbHmWidgetMap = NULL;
    mchmdefault(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap, KORL_U64_MAX);
    u$ widgetsRemaining = 0;
    korl_time_probeStart(nullify_unused_widgets);
    const _Korl_Gui_Widget*const widgetsEnd = context->stbDaWidgets + arrlen(context->stbDaWidgets);
    for(_Korl_Gui_Widget* widget = context->stbDaWidgets; widget < widgetsEnd; widget++)
    {
        const u$ i = widget - context->stbDaWidgets;
        korl_assert(widget->identifierHash);
        if(widget->usedThisFrame || !widget->identifierHashParent/*even if they are unused this frame, we still retain all root widgets!*/)
        {
            if(widgetsRemaining < i)
                context->stbDaWidgets[widgetsRemaining] = *widget;
            if(widget->usedThisFrame)
            {
                KORL_ZERO_STACK(_Korl_Gui_UsedWidget, newUsedWidget);
                newUsedWidget.widget = context->stbDaWidgets + widgetsRemaining;
                mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaUsedWidgets, newUsedWidget);
                korl_assert(mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap, newUsedWidget.widget->identifierHash) < 0);// ensure that this widget's id hash is unique!
                mchmput(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap, newUsedWidget.widget->identifierHash, arrlenu(context->stbDaUsedWidgets) - 1);
            }
            widgetsRemaining++;
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
    /* now that we have a list of widgets that are used this frame, we can 
        gather the meta data necessary to perform a topological sort of the 
        entire list, which will allow us to process the entire widget DAG from 
        back=>front 
        Example DAG:
        - {WINDOW[0], depth=0, order=0, childrenDirect=2, childrenTotal=4}
            - {BUTTON[0], depth=1, order=0, childrenDirect=0, childrenTotal=0}
            - {PANEL[0], depth=1, order=1, childrenDirect=2, childrenTotal=2}
                - {TEXT[0], depth=2, order=0, childrenDirect=0, childrenTotal=0}
                - {TEXT[1], depth=2, order=1, childrenDirect=0, childrenTotal=0}
        - {WINDOW[1], depth=0, order=1, childrenDirect=1, childrenTotal=1}
            - {BUTTON[1], depth=1, order=0, childrenDirect=0, childrenTotal=0}
        Sort approach:
        - for each UsedWidget, recursively find depth,childrenTotal; O(???)
        - initialize a new UsedWidget array of the same size with all NULL entries; let's call this usedSorted
        - start at depth=0
        - while arrlenu(stbDaUsedWidgets)
            - sort by depth (ascending); O(NlogN)
            - count how many widgets are at depth; O(N)
            - sort this sub-array by order (ascending); O(NlogN)
            - iterate over usedSorted until we find the first NULL entry; O(N)
            - for each sub-array entry
                - copy this data into usedSorted[currentNull]
                - currentNull += subArray[i].childrenTotal
            - set the new beginning of stbDaUsedWidgets to be the END iterator of subArray
            - depth++, repeat
        Sort Approach 2: (currently using this approach)
        - for each UsedWidget, recursively find depth; O(N)
        - sort stbDaUsedWidgets by depth, then by order; O(NlogN)
        - initialize new UsedWidget array (same capacity, but empty); called usedSorted
        - for i=0 in stbDaUsedWidgets O(N^2)
            - if the widget has a parent
                - break
            - initialize empty set of widget hash Ids
            - add this widget's hash Id to the set
            - add this widget to usedSorted
            - for j=i+1 in stbDaUsedWidgets
                - if this widget has a parent & the parent is in the set
                    - add this widget's hash Id to the set
                    - add this widget to usedSorted
        */
    // recursively find the DAG depth of each UsedWidget //
    const _Korl_Gui_UsedWidget*const usedWidgetsEnd = context->stbDaUsedWidgets + arrlen(context->stbDaUsedWidgets);
    for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
        _korl_gui_findUsedWidgetDepthRecursive(usedWidget, &stbHmWidgetMap);
    // sort the UsedWidgets by depth, then by order //
    _korl_gui_usedWidget_quick_sort(context->stbDaUsedWidgets, arrlenu(context->stbDaUsedWidgets));
    mchmfree(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap);// the map from hash=>index is now invalid!
    // topologically sort stbDaUsedWidgets using the above data //
    _Korl_Gui_UsedWidget* usedWidgetSortIterator = context->stbDaUsedWidgets;
    _Korl_Gui_UsedWidget* stbDaUsedWidgetsTempCopy = NULL;
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaUsedWidgetsTempCopy, arrlenu(context->stbDaUsedWidgets));
    korl_memory_copy(stbDaUsedWidgetsTempCopy, context->stbDaUsedWidgets, arrlenu(context->stbDaUsedWidgets)*sizeof(*context->stbDaUsedWidgets));
    const _Korl_Gui_UsedWidget*const stbDaUsedWidgetsTempCopyEnd = stbDaUsedWidgetsTempCopy + arrlen(stbDaUsedWidgetsTempCopy);
    // @TODO: PERFORMANCE; this can be optimized from O(N^2) to O(N) if we carefully remove visited UsedWidget entries from the temp copy list without invalidating iterators in the parent loop
    for(_Korl_Gui_UsedWidget* usedWidgetTempCopy = stbDaUsedWidgetsTempCopy; usedWidgetTempCopy < stbDaUsedWidgetsTempCopyEnd; usedWidgetTempCopy++)
    {
        if(usedWidgetTempCopy->widget->identifierHashParent)
            break;
        mchmfree(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap);// we're going to re-use this map to keep track of which widgets are in each root widget sub-tree
        mchmdefault(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap, KORL_U64_MAX);
        mchmput(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap, usedWidgetTempCopy->widget->identifierHash, KORL_U64_MAX);
        *usedWidgetSortIterator = *usedWidgetTempCopy;
        usedWidgetSortIterator++;
        for(_Korl_Gui_UsedWidget* usedWidgetTempCopy2 = usedWidgetTempCopy + 1; usedWidgetTempCopy2 < stbDaUsedWidgetsTempCopyEnd; usedWidgetTempCopy2++)
        {
            if(   usedWidgetTempCopy2->widget->identifierHashParent 
               && mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap, usedWidgetTempCopy2->widget->identifierHashParent) >= 0)
            {
                mchmput(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap, usedWidgetTempCopy2->widget->identifierHash, KORL_U64_MAX);
                *usedWidgetSortIterator = *usedWidgetTempCopy2;
                usedWidgetSortIterator++;
            }
        }
    }
    /* sanity-check the sorted list of used widgets to make sure that for each 
        widget sub tree, all child orderIndex values are unique */
    //@TODO
    /* prepare the view/projection graphics state */
    const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize();
    Korl_Gfx_Camera cameraOrthographic = korl_gfx_createCameraOrtho(korl_checkCast_i$_to_f32(arrlen(context->stbDaUsedWidgets) + 1/*+1 so the back widget will still be _above_ the rear clip plane*/)/*clipDepth; each used widget can have its own integral portion of the depth buffer, so if we have 2 widgets, the first can be placed at depth==-2, and the second can be placed at depth==-1; individual graphics components at each depth can safely be placed within this range without having to worry about interfering with other widget graphics, since we have already sorted the widgets from back=>front*/);
    cameraOrthographic.position.xy = 
    cameraOrthographic.target.xy   = (Korl_Math_V2f32){surfaceSize.x/2.f, surfaceSize.y/2.f};
    korl_gfx_useCamera(cameraOrthographic);
#if KORL_DEBUG && 0/*@TODO: this is just some code to test out rendering with the depth buffer; should be okay to delete this at any time*/
    {
        Korl_Vulkan_Color4u8 colors[] = {KORL_COLOR4U8_RED, KORL_COLOR4U8_GREEN, KORL_COLOR4U8_BLUE, KORL_COLOR4U8_YELLOW, KORL_COLOR4U8_CYAN, KORL_COLOR4U8_MAGENTA};
        Korl_Gfx_Batch*const debugBatch = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, (Korl_Math_V2f32){69,69}, (Korl_Math_V2f32){0,0}, (Korl_Vulkan_Color4u8){255,255,255,255});
        for(u$ i = 0; i < arrlenu(context->stbDaWidgets); i++)
        {
            const f32 position = i*20.f;
            korl_gfx_batchRectangleSetColor(debugBatch, colors[i%korl_arraySize(colors)]);
            korl_gfx_batchSetPosition(debugBatch, (f32[]){position, position, -KORL_C_CAST(f32, i)}, 3);
            korl_gfx_batch(debugBatch, KORL_GFX_BATCH_FLAGS_NONE);
        }
    }
#endif
    /* process _all_ sorted in-use widgets for this frame */
    korl_shared_const Korl_Math_V2f32 ORIGIN_RATIO_UPPER_LEFT = {0, 1};// remember, +Y is UP!
    #ifdef _KORL_GUI_DEBUG_DRAW_COORDINATE_FRAMES
        Korl_Gfx_Batch*const batchLinesOrigin2d = korl_gfx_createBatchLines(context->allocatorHandleStack, 2);
        korl_gfx_batchSetLine(batchLinesOrigin2d, 0, KORL_MATH_V2F32_ZERO.elements, KORL_MATH_V2F32_X.elements, 2, KORL_COLOR4U8_RED);
        korl_gfx_batchSetLine(batchLinesOrigin2d, 1, KORL_MATH_V2F32_ZERO.elements, KORL_MATH_V2F32_Y.elements, 2, KORL_COLOR4U8_GREEN);
    #endif
    u16 rootWidgetCount = 0;// used to normalize root widget orderIndex values, since it is possible to fragment these values at any time
    _Korl_Gui_UsedWidget** stbDaUsedWidgetStack = NULL;// used to perform logic based on each widget's parent; all logic with this is done under the assumption that the list of widgets has been topologically sorted
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaUsedWidgetStack, 16);
    for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
    {
        const u$  w = usedWidget - context->stbDaUsedWidgets;// simple index into the used widget array, so we don't have to keep calculating this value for Z placement of graphics components
        const f32 z = -korl_checkCast_u$_to_f32(arrlen(context->stbDaUsedWidgets) - w);// convert index to depth; remember, the widgets with a lower index appear _behind_ successive widgets
        usedWidget->transient.aabbChildren = KORL_MATH_AABB2F32_EMPTY;
        _Korl_Gui_Widget*const widget = usedWidget->widget;
        Korl_Math_V2f32 childWidgetCursorOffset = KORL_MATH_V2F32_ZERO;// _not_ used to determine widget's position; this will determine the positions of widget's children in later iterations
        /* pop widgets from the stack until this widget's parent is on the top (or there stack is empty);
            we can do this because context->stbDaUsedWidgets should have been sorted topologically */
        while(arrlenu(stbDaUsedWidgetStack) && arrlast(stbDaUsedWidgetStack)->widget->identifierHash != widget->identifierHashParent)
            /* when a widget is popped from this stack, it _must_ be the case 
                that _all_ of its children have been processed, so we can do 
                special logic right now on this widget, such as auto-resizing to 
                fit all the content! */
            _korl_gui_frameEnd_onUsedWidgetChildrenProcessed(arrpop(stbDaUsedWidgetStack));
        _Korl_Gui_UsedWidget*const usedWidgetParent = arrlenu(stbDaUsedWidgetStack) ? arrlast(stbDaUsedWidgetStack) : NULL;
        const _Korl_Gui_Widget*const widgetParent = arrlenu(stbDaUsedWidgetStack) ? arrlast(stbDaUsedWidgetStack)->widget : NULL;// @TODO: make this the UsedWidget, and remove the `arrlast(stbDaUsedWidgetStack)` code occurances below
        /* determine where the widget's origin will be in world-space */
        const bool useParentWidgetCursor = korl_math_isNanf32(widget->parentOffset.x) || korl_math_isNanf32(widget->parentOffset.y);
        if(widgetParent)
        {
            const Korl_Math_V2f32 parentAnchor = korl_math_v2f32_add(widgetParent->position
                                                                    ,korl_math_v2f32_multiply(widget->parentAnchor
                                                                                             ,(Korl_Math_V2f32){ widgetParent->size.x
                                                                                                               ,-widgetParent->size.y/*inverted, since +Y is UP, & the parent's position is its top-left corner*/}));
            if(useParentWidgetCursor)
                widget->position = korl_math_v2f32_add(parentAnchor, korl_math_v2f32_add(usedWidgetParent->transient.childWidgetCursorOrigin, usedWidgetParent->transient.childWidgetCursor));
            else
                widget->position = korl_math_v2f32_add(parentAnchor, widget->parentOffset);
            ///@TODO: figure out why the minimize button on windows gets wonky AABB calculation when the window is resized to be small in the X dimension
        }
        /* now that we know where the widget will be placed, we can determine the AABB of _most_ widgets; 
            the widget's rendering logic is free to modify this value at any time in order to update the 
            widget's size metrics for future frames */
        if(widget->isContentHidden)
        {
            switch(widget->type)// only certain widgets have the ability to hide their content //
            {
            case KORL_GUI_WIDGET_TYPE_WINDOW:{
                korl_assert(widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR);// only windows w/ title bars can hide their content!
                widget->size.y = context->style.windowTitleBarPixelSizeY;
                break;}
            default:{
                korl_log(ERROR, "invalid widget type: %i", widget->type);
                break;}
            }
        }
        usedWidget->transient.aabb = korl_math_aabb2f32_fromPoints(widget->position.x                 , widget->position.y - widget->size.y
                                                                  ,widget->position.x + widget->size.x, widget->position.y);
        if(widgetParent)
            usedWidget->transient.aabb = korl_math_aabb2f32_intersect(usedWidget->transient.aabb, usedWidgetParent->transient.aabb);
        const Korl_Math_V2f32 aabbSize = korl_math_aabb2f32_size(usedWidget->transient.aabb);
        usedWidget->transient.aabbContent = korl_math_aabb2f32_fromPoints(widget->position.x, widget->position.y, widget->position.x, widget->position.y);// initialize the content AABB to be empty in the top-left corner; this will grow as each widget processes its own content & its child widgets recursively
        /* prepare to draw all the widget's contents by cutting out a scissor 
            region the size of the widget, preventing us from accidentally 
            render any pixels outside the widget; note that this AABB is the 
            intersection of this widget & all of its parents, so that we can't 
            accidentally draw outside of a parent widget */
        korl_time_probeStart(setup_camera);
        Korl_Math_V2f32 scissorPosition = {usedWidget->transient.aabb.min.x, surfaceSize.y - usedWidget->transient.aabb.max.y};
        Korl_Math_V2f32 scissorSize     = aabbSize;
        if(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW)
        {
            korl_shared_const f32 PANEL_BORDER_PIXELS = 1.f;
            scissorPosition.x -=   PANEL_BORDER_PIXELS;
            scissorPosition.y -=   PANEL_BORDER_PIXELS;
            scissorSize.x     += 2*PANEL_BORDER_PIXELS;
            scissorSize.y     += 2*PANEL_BORDER_PIXELS;
        }
        korl_gfx_cameraSetScissor(&cameraOrthographic, scissorPosition.x,scissorPosition.y, scissorSize.x,scissorSize.y);
        korl_gfx_useCamera(cameraOrthographic);
        korl_time_probeStop(setup_camera);
        switch(widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_WINDOW:{
            usedWidget->transient.isFirstFrame = widget->subType.window.isFirstFrame;
            if(!(widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE))// auto-resizing windows just consider _all_ frames to be the first frame
                widget->subType.window.isFirstFrame = false;
            const bool isActiveTopLevelWindow = context->isTopLevelWindowActive 
                                             && !widget->identifierHashParent 
                                             &&  widget->orderIndex == context->rootWidgetOrderIndexHighest;
            /* draw the window panel */
            Korl_Vulkan_Color4u8 windowColor   = context->style.colorWindow;
            Korl_Vulkan_Color4u8 titleBarColor = context->style.colorTitleBar;
            if(isActiveTopLevelWindow)
            {
                windowColor   = context->style.colorWindowActive;
                titleBarColor = context->style.colorTitleBarActive;
            }
            korl_time_probeStart(draw_window_panel);
            Korl_Gfx_Batch*const batchWindowPanel = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, aabbSize, ORIGIN_RATIO_UPPER_LEFT, windowColor);
            korl_gfx_batchSetPosition(batchWindowPanel, (f32[]){widget->position.x, widget->position.y, z}, 3);
            korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAGS_NONE);
            if(widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            {
                childWidgetCursorOffset = (Korl_Math_V2f32){0, -context->style.windowTitleBarPixelSizeY};
                korl_time_probeStart(title_bar);
                /* draw the window title bar */
                korl_gfx_batchSetPosition(batchWindowPanel, (f32[]){widget->position.x, widget->position.y, z + 0.1f}, 3);
                korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){aabbSize.x, context->style.windowTitleBarPixelSizeY});
                korl_gfx_batchRectangleSetColor(batchWindowPanel, titleBarColor);// conditionally highlight the title bar color
                korl_gfx_batchSetVertexColor(batchWindowPanel, 0, context->style.colorTitleBar);// keep the bottom two vertices the default title bar color
                korl_gfx_batchSetVertexColor(batchWindowPanel, 1, context->style.colorTitleBar);// keep the bottom two vertices the default title bar color
                korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAGS_NONE);
                /* draw the window title bar text */
                Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorHandleStack
                                                                                    ,string_getRawUtf16(context->style.fontWindowText)
                                                                                    ,string_getRawUtf16(widget->subType.window.titleBarText)
                                                                                    ,context->style.windowTextPixelSizeY
                                                                                    ,context->style.colorText
                                                                                    ,context->style.textOutlinePixelSize
                                                                                    ,context->style.colorTextOutline);
                const Korl_Math_V2f32 batchTextSize = korl_math_aabb2f32_size(korl_gfx_batchTextGetAabb(batchWindowTitleText));
                korl_gfx_batchSetPosition(batchWindowTitleText
                                         ,(f32[]){widget->position.x
                                                 ,widget->position.y - (context->style.windowTitleBarPixelSizeY - batchTextSize.y) / 2.f
                                                 ,z + 0.2f}, 3);
                korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAGS_NONE);
                Korl_Math_Aabb2f32 batchTextAabb = korl_gfx_batchTextGetAabb(batchWindowTitleText);// model-space, needs to be transformed to world-space
                const Korl_Math_V2f32 batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
                const f32 titleBarTextAabbSizeX = batchTextAabbSize.x + /*expand the content AABB size for the title bar buttons*/(usedWidget->widget->subType.window.titleBarButtonCount * context->style.windowTitleBarPixelSizeY);
                const f32 titleBarTextAabbSizeY = context->style.windowTitleBarPixelSizeY;
                batchTextAabb = korl_math_aabb2f32_fromPoints(widget->position.x, widget->position.y, widget->position.x + titleBarTextAabbSizeX, widget->position.y - titleBarTextAabbSizeY);
                usedWidget->transient.aabbContent = korl_math_aabb2f32_union(usedWidget->transient.aabbContent, batchTextAabb);
                /**/
                korl_time_probeStop(title_bar);
            }//window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR
            /* draw the window border */
            korl_time_probeStart(draw_window_border);
            Korl_Vulkan_Color4u8 colorBorder = context->style.colorWindowBorder;
            if(isActiveTopLevelWindow)
                if(   context->identifierHashWindowHovered == widget->identifierHash
                   && context->mouseHoverWindowEdgeFlags == KORL_GUI_EDGE_FLAGS_NONE)
                    colorBorder = context->style.colorWindowBorderResize;
                else
                    colorBorder = context->style.colorWindowBorderActive;
            else if(context->identifierHashWindowHovered == widget->identifierHash)
                colorBorder = context->style.colorWindowBorderHovered;
            /* before drawing the window border, we conditionally draw a cone 
                highlighting the side(s) that are being hovered to provide user 
                feedback that the window can be resized from the highlighted boundary */
            const Korl_Math_V2f32 windowMiddle = (Korl_Math_V2f32){widget->position.x + aabbSize.x/2
                                                                  ,widget->position.y - aabbSize.y/2};
            if(context->identifierHashWindowHovered == widget->identifierHash && context->mouseHoverWindowEdgeFlags)
            {
                const f32 longestAabbSide = KORL_MATH_MAX(aabbSize.x, aabbSize.y);
                const f32 shortestAabbSide = KORL_MATH_MIN(aabbSize.x, aabbSize.y);
                const f32 windowDiagonal = korl_math_v2f32_magnitude(&widget->size);
                f32 triLength = widget->size.x/2;
                f32 triWidth  = widget->size.y/2;
                if(   context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_UP
                   || context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_DOWN)
                {
                    triLength = widget->size.y/2;
                    triWidth  = widget->size.x/2;
                    if(   context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_LEFT
                       || context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_RIGHT)
                    {
                        triLength = windowDiagonal/2;
                        triWidth  = shortestAabbSide;
                    }
                }
                // we create two triangles for the edge indicator fan so that we can lerp from the highlight color to the border color
                Korl_Gfx_Batch*const batchEdgeHover = korl_gfx_createBatchTriangles(context->allocatorHandleStack, 2);
                KORL_C_CAST(Korl_Math_V3f32*, batchEdgeHover->_vertexPositions)[0].xy = KORL_MATH_V2F32_ZERO;
                KORL_C_CAST(Korl_Math_V3f32*, batchEdgeHover->_vertexPositions)[1].xy = (Korl_Math_V2f32){triLength + 1, 0.f};
                KORL_C_CAST(Korl_Math_V3f32*, batchEdgeHover->_vertexPositions)[2].xy = (Korl_Math_V2f32){triLength + 1, triWidth};
                KORL_C_CAST(Korl_Math_V3f32*, batchEdgeHover->_vertexPositions)[3].xy = KORL_MATH_V2F32_ZERO;
                KORL_C_CAST(Korl_Math_V3f32*, batchEdgeHover->_vertexPositions)[4].xy = (Korl_Math_V2f32){triLength + 1, -triWidth};
                KORL_C_CAST(Korl_Math_V3f32*, batchEdgeHover->_vertexPositions)[5].xy = (Korl_Math_V2f32){triLength + 1, 0.f};
                batchEdgeHover->_vertexColors[0] = context->style.colorWindowBorderResize;
                batchEdgeHover->_vertexColors[1] = context->style.colorWindowBorderResize;
                batchEdgeHover->_vertexColors[2] = colorBorder;
                batchEdgeHover->_vertexColors[3] = context->style.colorWindowBorderResize;
                batchEdgeHover->_vertexColors[4] = colorBorder;
                batchEdgeHover->_vertexColors[5] = context->style.colorWindowBorderResize;
                korl_gfx_batchSetPosition(batchEdgeHover, (f32[]){windowMiddle.x, windowMiddle.y, z}, 3);
                f32 edgeHoverRadians = 0.f;
                if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_UP)
                {
                    edgeHoverRadians = KORL_PI32/2;
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_LEFT)
                        edgeHoverRadians = korl_math_v2f32_radiansZ(korl_math_v2f32_subtract((Korl_Math_V2f32){usedWidget->transient.aabb.min.x, usedWidget->transient.aabb.max.y}, windowMiddle));
                    else if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_RIGHT)
                        edgeHoverRadians = korl_math_v2f32_radiansZ(korl_math_v2f32_subtract((Korl_Math_V2f32){usedWidget->transient.aabb.max.x, usedWidget->transient.aabb.max.y}, windowMiddle));
                }
                else if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_DOWN)
                {
                    edgeHoverRadians = -KORL_PI32/2;
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_LEFT)
                        edgeHoverRadians = korl_math_v2f32_radiansZ(korl_math_v2f32_subtract((Korl_Math_V2f32){usedWidget->transient.aabb.min.x, usedWidget->transient.aabb.min.y}, windowMiddle));
                    else if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_RIGHT)
                        edgeHoverRadians = korl_math_v2f32_radiansZ(korl_math_v2f32_subtract((Korl_Math_V2f32){usedWidget->transient.aabb.max.x, usedWidget->transient.aabb.min.y}, windowMiddle));
                }
                else if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_LEFT)
                    edgeHoverRadians = KORL_PI32;
                else if(context->mouseHoverWindowEdgeFlags & KORL_GUI_EDGE_FLAG_RIGHT)
                {
                    // this is the default mesh position; no need to do anything
                }
                korl_gfx_batchSetRotation(batchEdgeHover, KORL_MATH_V3F32_Z, edgeHoverRadians);
                korl_gfx_batch(batchEdgeHover, KORL_GFX_BATCH_FLAGS_NONE);
            }
            /* instead of just naively drawing a bunch of lines around the window, which I have found to result in 
                glitchy rasterization due to rounding errors (there isn't an easy way to place lines at an exact 
                pixel outside of a rectangle), we simply use the depth buffer to draw a giant rectangle to fill the 
                scissor rectangle placed behind (greater -Z magnitude) everything that was just drawn */
            Korl_Gfx_Batch*const batchBorder = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, korl_math_v2f32_multiplyScalar(aabbSize, 2), (Korl_Math_V2f32){0.5f, 0.5f}, colorBorder);
            korl_gfx_batchSetPosition(batchBorder, (f32[]){windowMiddle.x, windowMiddle.y, z}, 3);
            korl_gfx_batch(batchBorder, KORL_GFX_BATCH_FLAGS_NONE);
            korl_time_probeStop(draw_window_border);
            korl_time_probeStop(draw_window_panel);
            /* clamp the window content size to some minimum */
            {
                Korl_Math_V2f32 contentSize = korl_math_aabb2f32_size(usedWidget->transient.aabbContent);
                KORL_MATH_ASSIGN_CLAMP_MIN(contentSize.x, (usedWidget->widget->subType.window.titleBarButtonCount + 1) * context->style.windowTitleBarPixelSizeY);
                KORL_MATH_ASSIGN_CLAMP_MIN(contentSize.y, context->style.windowTitleBarPixelSizeY);
                usedWidget->transient.aabbContent.max.x = usedWidget->transient.aabbContent.min.x + contentSize.x;
                usedWidget->transient.aabbContent.min.y = usedWidget->transient.aabbContent.max.y - contentSize.y;
            }
            break;}
        case KORL_GUI_WIDGET_TYPE_BUTTON:{
            Korl_Vulkan_Color4u8 colorButton = KORL_COLOR4U8_TRANSPARENT;
            switch(widget->subType.button.display)
            {
            case _KORL_GUI_WIDGET_BUTTON_DISPLAY_TEXT:{
                if(widget->isHovered)
                    if(widget->identifierHash == context->identifierHashWidgetMouseDown)
                        colorButton = context->style.colorButtonPressed;
                    else
                        colorButton = context->style.colorButtonActive;
                else
                    colorButton = context->style.colorButtonInactive;
                break;}
            case _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_CLOSE:{
                if(widget->isHovered)
                    if(widget->identifierHash == context->identifierHashWidgetMouseDown)
                        colorButton = context->style.colorButtonPressed;
                    else
                        colorButton = context->style.colorButtonWindowCloseActive;// special red color just for CLOSE button
                else
                    colorButton = context->style.colorTitleBar;
                break;}
            case _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_MINIMIZE:{
                if(widget->isHovered)
                    if(widget->identifierHash == context->identifierHashWidgetMouseDown)
                        colorButton = context->style.colorButtonPressed;
                    else
                        colorButton = context->style.colorTitleBarActive;
                else
                    colorButton = context->style.colorTitleBar;
                break;}
            }
            Korl_Gfx_Batch* batchButtonPanel = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, widget->size, ORIGIN_RATIO_UPPER_LEFT, colorButton);
            korl_gfx_batchSetPosition2dV2f32(batchButtonPanel, widget->position);
            korl_gfx_batch(batchButtonPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            switch(widget->subType.button.display)
            {
            case _KORL_GUI_WIDGET_BUTTON_DISPLAY_TEXT:{
                Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, string_getRawUtf16(context->style.fontWindowText), widget->subType.button.displayText.data, context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
                Korl_Math_Aabb2f32 batchTextAabb = korl_gfx_batchTextGetAabb(batchText);
                const Korl_Math_V2f32 batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
                const f32 buttonAabbSizeX = batchTextAabbSize.x + context->style.widgetButtonLabelMargin * 2.f;
                const f32 buttonAabbSizeY = batchTextAabbSize.y + context->style.widgetButtonLabelMargin * 2.f;
                batchTextAabb = korl_math_aabb2f32_fromPoints(widget->position.x, widget->position.y, widget->position.x + buttonAabbSizeX, widget->position.y - buttonAabbSizeY);
                usedWidget->transient.aabbContent = korl_math_aabb2f32_union(usedWidget->transient.aabbContent, batchTextAabb);
                //KORL-ISSUE-000-000-008: instead of using the AABB of this text batch, we should be using the font's metrics!  Probably??  Different text batches of the same font will yield different sizes here, which will cause widget sizes to vary...
                korl_gfx_batchSetPosition2d(batchText
                                           ,widget->position.x + context->style.widgetButtonLabelMargin
                                           ,widget->position.y - context->style.widgetButtonLabelMargin);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                break;}
            case _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_CLOSE:{
                const f32 smallestSize = KORL_MATH_MIN(aabbSize.x, aabbSize.y);
                Korl_Gfx_Batch*const batchWindowTitleCloseIconPiece = 
                    korl_gfx_createBatchRectangleColored(context->allocatorHandleStack
                                                        ,(Korl_Math_V2f32){0.1f*smallestSize
                                                                          ,     smallestSize}
                                                        ,(Korl_Math_V2f32){0.5f, 0.5f}
                                                        ,context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleCloseIconPiece
                                           ,widget->position.x + smallestSize/2.f
                                           ,widget->position.y - smallestSize/2.f);
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z,  KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z, -KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                /* our content AABB is just the widget's assigned size */
                usedWidget->transient.aabbContent.max.x += widget->size.x;
                usedWidget->transient.aabbContent.min.y -= widget->size.y;
                break;}
            case _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_MINIMIZE:{
                ///@TODO: render a different symbol if a window is minimized
                Korl_Gfx_Batch*const batchWindowTitleIconPiece = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack
                                                                                                     ,(Korl_Math_V2f32){     aabbSize.x
                                                                                                                       ,0.1f*aabbSize.y}
                                                                                                     ,(Korl_Math_V2f32){0.5f, 0.5f}
                                                                                                     ,context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleIconPiece
                                           ,widget->position.x + aabbSize.x/2.f
                                           ,widget->position.y - aabbSize.y/2.f);
                korl_gfx_batch(batchWindowTitleIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                /* our content AABB is just the widget's assigned size */
                usedWidget->transient.aabbContent.max.x += widget->size.x;
                usedWidget->transient.aabbContent.min.y -= widget->size.y;
                break;}
            default:{
                korl_log(ERROR, "invalid button display: %i", widget->subType.button.display);
                break;}
            }
            break;}
        case KORL_GUI_WIDGET_TYPE_TEXT:{
            korl_time_probeStart(widget_text);
            if(widget->subType.text.gfxText)
            {
                korl_assert(!widget->subType.text.displayText.data);
                widget->subType.text.gfxText->modelTranslate = (Korl_Math_V3f32){widget->position.x, widget->position.y, z};
                korl_gfx_text_draw(widget->subType.text.gfxText, usedWidget->transient.aabb);
                const Korl_Math_Aabb2f32 textModelAabb = korl_gfx_text_getModelAabb(widget->subType.text.gfxText);
                const Korl_Math_V2f32    textAabbSize  = korl_math_aabb2f32_size(textModelAabb);
                usedWidget->transient.aabbContent.max.x += textAabbSize.x;
                usedWidget->transient.aabbContent.min.y -= textAabbSize.y;
            }
            else
            {
                korl_assert(widget->subType.text.displayText.data);
                korl_assert(!widget->subType.text.gfxText);
                Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, string_getRawUtf16(context->style.fontWindowText), widget->subType.text.displayText.data, context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
                //KORL-ISSUE-000-000-008: instead of using the AABB of this text batch, we should be using the font's metrics!  Probably??  Different text batches of the same font will yield different sizes here, which will cause widget sizes to vary...
                korl_gfx_batchSetPosition(batchText, (f32[]){widget->position.x, widget->position.y, z}, 3);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAGS_NONE);
                const Korl_Math_Aabb2f32 batchTextAabb     = korl_gfx_batchTextGetAabb(batchText);
                const Korl_Math_V2f32    batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
                usedWidget->transient.aabbContent.max.x += batchTextAabbSize.x;
                usedWidget->transient.aabbContent.min.y -= batchTextAabbSize.y;
            }
            korl_time_probeStop(widget_text);
            break;}
        case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
            /* we can "scroll" in the scroll area by modifying the transient child widget cursor */
            childWidgetCursorOffset = widget->subType.scrollArea.contentOffset;
            /* to determine the content size of the scroll area, we want to attempt to "fill" the remaining available content area of our parent */
            korl_assert(usedWidgetParent);
            usedWidget->transient.aabbContent = korl_math_aabb2f32_fromPoints(widget->position.x, widget->position.y
                                                                             ,usedWidgetParent->widget->position.x + usedWidgetParent->widget->size.x
                                                                             ,usedWidgetParent->widget->position.y - usedWidgetParent->widget->size.y);
            usedWidget->widget->size = korl_math_aabb2f32_size(usedWidget->transient.aabbContent);
            #ifdef _KORL_GUI_DEBUG_DRAW_SCROLL_AREA// just some debug test code to see whether or not the scroll area widget is being resized properly, since this widget is actually just invisible
            {
                Korl_Gfx_Batch*const batch = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, widget->size, ORIGIN_RATIO_UPPER_LEFT, (Korl_Vulkan_Color4u8){255,0,0,128});
                if(widget->isHovered)
                    batch->_vertexColors[1] = (Korl_Vulkan_Color4u8){0,0,255,128};
                else
                    batch->_vertexColors[1] = (Korl_Vulkan_Color4u8){0,255,0,128};
                korl_gfx_batchSetPosition(batch, (f32[]){widget->position.x, widget->position.y, z}, 3);
                korl_gfx_batch(batch, KORL_GFX_BATCH_FLAGS_NONE);
            }
            #endif
            break;}
        case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
            /* define the content area bounds */
            usedWidget->transient.aabbContent.max.x += widget->size.x;
            usedWidget->transient.aabbContent.min.y -= widget->size.y;
            /* draw the slider */
            const Korl_Vulkan_Color4u8 colorSlider = context->identifierHashWidgetMouseDown == widget->identifierHash && widget->subType.scrollBar.mouseDownRegion == KORL_GUI_SCROLL_BAR_REGION_SLIDER 
                                                     ? context->style.colorScrollBarPressed
                                                     : widget->isHovered && widget->subType.scrollBar.mouseDownRegion == KORL_GUI_SCROLL_BAR_REGION_SLIDER 
                                                       ? context->style.colorScrollBarActive
                                                       : context->style.colorScrollBar;
            const Korl_Math_V2f32 sliderSize = widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y
                                               ? (Korl_Math_V2f32){widget->size.x, widget->size.y*widget->subType.scrollBar.visibleRegionRatio}
                                               : (Korl_Math_V2f32){widget->size.x*widget->subType.scrollBar.visibleRegionRatio, widget->size.y};
            const f32 sliderOffset = widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y
                                     ? (widget->size.y - sliderSize.y)*widget->subType.scrollBar.scrollPositionRatio
                                     : (widget->size.x - sliderSize.x)*widget->subType.scrollBar.scrollPositionRatio;
            Korl_Gfx_Batch* batchSlider = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, sliderSize, ORIGIN_RATIO_UPPER_LEFT, colorSlider);
            switch(widget->subType.scrollBar.axis)
            {
            case KORL_GUI_SCROLL_BAR_AXIS_X:{
                korl_gfx_batchSetPosition(batchSlider, (f32[]){widget->position.x + sliderOffset, widget->position.y, z + 0.5f}, 3);
                break;}
            case KORL_GUI_SCROLL_BAR_AXIS_Y:{
                korl_gfx_batchSetPosition(batchSlider, (f32[]){widget->position.x, widget->position.y - sliderOffset, z + 0.5f}, 3);
                break;}
            }
            korl_gfx_batch(batchSlider, KORL_GFX_BATCH_FLAGS_NONE);
            /* draw the background region */
            const Korl_Vulkan_Color4u8 colorBackground = context->style.colorScrollBar;
            Korl_Gfx_Batch* batch = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, widget->size, ORIGIN_RATIO_UPPER_LEFT, colorBackground);
            switch(widget->subType.scrollBar.axis)
            {
            case KORL_GUI_SCROLL_BAR_AXIS_X:{
                batch->_vertexColors[2].a = batch->_vertexColors[3].a = 0;
                break;}
            case KORL_GUI_SCROLL_BAR_AXIS_Y:{
                batch->_vertexColors[0].a = batch->_vertexColors[3].a = 0;
                break;}
            }
            korl_gfx_batchSetPosition(batch, (f32[]){widget->position.x, widget->position.y, z}, 3);
            korl_gfx_batch(batch, KORL_GFX_BATCH_FLAGS_NONE);
            break;}
        default:{
            korl_log(ERROR, "unhandled widget type: %i", widget->type);
            break;}
        }
        /* union this widget's transient content AABB recursively with all its parents, 
            so that our parents will be able to know what their optimal new AABB should 
            be when they are popped off the stack (all children are processed) */
        if(useParentWidgetCursor)// only accumulate our content AABB with parents if we're using automatic placement; this allows things like titlebar buttons & scroll bars to be placed anywhere without affecting content AABBs
        {
            const _Korl_Gui_UsedWidget*const*const stbDaUsedWidgetStackEnd = stbDaUsedWidgetStack + arrlen(stbDaUsedWidgetStack);
            for(_Korl_Gui_UsedWidget** usedWidgetStack = stbDaUsedWidgetStack; usedWidgetStack < stbDaUsedWidgetStackEnd; usedWidgetStack++)
            {
                (*usedWidgetStack)->transient.aabbContent = korl_math_aabb2f32_union((*usedWidgetStack)->transient.aabbContent, usedWidget->transient.aabbContent);
                (*usedWidgetStack)->transient.aabbChildren = korl_math_aabb2f32_union((*usedWidgetStack)->transient.aabbChildren, usedWidget->transient.aabbContent);
            }
        }
        /* adjust the parent widget's cursor to the "next line" */
        if(widgetParent && useParentWidgetCursor)
        {
            const Korl_Math_V2f32 contentSize = korl_math_aabb2f32_size(usedWidget->transient.aabbContent);
            KORL_MATH_ASSIGN_CLAMP_MIN(usedWidgetParent->transient.currentWidgetRowHeight, contentSize.y);
            if(widget->realignY)
                /* do nothing to the widgetCursor Y position, but advance the X position */
                usedWidgetParent->transient.childWidgetCursor.x += contentSize.x;
            else
            {
                usedWidgetParent->transient.childWidgetCursor.x    = 0;// restore the X cursor position
                usedWidgetParent->transient.childWidgetCursor.y   -= usedWidgetParent->transient.currentWidgetRowHeight;// advance to the next Y position below this widget
                usedWidgetParent->transient.currentWidgetRowHeight = 0;// we're starting a new horizontal row of widgets; reset the AABB.y size accumulator
            }
        }
        /* push this widget onto the parent stack */
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaUsedWidgetStack, usedWidget);
        arrlast(stbDaUsedWidgetStack)->transient.childWidgetCursorOrigin = childWidgetCursorOffset;
        /* reset these values for the next frame */
        widget->usedThisFrame       = false;// this can actually be done at any time during this loop, but might as well put it here ¯\_(ツ)_/¯
        widget->transientChildCount = 0;
        /* normalize root widget orderIndex values; _must_ be done _after_ processing widget logic! */
        if(!widget->identifierHashParent)
        {
            widget->orderIndex = rootWidgetCount++;
            korl_assert(rootWidgetCount);// check integer overflow
        }
    }
    context->rootWidgetOrderIndexHighest = rootWidgetCount ? rootWidgetCount - 1 : 0;
    /* drain the usedWidgetStack, so that all widgets will have a chance to 
        perform logic when we know that _all_ their children have finished being 
        processed */
    while(arrlenu(stbDaUsedWidgetStack))
        _korl_gui_frameEnd_onUsedWidgetChildrenProcessed(arrpop(stbDaUsedWidgetStack));
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
        for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
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
    _Korl_Gui_Context*const context = &_korl_gui_context;
    context->loopIndex = loopIndex;
}
korl_internal KORL_FUNCTION_korl_gui_realignY(korl_gui_realignY)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    if(   context->currentWidgetIndex < 0
       || context->currentWidgetIndex >= korl_checkCast_u$_to_i$(arrlenu(context->stbDaWidgets)))
        return;// silently do nothing if user has not created a widget yet for the current window
    context->stbDaWidgets[context->currentWidgetIndex].realignY = true;
}
korl_internal KORL_FUNCTION_korl_gui_widgetTextFormat(korl_gui_widgetTextFormat)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(textFormat), KORL_GUI_WIDGET_TYPE_TEXT, &newAllocation);
    va_list vaList;
    va_start(vaList, textFormat);
    const wchar_t*const stackDisplayText = korl_memory_stringFormatVaList(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
    widget->subType.text.displayText = KORL_RAW_CONST_UTF16(stackDisplayText);
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
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
    _Korl_Gui_Context*const context = &_korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(textFormat), KORL_GUI_WIDGET_TYPE_BUTTON, &newAllocation);
    if(textFormat == _KORL_GUI_WIDGET_BUTTON_WINDOW_CLOSE)
        widget->subType.button.display = _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_CLOSE;
    else if(textFormat == _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZE)
        widget->subType.button.display = _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_MINIMIZE;
    else
    {
        widget->subType.button.display = _KORL_GUI_WIDGET_BUTTON_DISPLAY_TEXT;
        /* store the formatted display text on the stack */
        va_list vaList;
        va_start(vaList, textFormat);
        const wchar_t*const stackDisplayText = korl_memory_stringFormatVaList(context->allocatorHandleStack, textFormat, vaList);
        va_end(vaList);
        widget->subType.button.displayText = KORL_RAW_CONST_UTF16(stackDisplayText);
    }
    const u8 resultActuationCount = widget->subType.button.actuationCount;
    widget->subType.button.actuationCount = 0;
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
    /**/
    return resultActuationCount;
}
korl_internal void korl_gui_widgetScrollAreaBegin(acu16 label)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(label.data), KORL_GUI_WIDGET_TYPE_SCROLL_AREA, &newAllocation);
    /* conditionally spawn SCROLL_BAR widgets based on whether or not the 
        SCROLL_AREA widget can contain all of its contents (based on cached data 
        from the previous frame stored in the Widget) */
    widget->subType.scrollArea.hasScrollBarX = 
    widget->subType.scrollArea.hasScrollBarY = false;
    // the presence of a scroll bar in any dimension depends on the size of scrollable region, calculated from the accumulated child widget AABB; 
    //  this region will change, based on the presence of a scroll bar in a perpendicular axis; this is to allow the user to scroll the visible contents
    //  such that they are not obscured by a scroll bar
    Korl_Math_V2f32 aabbChildrenSize = widget->subType.scrollArea.aabbChildrenSize;
    if(aabbChildrenSize.x > widget->size.x)
    {
        aabbChildrenSize.y += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarX = true;
    }
    if(aabbChildrenSize.y > widget->size.y)
    {
        aabbChildrenSize.x += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarY = true;
    }
    // we have to perform the check again, as a scroll bar in a perpendicular axis may now be required if we now require a scroll bar in any given axis; 
    //  of course, making sure that we don't account for a scroll bar in any given axis more than once (hence the `hasScrollBar*` flags)
    if(aabbChildrenSize.x > widget->size.x && !widget->subType.scrollArea.hasScrollBarX)
    {
        aabbChildrenSize.y += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarX = true;
    }
    if(aabbChildrenSize.y > widget->size.y && !widget->subType.scrollArea.hasScrollBarY)
    {
        aabbChildrenSize.x += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarY = true;
    }
    const Korl_Math_V2f32 clippedSize = korl_math_v2f32_subtract(aabbChildrenSize, widget->size);
    if(widget->subType.scrollArea.hasScrollBarX)
    {
        korl_assert(aabbChildrenSize.x > widget->size.x);
        _korl_gui_setNextWidgetSize((Korl_Math_V2f32){widget->size.x - (widget->subType.scrollArea.hasScrollBarY ? context->style.windowScrollBarPixelWidth/*prevent overlap w/ y-axis SCROLL_BAR*/ : 0), context->style.windowScrollBarPixelWidth});
        _korl_gui_setNextWidgetParentAnchor((Korl_Math_V2f32){0, 1});
        _korl_gui_setNextWidgetParentOffset((Korl_Math_V2f32){0, context->style.windowScrollBarPixelWidth});
        _korl_gui_setNextWidgetOrderIndex(KORL_C_CAST(u16, -2));
        f32 scrollPositionRatio = korl_math_abs(widget->subType.scrollArea.contentOffset.x / clippedSize.x);
        korl_gui_widgetScrollBar(KORL_RAW_CONST_UTF16(L"_KORL_GUI_SCROLL_AREA_BAR_X"), KORL_GUI_SCROLL_BAR_AXIS_X, widget->size.x / aabbChildrenSize.x, &scrollPositionRatio);
    }
    if(widget->subType.scrollArea.hasScrollBarY)
    {
        korl_assert(aabbChildrenSize.y > widget->size.y);
        _korl_gui_setNextWidgetSize((Korl_Math_V2f32){context->style.windowScrollBarPixelWidth, widget->size.y});
        _korl_gui_setNextWidgetParentAnchor((Korl_Math_V2f32){1, 0});
        _korl_gui_setNextWidgetParentOffset((Korl_Math_V2f32){-context->style.windowScrollBarPixelWidth, 0});
        _korl_gui_setNextWidgetOrderIndex(KORL_C_CAST(u16, -1));
        f32 scrollPositionRatio = korl_math_abs(widget->subType.scrollArea.contentOffset.y / clippedSize.y);
        korl_gui_widgetScrollBar(KORL_RAW_CONST_UTF16(L"_KORL_GUI_SCROLL_AREA_BAR_Y"), KORL_GUI_SCROLL_BAR_AXIS_Y, widget->size.y / aabbChildrenSize.y, &scrollPositionRatio);
    }
}
korl_internal void korl_gui_widgetScrollAreaEnd(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(arrlenu(context->stbDaWidgetParentStack) > 0);
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    const _Korl_Gui_Widget*const widget = context->stbDaWidgets + widgetIndex;
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_SCROLL_AREA);
}
korl_internal void korl_gui_widgetScrollBar(acu16 label, Korl_Gui_ScrollBar_Axis axis, f32 visibleRegionRatio, f32* io_scrollPositionRatio)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(label.data), KORL_GUI_WIDGET_TYPE_SCROLL_BAR, &newAllocation);
    /**/
    widget->subType.scrollBar.axis                = axis;
    widget->subType.scrollBar.visibleRegionRatio  = visibleRegionRatio;
    widget->subType.scrollBar.scrollPositionRatio = *io_scrollPositionRatio;
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
    /**/
}
korl_internal void korl_gui_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_gui_context, sizeof(_korl_gui_context));
}
korl_internal bool korl_gui_saveStateRead(HANDLE hFile)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    if(!ReadFile(hFile, &_korl_gui_context, sizeof(_korl_gui_context), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    return true;
}
