#include "korl-gui.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-gfx.h"
#include "korl-gui-internal-common.h"
#include "korl-time.h"
#include "korl-string.h"
#include "korl-stb-ds.h"
#include "korl-algorithm.h"
typedef struct _Korl_Gui_UsedWidget
{
    _Korl_Gui_Widget* widget;
    struct
    {
        bool visited;// set to true when we have visited this node; this struct is simply a wrapper around _Korl_Gui_Widget so we can sort all widgets from back=>front
        /* all values below here are invalid until the visited flag is set */
        struct _Korl_Gui_UsedWidget** stbDaChildren;// this should be nullified as soon as topological sort procedures are finished at the top of korl_gui_frameEnd
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
        u64 candidateLeafWidgetActive;
    } transient;// this data is only used/valid at the end of the frame, when we need to process the list of all UsedWidgets that have been accumulated during the frame
} _Korl_Gui_UsedWidget;
typedef struct _Korl_Gui_WidgetMap
{
    u64 key;  // the widget's id hash value
    u$  value;// the index of the _Korl_Gui_UsedWidget in the context's stbDaUsedWidgets member
} _Korl_Gui_WidgetMap;
#if KORL_DEBUG
    // #define _KORL_GUI_DEBUG_DRAW_COORDINATE_FRAMES
    // #define _KORL_GUI_DEBUG_DRAW_SCROLL_AREA
#endif
#if defined(_LOCAL_STRING_POOL_POINTER)
    #undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (_korl_gui_context->stringPool)
korl_shared_const wchar_t _KORL_GUI_ORPHAN_WIDGET_WINDOW_TITLE_BAR_TEXT[] = L"DEBUG";
korl_shared_const u64     _KORL_GUI_ORPHAN_WIDGET_WINDOW_ID_HASH          = KORL_U64_MAX;
korl_shared_const wchar_t _KORL_GUI_WIDGET_BUTTON_WINDOW_CLOSE[]          = L"X"; // special internal button string to allow button widget to draw special graphics
korl_shared_const wchar_t _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZE[]       = L"-"; // special internal button string to allow button widget to draw special graphics
korl_shared_const wchar_t _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZED[]      = L"!-";// special internal button string to allow button widget to draw special graphics
korl_internal KORL_ALGORITHM_COMPARE(_korl_gui_compareUsedWidget_ascendOrderIndex)
{
    const _Korl_Gui_UsedWidget*const x = *KORL_C_CAST(const _Korl_Gui_UsedWidget**, a);
    const _Korl_Gui_UsedWidget*const y = *KORL_C_CAST(const _Korl_Gui_UsedWidget**, b);
    return x->widget->orderIndex < y->widget->orderIndex ? -1 
           : x->widget->orderIndex > y->widget->orderIndex ? 1 
             : 0;
}
typedef struct _Korl_Gui_CodepointTestData_Log
{
    u8 trailingMetaTagCodepoints;
    u8 metaTagComponent;// 0=>log level, 1=>time stamp, 2=>line, 3=>file, 4=>function
    u8 metaTagComponentCodepointIndex;
    const u8* pCodeUnitMetaTagStart;
} _Korl_Gui_CodepointTestData_Log;
korl_internal KORL_GFX_TEXT_CODEPOINT_TEST(_korl_gui_codepointTest_log)
{
    // log meta data example:
    //╟INFO   ┆11:48'00"525┆   58┆korl-vulkan.c┆_korl_vulkan_debugUtilsMessengerCallback╢ 
    _Korl_Gui_CodepointTestData_Log*const data = KORL_C_CAST(_Korl_Gui_CodepointTestData_Log*, userData);
    if(data->pCodeUnitMetaTagStart)
    {
        bool endOfMetaTagComponent = false;
        if(codepoint == L'╢')
        {
            data->trailingMetaTagCodepoints = 2;
            data->pCodeUnitMetaTagStart     = NULL;
            endOfMetaTagComponent = true;
        }
        else if(codepoint == L'┆')
            endOfMetaTagComponent = true;
        if(endOfMetaTagComponent)
        {
            switch(data->metaTagComponent)
            {
            case 0:{// log level
                korl_assert(o_currentLineColor);
                switch(*data->pCodeUnitMetaTagStart)
                {
                case L'A':{// ASSERT
                    o_currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){1, 0, 0};// red
                    break;}
                case L'E':{// ERROR
                    o_currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){1, 0, 0};// red
                    break;}
                case L'W':{// WARNING
                    o_currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){1, 1, 0};// yellow
                    break;}
                case L'I':{// INFO
                    // do nothing; the line color defaults to white!
                    break;}
                case L'V':{// VERBOSE
                    o_currentLineColor->xyz = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){0, 1, 1};// cyan
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
    else if(codepoint == L'╟')
    {
        data->pCodeUnitMetaTagStart = currentCodeUnit + bytesPerCodeUnit*codepointCodeUnits;
        data->metaTagComponent      = 0;
    }
    if(data->trailingMetaTagCodepoints)
        data->trailingMetaTagCodepoints--;
    if(data->pCodeUnitMetaTagStart || data->trailingMetaTagCodepoints)
        return false;
    return true;
}
korl_internal void _korl_gui_frameEnd_recursiveAddToParent(_Korl_Gui_UsedWidget* usedWidget, _Korl_Gui_WidgetMap** stbHmWidgetMap)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    if(usedWidget->dagMetaData.visited)
        return;
    usedWidget->dagMetaData.visited = true;
    if(usedWidget->widget->identifierHashParent)
    {
        const i$ hmIndexParent = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), *stbHmWidgetMap, usedWidget->widget->identifierHashParent);
        korl_assert(hmIndexParent >= 0);
        _Korl_Gui_UsedWidget*const usedWidgetParent = context->stbDaUsedWidgets + hmIndexParent;
        const i$ w = usedWidget - context->stbDaUsedWidgets;
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), usedWidgetParent->dagMetaData.stbDaChildren, &(context->stbDaUsedWidgets[w]));
        _korl_gui_frameEnd_recursiveAddToParent(usedWidgetParent, stbHmWidgetMap);
    }
}
korl_internal void _korl_gui_frameEnd_recursiveAppend(_Korl_Gui_UsedWidget** io_stbDaResult, _Korl_Gui_UsedWidget* usedWidget)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), *io_stbDaResult, *usedWidget);
    korl_algorithm_sort_quick(usedWidget->dagMetaData.stbDaChildren, arrlenu(usedWidget->dagMetaData.stbDaChildren), sizeof(*usedWidget->dagMetaData.stbDaChildren), _korl_gui_compareUsedWidget_ascendOrderIndex);
    const _Korl_Gui_UsedWidget*const*const stbDaChildrenEnd = usedWidget->dagMetaData.stbDaChildren + arrlen(usedWidget->dagMetaData.stbDaChildren);
    for(_Korl_Gui_UsedWidget** child = usedWidget->dagMetaData.stbDaChildren; child < stbDaChildrenEnd; child++)
        _korl_gui_frameEnd_recursiveAppend(io_stbDaResult, *child);
}
korl_internal void _korl_gui_resetTransientNextWidgetModifiers(void)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    context->transientNextWidgetModifiers.size         = korl_math_v2f32_nan();
    context->transientNextWidgetModifiers.anchor       = korl_math_v2f32_nan();
    context->transientNextWidgetModifiers.parentAnchor = korl_math_v2f32_nan();
    context->transientNextWidgetModifiers.parentOffset = korl_math_v2f32_nan();
    context->transientNextWidgetModifiers.orderIndex   = -1;
}
korl_internal _Korl_Gui_Widget* _korl_gui_getWidget(u64 identifierHash, u$ widgetType, bool* out_newAllocation)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    /* if there is no current active window, then we should just default to use 
        an internal "debug" window to allow the user to just create widgets at 
        any time without worrying about creating a window first */
    if(arrlen(context->stbDaWidgetParentStack) <= 0)
        korl_gui_windowBegin(_KORL_GUI_ORPHAN_WIDGET_WINDOW_TITLE_BAR_TEXT, NULL, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
    korl_assert(arrlen(context->stbDaWidgetParentStack) > 0);
    _Korl_Gui_Widget* widgetDirectParent = context->stbDaWidgets + arrlast(context->stbDaWidgetParentStack);
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
    widgetDirectParent = context->stbDaWidgets + arrlast(context->stbDaWidgetParentStack);// need to re-obtain parent widget, since this pointer is now invalid
    korl_memory_zero(widget, sizeof(*widget));
    widget->identifierHash       = identifierHashPrime;
    widget->identifierHashParent = widgetDirectParent->identifierHash;
    widget->type                 = widgetType;
    *out_newAllocation = true;
widgetIndexValid:
    context->currentUserWidgetIndex = -1;
    widget = &context->stbDaWidgets[widgetIndex];
    korl_assert(widget->type == widgetType);
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), context->stbDaWidgetParentStack, korl_checkCast_u$_to_i16(widgetIndex));
    widget->usedThisFrame = true;
    widget->realignY      = false;
    widget->isSizeCustom  = false;
    widget->orderIndex    = widgetDirectParent->transientChildCount++;
    /* disable this widget if any parent widgets in this hierarchy is unused */
    for(const i16* parentIndex = context->stbDaWidgetParentStack; parentIndex < stbDaWidgetParentStackEnd; parentIndex++)
        if(!context->stbDaWidgets[*parentIndex].usedThisFrame)
        {
            widget->usedThisFrame = false;
            break;
        }
    /* apply transient next widget modifiers */
    if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.size))
    {
        widget->size = context->transientNextWidgetModifiers.size;
        widget->isSizeCustom = true;
    }
        // do nothing otherwise, since the widget->size is transient & will potentially change due to widget logic at the end of each frame
    if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.anchor))
        widget->anchor = context->transientNextWidgetModifiers.anchor;
    else
        widget->anchor = KORL_MATH_V2F32_ZERO;
    if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.parentAnchor))
        widget->parentAnchor = context->transientNextWidgetModifiers.parentAnchor;
    else
        widget->parentAnchor = KORL_MATH_V2F32_ZERO;
    if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.parentOffset))
        widget->parentOffset = context->transientNextWidgetModifiers.parentOffset;
    else
        widget->parentOffset = korl_math_v2f32_nan();
    if(context->transientNextWidgetModifiers.orderIndex >= 0)
        widget->orderIndex = korl_checkCast_i$_to_u16(context->transientNextWidgetModifiers.orderIndex);
    return widget;
}
korl_internal void _korl_gui_widget_destroy(_Korl_Gui_Widget*const widget)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
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
    case KORL_GUI_WIDGET_TYPE_INPUT_TEXT:{
        // the input string buffer memory is stored externally (by the user)
        break;}
    default:{
        korl_log(ERROR, "invalid widget type: %i", widget->type);
        break;}
    }
}
korl_internal void _korl_gui_widget_collectDefragmentPointers(_Korl_Gui_Widget*const context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent)
{
    _Korl_Gui_Context*const guiContext = _korl_gui_context;
    switch(context->type)
    {
    case KORL_GUI_WIDGET_TYPE_WINDOW:{
        break;}
    case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
        break;}
    case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
        break;}
    case KORL_GUI_WIDGET_TYPE_TEXT:{
        if(context->subType.text.gfxText)
        {
            KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context->subType.text.gfxText, parent);
            korl_gfx_text_collectDefragmentPointers(context->subType.text.gfxText, stbDaMemoryContext, pStbDaDefragmentPointers, context->subType.text.gfxText);
        }
        break;}
    case KORL_GUI_WIDGET_TYPE_BUTTON:{
        break;}
    case KORL_GUI_WIDGET_TYPE_INPUT_TEXT:{
        break;}
    default:{
        korl_log(ERROR, "invalid widget type: %i", context->type);
        break;}
    }
}
/** Prepare a new GUI batch for the current application frame.  The user _must_ 
 * call \c korl_gui_frameEnd after each call to \c _korl_gui_frameBegin . */
korl_internal void _korl_gui_frameBegin(void)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    korl_memory_allocator_empty(context->allocatorHandleStack);
    context->stbDaWidgetParentStack = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), context->stbDaWidgetParentStack, 16);
    _korl_gui_resetTransientNextWidgetModifiers();
    context->currentUserWidgetIndex = -1;
}
korl_internal void _korl_gui_setNextWidgetOrderIndex(u16 orderIndex)
{
    _korl_gui_context->transientNextWidgetModifiers.orderIndex = orderIndex;
}
korl_internal void _korl_gui_widget_window_activate(_Korl_Gui_Widget* widget)
{
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
    _Korl_Gui_Context*const context = _korl_gui_context;
    //KORL-ISSUE-000-000-117: gui: don't we only want to do this logic if the activated window has changed?  not doing so is going to likely cause really annoying behavior such as the last window's leaf widget candidate always getting activated even when the window was already active :|
    {
        context->identifierHashLeafWidgetActive = 0;
        widget->subType.window.isActivatedThisFrame = true;
    }
    context->isTopLevelWindowActive = true;
}
korl_internal void _korl_gui_widget_scrollArea_scroll(_Korl_Gui_Widget* widget, Korl_Gui_ScrollBar_Axis axis, f32 delta)
{
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_SCROLL_AREA);
    switch(axis)
    {
    case KORL_GUI_SCROLL_BAR_AXIS_X:{
        widget->subType.scrollArea.contentOffset.x += delta;
        KORL_MATH_ASSIGN_CLAMP(widget->subType.scrollArea.contentOffset.x, -(widget->subType.scrollArea.aabbScrollableSize.x - widget->size.x), 0);
        break;}
    case KORL_GUI_SCROLL_BAR_AXIS_Y:{
        widget->subType.scrollArea.contentOffset.y += delta;
        const f32 scrollMax = widget->subType.scrollArea.aabbScrollableSize.y - widget->size.y;
        KORL_MATH_ASSIGN_CLAMP(widget->subType.scrollArea.contentOffset.y, 0, scrollMax);
        if(!korl_math_isNearlyZero(delta))
            widget->subType.scrollArea.isScrolledToEndY = widget->subType.scrollArea.contentOffset.y >= scrollMax;
        break;}
    }
}
korl_internal Korl_Math_V2f32 _korl_gui_widget_scrollBar_sliderSize(_Korl_Gui_Widget* widget)
{
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_SCROLL_BAR);
    f32 sliderLength = 0;
    switch(widget->subType.scrollBar.axis)
    {
    case KORL_GUI_SCROLL_BAR_AXIS_X:{
        sliderLength = widget->size.x*widget->subType.scrollBar.visibleRegionRatio;
        break;}
    case KORL_GUI_SCROLL_BAR_AXIS_Y:{
        sliderLength = widget->size.y*widget->subType.scrollBar.visibleRegionRatio;
        break;}
    }
    KORL_MATH_ASSIGN_CLAMP_MIN(sliderLength, 1);// prevent the slider length from being less than 1
    switch(widget->subType.scrollBar.axis)
    {
    case KORL_GUI_SCROLL_BAR_AXIS_X:{
        return (Korl_Math_V2f32){sliderLength, widget->size.y};}
    case KORL_GUI_SCROLL_BAR_AXIS_Y:{
        return (Korl_Math_V2f32){widget->size.x, sliderLength};}
    }
    korl_assert(!"invalid control path");
    return KORL_MATH_V2F32_ZERO;// should never execute; this is only to satisfy compiler warnings
}
korl_internal Korl_Gui_ScrollBar_Region _korl_gui_widget_scrollBar_identifyMouseRegion(_Korl_Gui_Widget* widget, Korl_Math_V2f32 mousePosition, Korl_Math_V2f32* o_sliderPosition)
{
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_SCROLL_BAR);
    korl_assert(   widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_X 
                || widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y);
    /* we need to re-calculate the scroll bar slider placement */
    // includes some copy-pasta from the SCROLL_BAR's frameEnd logic
    const Korl_Math_V2f32 sliderSize = _korl_gui_widget_scrollBar_sliderSize(widget);
    const f32 sliderOffset = widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y
                             ? (widget->size.y - sliderSize.y)*widget->subType.scrollBar.scrollPositionRatio
                             : (widget->size.x - sliderSize.x)*widget->subType.scrollBar.scrollPositionRatio;
    const Korl_Math_V2f32 sliderPosition = widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y 
                                           ? (Korl_Math_V2f32){widget->position.x, widget->position.y - sliderOffset}
                                           : (Korl_Math_V2f32){widget->position.x + sliderOffset, widget->position.y};
    const Korl_Math_Aabb2f32 sliderAabb = korl_math_aabb2f32_fromPoints(sliderPosition.x, sliderPosition.y, sliderPosition.x + sliderSize.x, sliderPosition.y - sliderSize.y);
    if(o_sliderPosition)
        *o_sliderPosition = sliderPosition;
    /* calculate which region of the scroll bar the mouse clicked on (slider, pre-slider, or post-slider) */
    if(korl_math_aabb2f32_containsV2f32(sliderAabb, mousePosition))
        return KORL_GUI_SCROLL_BAR_REGION_SLIDER;
    else
        switch(widget->subType.scrollBar.axis)
        {
        case KORL_GUI_SCROLL_BAR_AXIS_X:{
            if(mousePosition.x < sliderAabb.min.x)
                return KORL_GUI_SCROLL_BAR_REGION_PRE_SLIDER;
            else
                return KORL_GUI_SCROLL_BAR_REGION_POST_SLIDER;
            break;}
        case KORL_GUI_SCROLL_BAR_AXIS_Y:{
            if(mousePosition.y > sliderAabb.max.y)
                return KORL_GUI_SCROLL_BAR_REGION_PRE_SLIDER;
            else
                return KORL_GUI_SCROLL_BAR_REGION_POST_SLIDER;
            break;}
        }
    korl_assert(!"impossible control path");
    return KORL_GUI_SCROLL_BAR_REGION_SLIDER;// this should never execute; this is just to satisfy the compiler
}
korl_internal void _korl_gui_widget_scrollBar_onMouseDrag(_Korl_Gui_Widget* widget, Korl_Math_V2f32 mousePosition)
{
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_SCROLL_BAR);
    korl_assert(   widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_X 
                || widget->subType.scrollBar.axis == KORL_GUI_SCROLL_BAR_AXIS_Y);
    /* calculate & store the drag distance (depending on the SCROLL_BAR's axis) 
        which will be later returned to the user who calls korl_gui_widgetScrollBar */
    Korl_Math_V2f32 mouseDrag = KORL_MATH_V2F32_ZERO;
    switch(widget->subType.scrollBar.dragMode)
    {
    case _KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_SLIDER:{
        const Korl_Math_V2f32 newTransientSliderPosition = korl_math_v2f32_subtract(mousePosition, widget->subType.scrollBar.mouseDownSliderOffset);
        KORL_ZERO_STACK(Korl_Math_V2f32, sliderPosition);
        _korl_gui_widget_scrollBar_identifyMouseRegion(widget, mousePosition, &sliderPosition);
        mouseDrag = korl_math_v2f32_subtract(newTransientSliderPosition, sliderPosition);
        break;}
    case _KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_CONTROL:{
        mouseDrag = korl_math_v2f32_subtract(mousePosition, widget->subType.scrollBar.mouseDownSliderOffset);
        widget->subType.scrollBar.mouseDownSliderOffset = mousePosition;// set the new mouse offset origin for the next frame
        break;}
    }
    switch(widget->subType.scrollBar.axis)
    {
    case KORL_GUI_SCROLL_BAR_AXIS_X:{
        widget->subType.scrollBar.draggedMagnitude = mouseDrag.x;
        break;}
    case KORL_GUI_SCROLL_BAR_AXIS_Y:{
        widget->subType.scrollBar.draggedMagnitude = mouseDrag.y;
        break;}
    }
}
korl_internal void korl_gui_initialize(void)
{
    /* create the heap allocator, & allocate the context inside of it */
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(1);
    const Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-gui" , KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_gui_context = korl_allocate(allocator, sizeof(*_korl_gui_context));
    /* now we can initialize the context */
    _Korl_Gui_Context*const context = _korl_gui_context;
    context->allocatorHandleHeap                  = allocator;
    context->allocatorHandleStack                 = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR , L"korl-gui-stack", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    context->stringPool                           = korl_allocate(context->allocatorHandleHeap, sizeof(*context->stringPool));
    *context->stringPool                          = korl_stringPool_create(context->allocatorHandleHeap);
    context->style.colorWindow                    = (Korl_Vulkan_Color4u8){ 16,  16,  16, 200};
    context->style.colorWindowActive              = (Korl_Vulkan_Color4u8){ 24,  24,  24, 230};
    context->style.colorWindowBorder              = (Korl_Vulkan_Color4u8){  0,   0,   0, 230};
    context->style.colorWindowBorderHovered       = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    context->style.colorWindowBorderResize        = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    context->style.colorWindowBorderActive        = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    context->style.colorTitleBar                  = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    context->style.colorTitleBarActive            = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    context->style.colorButtonInactive            = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    context->style.colorButtonActive              = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    context->style.colorButtonPressed             = (Korl_Vulkan_Color4u8){  0,   8,   0, 255};
    context->style.colorButtonWindowTitleBarIcons = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    context->style.colorButtonWindowCloseActive   = (Korl_Vulkan_Color4u8){255,   0,   0, 255};
    context->style.colorScrollBar                 = (Korl_Vulkan_Color4u8){  8,   8,   8, 230};
    context->style.colorScrollBarActive           = (Korl_Vulkan_Color4u8){ 32,  32,  32, 250};
    context->style.colorScrollBarPressed          = (Korl_Vulkan_Color4u8){  0,   0,   0, 250};
    context->style.colorText                      = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    context->style.colorTextOutline               = (Korl_Vulkan_Color4u8){  0,   5,   0, 255};
    context->style.textOutlinePixelSize           = 0.f;
    context->style.fontWindowText                 = string_newEmptyUtf16(0);
    context->style.windowTextPixelSizeY           = 20.f;// _probably_ a good idea to make this <= `windowTitleBarPixelSizeY`
    context->style.windowTitleBarPixelSizeY       = 20.f;
    context->style.widgetSpacingY                 =  0.f;
    context->style.widgetButtonLabelMargin        =  2.f;
    context->style.windowScrollBarPixelWidth      = 15.f;
    context->pendingUnicodeSurrogate              = -1;
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaWidgets, 128);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleHeap), context->stbDaUsedWidgets, 128);
    /* kick-start the first GUI frame as soon as initialization of this module is complete */
    _korl_gui_frameBegin();
}
korl_internal void korl_gui_onMouseEvent(const _Korl_Gui_MouseEvent* mouseEvent)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    const _Korl_Gui_UsedWidget*const stbDaUsedWidgetsEnd = context->stbDaUsedWidgets + arrlen(context->stbDaUsedWidgets);
    switch(mouseEvent->type)
    {
    case _KORL_GUI_MOUSE_EVENT_TYPE_MOVE:{
        /* clear all widget hover flags */
        for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
            usedWidget->widget->isHovered = false;
        /**/
        _Korl_Gui_UsedWidget* draggedUsedWidget = NULL;
        if(context->identifierHashWidgetDragged)
        {
            for(_Korl_Gui_UsedWidget* uw = context->stbDaUsedWidgets; uw < stbDaUsedWidgetsEnd; uw++)
                if(uw->widget->identifierHash == context->identifierHashWidgetDragged)
                {
                    draggedUsedWidget = uw;
                    break;
                }
            if(!draggedUsedWidget)
                /* for whatever reason, we can't find the id hash of the dragged 
                    widget anymore, so let's just invalidate this id hash */
                context->identifierHashWidgetDragged = 0;
        }
        if(draggedUsedWidget)
            switch(draggedUsedWidget->widget->type)
            {
            case KORL_GUI_WIDGET_TYPE_WINDOW:{
                if(context->mouseHoverWindowEdgeFlags)
                {
                    korl_assert(draggedUsedWidget->widget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
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
                    draggedUsedWidget->widget->position.x = aabb.min.x;
                    draggedUsedWidget->widget->position.y = aabb.max.y;
                    draggedUsedWidget->widget->size.x     = aabb.max.x - aabb.min.x;
                    draggedUsedWidget->widget->size.y     = aabb.max.y - aabb.min.y;
                    draggedUsedWidget->transient.aabb = aabb;
                }
                else
                {
                    draggedUsedWidget->transient.aabb.min.x = mouseEvent->subType.move.position.x + context->mouseDownWidgetOffset.x;
                    draggedUsedWidget->transient.aabb.max.y = mouseEvent->subType.move.position.y + context->mouseDownWidgetOffset.y;
                    draggedUsedWidget->widget->position = (Korl_Math_V2f32){draggedUsedWidget->transient.aabb.min.x, draggedUsedWidget->transient.aabb.max.y};
                }
                break;}
            case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
                _korl_gui_widget_scrollBar_onMouseDrag(draggedUsedWidget->widget, mouseEvent->subType.move.position);
                break;}
            default:{
                break;}
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
                context->stbDaUsedWidgets && usedWidget >= context->stbDaUsedWidgets && continueRayCast; usedWidget--)
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
                        widget->subType.scrollBar.mouseDownRegion = _korl_gui_widget_scrollBar_identifyMouseRegion(widget, mouseEvent->subType.move.position, NULL);
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
            context->isTopLevelWindowActive         = false;
            context->identifierHashWidgetMouseDown  = 0;
            context->identifierHashLeafWidgetActive = 0;
            /* iterate over all widgets from front=>back */
            for(_Korl_Gui_UsedWidget* usedWidget = KORL_C_CAST(_Korl_Gui_UsedWidget*, stbDaUsedWidgetsEnd - 1); context->stbDaUsedWidgets && usedWidget >= context->stbDaUsedWidgets; usedWidget--)
            {
                _Korl_Gui_Widget*const widget = usedWidget->widget;
                Korl_Math_Aabb2f32 widgetAabb = usedWidget->transient.aabb;
                if(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW && widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE)
                    korl_math_aabb2f32_expand(&widgetAabb, _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS);
                if(!korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.button.position))
                    continue;
                bool widgetCanMouseDrag = false;
                bool widgetCanMouseDown = false;
                bool eventCaptured      = false;
                switch(widget->type)
                {
                case KORL_GUI_WIDGET_TYPE_WINDOW:{
                    korl_assert(!widget->identifierHashParent);// simplification; windows are always top-level (no child windows)
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
                        if(!subTreeEnd->widget->identifierHashParent)
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
                        korl_memory_copy(usedWidget + (stbDaUsedWidgetsEnd - subTreeEnd), stbDaTempSubTree, subTreeSize*sizeof(*usedWidget));
                    }
                    _korl_gui_widget_window_activate(widget);
                    eventCaptured      = true;
                    widgetCanMouseDown = true;
                    widgetCanMouseDrag = true;
                    break;}
                case KORL_GUI_WIDGET_TYPE_SCROLL_AREA:{
                    break;}
                case KORL_GUI_WIDGET_TYPE_SCROLL_BAR:{
                    KORL_ZERO_STACK(Korl_Math_V2f32, sliderPosition);
                    widget->subType.scrollBar.mouseDownRegion = _korl_gui_widget_scrollBar_identifyMouseRegion(widget, mouseEvent->subType.button.position, &sliderPosition);
                    widgetCanMouseDown = true;
                    if(mouseEvent->subType.button.keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL)
                    {
                        /* if we're holding down the [Ctrl] key, enter a special "absolute scroll" mode, 
                            so that instead of scrolling the contents based on the % the slider has moved 
                            within the range of the SCROLL_BAR, we can scroll the content offset based on 
                            literally the pixel difference of the mouse cursor (same scroll speed no matter 
                            what the content/view ratio is) */
                        widget->subType.scrollBar.dragMode              = _KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_CONTROL;
                        widget->subType.scrollBar.mouseDownSliderOffset = mouseEvent->subType.button.position;
                        widgetCanMouseDrag = true;
                    }
                    else if(widget->subType.scrollBar.mouseDownRegion == KORL_GUI_SCROLL_BAR_REGION_SLIDER)
                    {
                        widget->subType.scrollBar.dragMode              = _KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_SLIDER;
                        widget->subType.scrollBar.mouseDownSliderOffset = korl_math_v2f32_subtract(mouseEvent->subType.button.position, sliderPosition);
                        widgetCanMouseDrag = true;
                    }
                    else // we clicked a region outside of the slider; just set the drag magnitude such that we scroll directly to this position
                    {
                        /* begin slider drag mode, with a mouse slider offset at the center of the slider */
                        const Korl_Math_V2f32 sliderSize = _korl_gui_widget_scrollBar_sliderSize(widget);
                        widget->subType.scrollBar.dragMode              = _KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_SLIDER;
                        widget->subType.scrollBar.mouseDownSliderOffset = (Korl_Math_V2f32){sliderSize.x/2, -sliderSize.y/2};
                        widgetCanMouseDrag = true;
                        /* process the same code as a mouse drag event, so that we immediately scroll when the mouse button is pressed */
                        _korl_gui_widget_scrollBar_onMouseDrag(widget, mouseEvent->subType.button.position);
                    }
                    break;}
                case KORL_GUI_WIDGET_TYPE_TEXT:{
                    break;}
                case KORL_GUI_WIDGET_TYPE_BUTTON:{
                    widgetCanMouseDown    = true;
                    break;}
                case KORL_GUI_WIDGET_TYPE_INPUT_TEXT:{
                    break;}
                default:{
                    korl_log(ERROR, "invalid widget type: %i", widget->type);
                    break;}
                }
                if(!context->identifierHashLeafWidgetActive && widget->transientChildCount == 0 && widget->canBeActiveLeaf)
                    context->identifierHashLeafWidgetActive = widget->identifierHash;
                if(widgetCanMouseDown && !context->identifierHashWidgetMouseDown)// we mouse down on _only_ the first widget
                {
                    context->identifierHashWidgetMouseDown = widget->identifierHash;
                    context->mouseDownWidgetOffset         = korl_math_v2f32_subtract(widget->position, mouseEvent->subType.button.position);
                }
                if(widgetCanMouseDrag && !context->identifierHashWidgetDragged && context->identifierHashWidgetMouseDown == widget->identifierHash)
                    context->identifierHashWidgetDragged = widget->identifierHash;
                if(eventCaptured)
                    break;// stop processing widgets when the event is captured
            }
        }
        else// mouse button released; on-click logic
        {
            /* in order to an "on-click" event to actuate, we must have already 
                have a "mouse-down" registered on the widget, _and_ the mouse 
                must still be in the bounds of the widget */
            _Korl_Gui_UsedWidget* clickedUsedWidget = NULL;
            if(context->identifierHashWidgetMouseDown)
                for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
                    if(usedWidget->widget->identifierHash == context->identifierHashWidgetMouseDown)
                    {
                        clickedUsedWidget = usedWidget;
                        break;
                    }
            if(clickedUsedWidget)
            {
                const Korl_Math_Aabb2f32 widgetAabb = clickedUsedWidget->transient.aabb;
                if(korl_math_aabb2f32_containsV2f32(widgetAabb, mouseEvent->subType.button.position))
                {
                    switch(clickedUsedWidget->widget->type)
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
                        if(clickedUsedWidget->widget->subType.button.actuationCount < KORL_U8_MAX)// silently discard on-click events if we would integer overflow
                            clickedUsedWidget->widget->subType.button.actuationCount++;
                        break;}
                    default:{
                        korl_log(ERROR, "invalid widget type: %i", clickedUsedWidget->widget->type);
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
        for(_Korl_Gui_UsedWidget* usedWidget = KORL_C_CAST(_Korl_Gui_UsedWidget*, stbDaUsedWidgetsEnd - 1); context->stbDaUsedWidgets && usedWidget >= context->stbDaUsedWidgets; usedWidget--)
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
                        _korl_gui_widget_scrollArea_scroll(widget, KORL_GUI_SCROLL_BAR_AXIS_X, mouseEvent->subType.wheel.value * context->style.windowTextPixelSizeY);
                        break;}
                    case _KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_Y:{
                        _korl_gui_widget_scrollArea_scroll(widget, KORL_GUI_SCROLL_BAR_AXIS_Y, -mouseEvent->subType.wheel.value * context->style.windowTextPixelSizeY);
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
korl_internal void korl_gui_onKeyEvent(const _Korl_Gui_KeyEvent* keyEvent)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    /* locate the active leaf widget, if it exists */
    _Korl_Gui_UsedWidget* activeLeafWidget = NULL;
    const _Korl_Gui_UsedWidget*const stbDaUsedWidgetsEnd = context->stbDaUsedWidgets + arrlen(context->stbDaUsedWidgets);
    for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
        if(usedWidget->widget->identifierHash == context->identifierHashLeafWidgetActive)
        {
            activeLeafWidget = usedWidget;
            break;
        }
    if(activeLeafWidget)
    {
        switch(activeLeafWidget->widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_INPUT_TEXT:{
            if(!keyEvent->isDown)
                break;
            const u$ oldCursor          = activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex;
            const i$ oldCursorSelection = activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection;
            const u$ cursorBegin = KORL_MATH_MIN(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex
                                                ,activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex + activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection);
            const u$ cursorEnd   = KORL_MATH_MAX(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex
                                                ,activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex + activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection);
            i$ cursorDelta = 0;
            bool cursorDeltaSelect = true;
            switch(keyEvent->virtualKey)
            {
            case KORL_KEY_Y:{
                if(activeLeafWidget->widget->subType.inputText.inputMode)
                    break;
                context->ignoreNextCodepoint = true;
                /*fallthrough*/}
            case KORL_KEY_C:{
                if(   keyEvent->virtualKey == KORL_KEY_C // since we also have fallthrough key cases above us
                   && !(keyEvent->keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL))
                    break;
                context->ignoreNextCodepoint = true;
                if(cursorEnd > cursorBegin)
                {
                    Korl_StringPool_String stringSelection = korl_string_subString(activeLeafWidget->widget->subType.inputText.string, cursorBegin, cursorEnd - cursorBegin);
                    acu8 rawUtf8 = string_getRawAcu8(&stringSelection);
                    korl_clipboard_set(KORL_CLIPBOARD_DATA_FORMAT_UTF8, rawUtf8);
                    string_free(&stringSelection);
                }
                break;}
            case KORL_KEY_P:{
                if(activeLeafWidget->widget->subType.inputText.inputMode)
                    break;
                context->ignoreNextCodepoint = true;
                /*fallthrough*/}
            case KORL_KEY_V:{
                if(   keyEvent->virtualKey == KORL_KEY_V // since we also have fallthrough key cases above us
                   && !(keyEvent->keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL))
                    break;
                context->ignoreNextCodepoint = true;
                if(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection)
                {
                    korl_string_erase(&activeLeafWidget->widget->subType.inputText.string, cursorBegin, cursorEnd - cursorBegin);
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = 0;
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex     = cursorBegin;
                }
                acu8 clipboardUtf8 = korl_clipboard_get(KORL_CLIPBOARD_DATA_FORMAT_UTF8, context->allocatorHandleStack);
                if(clipboardUtf8.size)
                    clipboardUtf8.size--;// ignore the null-terminator
                /* submit this new codepoint to the input text's string at the cursor position(s) */
                const u32 stringGraphemes = korl_stringPool_getGraphemeSize(activeLeafWidget->widget->subType.inputText.string);
                korl_string_insertUtf8(&activeLeafWidget->widget->subType.inputText.string, activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex, clipboardUtf8);
                const u32 stringGraphemesAfter = korl_stringPool_getGraphemeSize(activeLeafWidget->widget->subType.inputText.string);
                /* adjust the cursor position(s) */
                activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex += stringGraphemesAfter - stringGraphemes;
                break;}
            case KORL_KEY_TENKEYLESS_6:{
                if(   activeLeafWidget->widget->subType.inputText.inputMode
                   || !(keyEvent->keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_SHIFT))
                    break;
                context->ignoreNextCodepoint = true;
                cursorDeltaSelect = false;
                /*fallthrough*/}
            case KORL_KEY_HOME:{
                const u32 stringGraphemes = korl_stringPool_getGraphemeSize(activeLeafWidget->widget->subType.inputText.string);
                cursorDelta = -korl_checkCast_u$_to_i$(stringGraphemes);
                break;}
            case KORL_KEY_A:{
                if(keyEvent->keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL)
                {
                    const u32 stringGraphemes = korl_stringPool_getGraphemeSize(activeLeafWidget->widget->subType.inputText.string);
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex     = 0;
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = stringGraphemes;
                    context->ignoreNextCodepoint = true;
                    break;
                }
                if(   activeLeafWidget->widget->subType.inputText.inputMode
                   || !(keyEvent->keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_SHIFT))
                    break;
                context->ignoreNextCodepoint = true;
                cursorDeltaSelect = false;
                activeLeafWidget->widget->subType.inputText.inputMode = true;
                /*fallthrough*/}
            case KORL_KEY_END:{
                const u32 stringGraphemes = korl_stringPool_getGraphemeSize(activeLeafWidget->widget->subType.inputText.string);
                cursorDelta = stringGraphemes;
                break;}
            case KORL_KEY_I:{
                if(   (keyEvent->keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL)
                   || !activeLeafWidget->widget->subType.inputText.inputMode)
                {
                    activeLeafWidget->widget->subType.inputText.inputMode = !activeLeafWidget->widget->subType.inputText.inputMode;
                    context->ignoreNextCodepoint = true;
                }
                break;}
            case KORL_KEY_J:{
                if(activeLeafWidget->widget->subType.inputText.inputMode)
                    break;
                context->ignoreNextCodepoint = true;
                /*fallthrough*/}
            case KORL_KEY_ARROW_LEFT:{
                cursorDelta = -1;
                break;}
            case KORL_KEY_L:{
                if(activeLeafWidget->widget->subType.inputText.inputMode)
                    break;
                context->ignoreNextCodepoint = true;
                /*fallthrough*/}
            case KORL_KEY_ARROW_RIGHT:{
                cursorDelta = 1;
                break;}
            case KORL_KEY_BACKSPACE:{
                if(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection)
                {
                    korl_string_erase(&activeLeafWidget->widget->subType.inputText.string, cursorBegin, cursorEnd - cursorBegin);
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = 0;
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex     = cursorBegin;
                }
                else if(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex > 0)
                {
                    korl_string_erase(&activeLeafWidget->widget->subType.inputText.string, activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex - 1, 1);
                    cursorDelta = -1;
                    cursorDeltaSelect = false;
                }
                context->ignoreNextCodepoint = true;
                break;}
            case KORL_KEY_X:{
                if(activeLeafWidget->widget->subType.inputText.inputMode)
                    break;
                context->ignoreNextCodepoint = true;
                /*fallthrough*/}
            case KORL_KEY_DELETE:{
                if(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection)
                {
                    korl_string_erase(&activeLeafWidget->widget->subType.inputText.string, cursorBegin, cursorEnd - cursorBegin);
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = 0;
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex     = cursorBegin;
                }
                else
                {
                    korl_string_erase(&activeLeafWidget->widget->subType.inputText.string, activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex, 1);
                }
                //NOTE: [Del] does _not_ seem to generate a codepoint...
                break;}
            case KORL_KEY_ENTER:{
                context->ignoreNextCodepoint = true;
                if(activeLeafWidget->widget->subType.inputText.enterKeyEventsReceived < KORL_U8_MAX)
                    activeLeafWidget->widget->subType.inputText.enterKeyEventsReceived++;
                break;}
            default: break;
            }
            if(cursorDelta)
            {
                if(cursorDeltaSelect && keyEvent->keyboardModifierFlags & _KORL_GUI_KEYBOARD_MODIFIER_FLAG_SHIFT)
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection += cursorDelta;
                else
                {
                    if(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection)
                        if(cursorDelta > 0)
                            activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex = cursorEnd;
                        else
                            activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex = cursorBegin;
                    else
                    {
                        KORL_MATH_ASSIGN_CLAMP_MIN(cursorDelta, -korl_checkCast_u$_to_i$(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex));
                        activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex += cursorDelta;
                    }
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = 0;
                }
            }
            /* keep the cursor components in the grapheme range of the string buffer */
            if(   oldCursor          != activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex
               || oldCursorSelection != activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection)
            {
                const u32 stringGraphemes = korl_stringPool_getGraphemeSize(activeLeafWidget->widget->subType.inputText.string);
                KORL_MATH_ASSIGN_CLAMP_MAX(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex, stringGraphemes);
                if(korl_checkCast_u$_to_i$(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex) + activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection < 0)
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = -korl_checkCast_u$_to_i$(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex);
                if(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex + activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection > stringGraphemes)
                    activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = stringGraphemes - activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex;
            }
            break;}
        default: break;
        }
    }
}
korl_internal void korl_gui_onCodepointEvent(const _Korl_Gui_CodepointEvent* codepointEvent)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    /* if this codepoint is a surrogate, we need to store the surrogate & wait until we receive its companion */
    u16 utf16Buffer[2];
    if(korl_string_isUtf16Surrogate(codepointEvent->utf16Unit))
    {
        if(context->pendingUnicodeSurrogate >= 0)
        {
            const u16 pendingUtf16Surrogate = korl_checkCast_i$_to_u16(context->pendingUnicodeSurrogate);
            korl_assert(korl_string_isUtf16Surrogate(pendingUtf16Surrogate));
            if(korl_string_isUtf16SurrogateHigh(codepointEvent->utf16Unit))
            {
                korl_assert(korl_string_isUtf16SurrogateLow(pendingUtf16Surrogate));
                utf16Buffer[0] = codepointEvent->utf16Unit;
                utf16Buffer[1] = pendingUtf16Surrogate;
            }
            else
            {
                korl_assert(korl_string_isUtf16SurrogateHigh(pendingUtf16Surrogate));
                utf16Buffer[0] = pendingUtf16Surrogate;
                utf16Buffer[1] = codepointEvent->utf16Unit;
            }
            /* now we can clear the pending surrogate, since we can now consume it together with the event's code unit */
            context->pendingUnicodeSurrogate = -1;
        }
        else
            return;/* there's nothing we can do; we just received a utf16 surrogate, but we can't do any further processing until we receive its pair! */
    }
    else/* otherwise, we expect to be able to just use the codepoint unit in isolation */
    {
        korl_assert(context->pendingUnicodeSurrogate < 0);// we expect there to _never_ be a codepoint surrogate in isolation!; each surrogate _must_ be accompanied by its companion
        utf16Buffer[0] = codepointEvent->utf16Unit;
    }
    if(context->ignoreNextCodepoint)
    {
        context->ignoreNextCodepoint = false;
        return;
    }
    // this will decode the UTF-16 codepoint, and do  all the checks to ensure it is valid data:
    Korl_String_CodepointIteratorUtf16 utf16It = korl_string_codepointIteratorUtf16_initialize(utf16Buffer, korl_arraySize(utf16Buffer));
    /* convert the UTF-16 input to UTF-8 */
    u8 utf8Buffer[4];
    const u8 utf8BufferSize = korl_string_codepoint_to_utf8(utf16It._codepoint, utf8Buffer);
    const acu8 utf8 = {.data = utf8Buffer, .size = utf8BufferSize};
    /* locate the active leaf widget, if it exists */
    _Korl_Gui_UsedWidget* activeLeafWidget = NULL;
    const _Korl_Gui_UsedWidget*const stbDaUsedWidgetsEnd = context->stbDaUsedWidgets + arrlen(context->stbDaUsedWidgets);
    for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < stbDaUsedWidgetsEnd; usedWidget++)
        if(usedWidget->widget->identifierHash == context->identifierHashLeafWidgetActive)
        {
            activeLeafWidget = usedWidget;
            break;
        }
    if(activeLeafWidget)
    {
        switch(activeLeafWidget->widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_INPUT_TEXT:{
            if(   !activeLeafWidget->widget->subType.inputText.isInputEnabled
               || !activeLeafWidget->widget->subType.inputText.inputMode)
                break;
            /* if the cursor values indicate a selection region, we need to delete this selection from the input string */
            if(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection != 0)
            {
                const u$ cursorBegin = KORL_MATH_MIN(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex
                                                    ,activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex + activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection);
                const u$ cursorEnd   = KORL_MATH_MAX(activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex
                                                    ,activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex + activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection);
                korl_string_erase(&activeLeafWidget->widget->subType.inputText.string, cursorBegin, cursorEnd - cursorBegin);
                activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex     = cursorBegin;
                activeLeafWidget->widget->subType.inputText.stringCursorGraphemeSelection = 0;
            }
            /* submit this new codepoint to the input text's string at the cursor position(s) */
            korl_string_insertUtf8(&activeLeafWidget->widget->subType.inputText.string, activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex, utf8);
            /* adjust the cursor position(s) */
            ++activeLeafWidget->widget->subType.inputText.stringCursorGraphemeIndex;
            break;}
        default: break;
        }
    }
}
korl_internal KORL_FUNCTION_korl_gui_setFontAsset(korl_gui_setFontAsset)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    string_free(&context->style.fontWindowText);
    if(fontAssetName)
        context->style.fontWindowText = string_newUtf16(fontAssetName);
}
korl_internal KORL_FUNCTION_korl_gui_windowBegin(korl_gui_windowBegin)
{
    //KORL-ISSUE-000-000-109: gui: there are a lot of similarities here to _getWidget; in an effort to generalize Window/Widget behavior, maybe I should just call _getWidget here???
    _Korl_Gui_Context*const context = _korl_gui_context;
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
    bool wasOpen = true;// assume that this window was previously closed, until we find it in the Widget pool
    const _Korl_Gui_Widget*const widgetsEnd = context->stbDaWidgets + arrlen(context->stbDaWidgets);
    for(_Korl_Gui_Widget* widget = context->stbDaWidgets; widget < widgetsEnd; widget++)
    {
        if(widget->identifierHash == identifierHash)
        {
            korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
            wasOpen = widget->subType.window.isOpen;
            if(!widget->subType.window.isOpen && !(out_isOpen && !*out_isOpen))
            {
                widget->subType.window.isOpen = true;
                widget->isContentHidden       = false;
            }
            currentWindowIndex = korl_checkCast_i$_to_i16(widget - context->stbDaWidgets);
            goto done_currentWindowIndexValid;
        }
    }
    wasOpen = false;// if we did not find it in the widget pool, it _must_ have not been open
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
    newWindow->size                        = KORL_MATH_V2F32_ZERO;
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
        newWindow->subType.window.styleFlags = styleFlags;
        newWindow->usedThisFrame             = out_isOpen ? *out_isOpen : newWindow->subType.window.isOpen;// IMPORTANT: we have to calculate this _before_ creating any other child widgets, because whether or not a new child widget will be used this frame is determined by all their parent windows' isOpen values; if the window isn't open, this entire widget sub-tree is unused for this frame
        /* apply transient next widget modifiers */
        if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.size))
            newWindow->size = context->transientNextWidgetModifiers.size;
            // do nothing otherwise, since the newWindow->size is transient & will potentially change due to widget logic at the end of each frame
        if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.anchor))
            newWindow->anchor = context->transientNextWidgetModifiers.anchor;
        else
            newWindow->anchor = KORL_MATH_V2F32_ZERO;
        if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.parentAnchor))
            newWindow->parentAnchor = context->transientNextWidgetModifiers.parentAnchor;
        else
            newWindow->parentAnchor = KORL_MATH_V2F32_ZERO;
        if(!korl_math_v2f32_hasNan(context->transientNextWidgetModifiers.parentOffset))
            newWindow->parentOffset = context->transientNextWidgetModifiers.parentOffset;
        else
            newWindow->parentOffset = korl_math_v2f32_nan();
        if(context->transientNextWidgetModifiers.orderIndex >= 0)
            newWindow->orderIndex = korl_checkCast_i$_to_u16(context->transientNextWidgetModifiers.orderIndex);
        _korl_gui_resetTransientNextWidgetModifiers();
        /* now that transient next widget modifiers have been applied, we can spawn child widgets */
        korl_shared_const Korl_Math_V2f32 TITLE_BAR_BUTTON_ANCHOR = {1, 0};// set title bar buttons relative to the window's upper-right corner
        Korl_Math_V2f32 titleBarButtonCursor = (Korl_Math_V2f32){-context->style.windowTitleBarPixelSizeY, 0.f};
        if(out_isOpen)
        {
            korl_gui_setNextWidgetSize((Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
            korl_gui_setNextWidgetParentAnchor(TITLE_BAR_BUTTON_ANCHOR);
            korl_gui_setNextWidgetParentOffset(titleBarButtonCursor);
            korl_math_v2f32_assignAdd(&titleBarButtonCursor, (Korl_Math_V2f32){-context->style.windowTitleBarPixelSizeY, 0.f});
            if(korl_gui_widgetButtonFormat(_KORL_GUI_WIDGET_BUTTON_WINDOW_CLOSE))
                newWindow->subType.window.isOpen = false;
            *out_isOpen = newWindow->subType.window.isOpen;
            newWindow->subType.window.titleBarButtonCount++;
        }
        else
            newWindow->subType.window.isOpen = true;
        if(styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            korl_gui_setNextWidgetSize((Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
            korl_gui_setNextWidgetParentAnchor(TITLE_BAR_BUTTON_ANCHOR);
            korl_gui_setNextWidgetParentOffset(titleBarButtonCursor);
            korl_math_v2f32_assignAdd(&titleBarButtonCursor, (Korl_Math_V2f32){-context->style.windowTitleBarPixelSizeY, 0.f});
            if(korl_gui_widgetButtonFormat(newWindow->isContentHidden ? _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZED : _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZE))
            {
                newWindow->isContentHidden = !newWindow->isContentHidden;
                if(newWindow->isContentHidden)
                    newWindow->hiddenContentPreviousSizeY = newWindow->size.y;
                else
                    newWindow->size.y = newWindow->hiddenContentPreviousSizeY;
            }
            newWindow->subType.window.titleBarButtonCount++;
        }
        /**/
        context->currentUserWidgetIndex = -1;
        /* add scroll area if this window allows it */
        if(styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE)
            korl_gui_widgetScrollAreaBegin(KORL_RAW_CONST_UTF16(L"KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE"), KORL_GUI_WIDGET_SCROLL_AREA_FLAGS_NONE);
        /* auto-activate the window when it is opened for the first time */
        if(!wasOpen && newWindow->subType.window.isOpen && (styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_DEFAULT_ACTIVE))
            _korl_gui_widget_window_activate(newWindow);
        /**/
        _korl_gui_resetTransientNextWidgetModifiers();
}
korl_internal KORL_FUNCTION_korl_gui_windowEnd(korl_gui_windowEnd)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    korl_assert(arrlen(context->stbDaWidgetParentStack) >= 0);
    _Korl_Gui_Widget*const rootWidget = context->stbDaWidgets + context->stbDaWidgetParentStack[0];
    korl_assert(rootWidget->type == KORL_GUI_WIDGET_TYPE_WINDOW);
    if(rootWidget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE)
        korl_gui_widgetScrollAreaEnd();
    korl_assert(arrlen(context->stbDaWidgetParentStack) == 1);
    arrpop(context->stbDaWidgetParentStack);
    _korl_gui_resetTransientNextWidgetModifiers();
}
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetSize(korl_gui_setNextWidgetSize)
{
    _korl_gui_context->transientNextWidgetModifiers.size = size;
}
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetAnchor(korl_gui_setNextWidgetAnchor)
{
    _korl_gui_context->transientNextWidgetModifiers.anchor = localAnchorRatioRelativeToTopLeft;
}
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetParentAnchor(korl_gui_setNextWidgetParentAnchor)
{
    _korl_gui_context->transientNextWidgetModifiers.parentAnchor = anchorRatioRelativeToParentTopLeft;
}
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetParentOffset(korl_gui_setNextWidgetParentOffset)
{
    _korl_gui_context->transientNextWidgetModifiers.parentOffset = positionRelativeToAnchor;
}
/** This function is called from within \c frameEnd as soon as we are in a 
 * state where we are absolutely sure that all of the children of \c usedWidget 
 * have been processed & drawn for the end of this frame. */
korl_internal void _korl_gui_frameEnd_onUsedWidgetChildrenProcessed(_Korl_Gui_UsedWidget* usedWidget)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    switch(usedWidget->widget->type)
    {
    case KORL_GUI_WIDGET_TYPE_WINDOW:{
        /* if a WINDOW widget just finished its first frame, auto-size it to fit all of its contents */
        if(usedWidget->transient.isFirstFrame || (usedWidget->widget->subType.window.styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE))
            usedWidget->transient.aabb = usedWidget->transient.aabbContent;
        usedWidget->widget->size = korl_math_aabb2f32_size(usedWidget->transient.aabb);
        /* if we were activated this frame & no active leaf widget is selected, 
            we should select the last transient LeafWidgetActive candidate this frame */
        if(usedWidget->widget->subType.window.isActivatedThisFrame && !context->identifierHashLeafWidgetActive)
            context->identifierHashLeafWidgetActive = usedWidget->transient.candidateLeafWidgetActive;
        usedWidget->widget->subType.window.isActivatedThisFrame = false;
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
    _Korl_Gui_Context*const context = _korl_gui_context;
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
            else
                if(widget->type == KORL_GUI_WIDGET_TYPE_WINDOW)
                    widget->subType.window.isOpen = false;// for WINDOW root widgets, we just set them to closed; which allows us to perform one-time logic when they are re-opened
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
        Another Example DAG:
        - {WINDOW[0], depth=0, order=0, childrenDirect=2, childrenTotal=3}
            - {SCROLL_AREA[0], depth=1, order=0, childrenDirect=1, childrenTotal=1}
                - {TEXT[0], depth=2, order=0, childrenDirect=0, childrenTotal=0}
            - {TEXT[1], depth=1, order=1, childrenDirect=0, childrenTotal=0}
        Topological Sort approach:
        - for each UsedWidget
            - recursively add itself to its parent's list of child UsedWidget*s
            - if the widget has no parent, add the UsedWidget* to a list of rootUsedWidgets
        - sort rootUsedWidgets by ascending orderIndex
        - for each rootUsedWidget
            - recursively add all child UsedWidget*s to a topologicalSortedUsedWidget list
        - copy `topologicalSortedUsedWidget` to `context->stbDaUsedWidgets` */
    // identify root UsedWidgets, & recursively find each UsedWidgets' direct children //
    const _Korl_Gui_UsedWidget*const usedWidgetsEnd = context->stbDaUsedWidgets + arrlen(context->stbDaUsedWidgets);
    _Korl_Gui_UsedWidget** stbDaRootWidgets = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaRootWidgets, arrlenu(context->stbDaUsedWidgets));
    for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
    {
        const i$ w = usedWidget - context->stbDaUsedWidgets;
        _korl_gui_frameEnd_recursiveAddToParent(usedWidget, &stbHmWidgetMap);
        if(!usedWidget->widget->identifierHashParent)
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaRootWidgets, &(context->stbDaUsedWidgets[w]));
    }
    // sort stbDaRootWidgets by ascending orderIndex //
    korl_algorithm_sort_quick(stbDaRootWidgets, arrlenu(stbDaRootWidgets), sizeof(*stbDaRootWidgets), _korl_gui_compareUsedWidget_ascendOrderIndex);
    // create an array to store the topological sort results, and recursively build this //
    _Korl_Gui_UsedWidget* stbDaTopologicalSortedUsedWidgets = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaTopologicalSortedUsedWidgets, arrlenu(context->stbDaUsedWidgets));
    const _Korl_Gui_UsedWidget*const*const stbDaRootWidgetsEnd = stbDaRootWidgets + arrlen(stbDaRootWidgets);
    for(_Korl_Gui_UsedWidget** rootWidget = stbDaRootWidgets; rootWidget < stbDaRootWidgetsEnd; rootWidget++)
        _korl_gui_frameEnd_recursiveAppend(&stbDaTopologicalSortedUsedWidgets, *rootWidget);
    korl_assert(arrlen(stbDaTopologicalSortedUsedWidgets) == arrlen(context->stbDaUsedWidgets));
    // finally, we can copy the sorted UsedWidget array to context->stbDaUsedWidgets //
    korl_memory_copy(context->stbDaUsedWidgets, stbDaTopologicalSortedUsedWidgets, arrlenu(stbDaTopologicalSortedUsedWidgets)*sizeof(*stbDaTopologicalSortedUsedWidgets));
    /* since the `context->stbDaUsedWidgets` has been modified, we should 
        invalidate all dangling pointers to the previous contents */
    mchmfree(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbHmWidgetMap);// the map from hash=>index is now invalid! (and we shouldn't need it anymore anyways, since we are now topologically sorted)
    for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
        usedWidget->dagMetaData.stbDaChildren = NULL;// all these are allocated from context->allocatorStack, so they should be freed automatically
    /* sanity-check the sorted list of used widgets to make sure that for each 
        widget sub tree, all child orderIndex values are unique */
    {
        typedef struct SanityCheckStack
        {
            i$ currentOrderIndex;
            _Korl_Gui_UsedWidget* usedWidget;
        } SanityCheckStack;
        SanityCheckStack* stbDaSanityCheckStack = NULL;
        for(_Korl_Gui_UsedWidget* usedWidget = context->stbDaUsedWidgets; usedWidget < usedWidgetsEnd; usedWidget++)
        {
            /* pop until this widget's parent is at the top, or the stack is empty */
            while(arrlenu(stbDaSanityCheckStack) && arrlast(stbDaSanityCheckStack).usedWidget->widget->identifierHash != usedWidget->widget->identifierHashParent)
                arrpop(stbDaSanityCheckStack);
            if(arrlenu(stbDaSanityCheckStack))
            {
                korl_assert(arrlast(stbDaSanityCheckStack).currentOrderIndex < usedWidget->widget->orderIndex);
                arrlast(stbDaSanityCheckStack).currentOrderIndex = usedWidget->widget->orderIndex;
            }
            const SanityCheckStack newStackPancake = {.currentOrderIndex = -1, .usedWidget = usedWidget};
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaSanityCheckStack, newStackPancake);
        }
        mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandleStack), stbDaSanityCheckStack);
    }
    /* prepare the view/projection graphics state */
    const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize();
    Korl_Gfx_Camera cameraOrthographic = korl_gfx_createCameraOrtho(korl_checkCast_i$_to_f32(arrlen(context->stbDaUsedWidgets) + 1/*+1 so the back widget will still be _above_ the rear clip plane*/)/*clipDepth; each used widget can have its own integral portion of the depth buffer, so if we have 2 widgets, the first can be placed at depth==-2, and the second can be placed at depth==-1; individual graphics components at each depth can safely be placed within this range without having to worry about interfering with other widget graphics, since we have already sorted the widgets from back=>front*/);
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
        /* determine where the widget's origin will be in world-space */
        const bool useParentWidgetCursor = korl_math_v2f32_hasNan(widget->parentOffset);
        if(usedWidgetParent)
        {
            const Korl_Math_V2f32 parentAnchor = korl_math_v2f32_add(usedWidgetParent->widget->position
                                                                    ,korl_math_v2f32_multiply(widget->parentAnchor
                                                                                             ,(Korl_Math_V2f32){ usedWidgetParent->widget->size.x
                                                                                                               ,-usedWidgetParent->widget->size.y/*inverted, since +Y is UP, & the parent's position is its top-left corner*/}));
            if(useParentWidgetCursor)
                widget->position = korl_math_v2f32_add(parentAnchor, korl_math_v2f32_add(usedWidgetParent->transient.childWidgetCursorOrigin, usedWidgetParent->transient.childWidgetCursor));
            else
                widget->position = korl_math_v2f32_add(parentAnchor, widget->parentOffset);
        }
        else/* this is a root-level widget; we need to use the rendering surface as our "parent" */
        {
            const Korl_Math_V2f32 parentAnchor = korl_math_v2f32_multiply(widget->parentAnchor, korl_math_v2f32_fromV2u32(surfaceSize));
            if(!useParentWidgetCursor)
                widget->position = korl_math_v2f32_add(parentAnchor, widget->parentOffset);
        }
        const Korl_Math_V2f32 localAnchor = korl_math_v2f32_multiply(widget->size, widget->anchor);
        korl_math_v2f32_assignAdd(&widget->position, (Korl_Math_V2f32){-localAnchor.x, localAnchor.y});
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
        if(usedWidgetParent)
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
                                                                                    ,string_getRawUtf16(&context->style.fontWindowText)
                                                                                    ,string_getRawUtf16(&widget->subType.window.titleBarText)
                                                                                    ,context->style.windowTextPixelSizeY
                                                                                    ,context->style.colorText
                                                                                    ,context->style.textOutlinePixelSize
                                                                                    ,context->style.colorTextOutline);
                Korl_Math_Aabb2f32    batchTextAabb         = korl_gfx_batchTextGetAabb(batchWindowTitleText);// model-space, needs to be transformed to world-space
                const Korl_Math_V2f32 batchTextAabbSize     = korl_math_aabb2f32_size(batchTextAabb);
                const f32             titleBarTextAabbSizeX =   batchTextAabbSize.x 
                                                              + /*expand the content AABB size for the title bar buttons*/(usedWidget->widget->subType.window.titleBarButtonCount * context->style.windowTitleBarPixelSizeY);
                const f32             titleBarTextAabbSizeY = context->style.windowTitleBarPixelSizeY;
                const f32             textPaddingY          = batchTextAabbSize.y < context->style.windowTitleBarPixelSizeY 
                                                              ? 0.5f * (context->style.windowTitleBarPixelSizeY - batchTextAabbSize.y)
                                                              : 0;
                korl_gfx_batchSetPosition(batchWindowTitleText, (f32[]){widget->position.x, widget->position.y - textPaddingY, z + 0.2f}, 3);
                korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAGS_NONE);
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
                Korl_Gfx_Font_Metrics fontMetrics = korl_gfx_font_getMetrics(string_getRawAcu16(&context->style.fontWindowText), context->style.windowTextPixelSizeY);
                const f32 textLineDeltaY = (fontMetrics.ascent - fontMetrics.decent) /*+ fontMetrics.lineGap // we don't need the lineGap, since we don't expect multiple text lines per button */;
                Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, string_getRawUtf16(&context->style.fontWindowText), widget->subType.button.displayText.data, context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
                Korl_Math_Aabb2f32 batchTextAabb = korl_gfx_batchTextGetAabb(batchText);
                const Korl_Math_V2f32 batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
                const f32 buttonAabbSizeX = batchTextAabbSize.x + (context->style.widgetButtonLabelMargin * 2.f);
                const f32 buttonAabbSizeY = textLineDeltaY      + (context->style.widgetButtonLabelMargin * 2.f);
                batchTextAabb = korl_math_aabb2f32_fromPoints(widget->position.x, widget->position.y, widget->position.x + buttonAabbSizeX, widget->position.y - buttonAabbSizeY);
                usedWidget->transient.aabbContent = korl_math_aabb2f32_union(usedWidget->transient.aabbContent, batchTextAabb);
                korl_gfx_batchSetPosition2d(batchText
                                           ,widget->position.x + context->style.widgetButtonLabelMargin
                                           ,widget->position.y - context->style.widgetButtonLabelMargin);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                break;}
            case _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_CLOSE:{
                const f32 smallestSize = KORL_MATH_MIN(widget->size.x, widget->size.y);
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
                const f32 smallestSize = KORL_MATH_MIN(widget->size.x, widget->size.y);
                Korl_Gfx_Batch*const batchWindowTitleIconPiece = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack
                                                                                                     ,(Korl_Math_V2f32){     smallestSize
                                                                                                                       ,0.1f*smallestSize}
                                                                                                     ,(Korl_Math_V2f32){0.5f, 0.5f}
                                                                                                     ,context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleIconPiece
                                           ,widget->position.x + smallestSize/2.f
                                           ,widget->position.y - smallestSize/2.f);
                if(widget->subType.button.specialButtonAlternateDisplay)
                    korl_gfx_batchSetRotation(batchWindowTitleIconPiece, KORL_MATH_V3F32_Z, KORL_PI32/2);
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
                Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, string_getRawUtf16(&context->style.fontWindowText), widget->subType.text.displayText.data, context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
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
            if(!widget->isSizeCustom)// we can't set the widget size here automatically if the user specifically set a size via `setNextWidgetSize`
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
            const Korl_Math_V2f32 sliderSize = _korl_gui_widget_scrollBar_sliderSize(widget);
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
        case KORL_GUI_WIDGET_TYPE_INPUT_TEXT:{
            /* prepare the graphics to draw the text buffer to obtain metrics, but defer drawing until later */
            const Korl_Gfx_Font_Metrics fontMetrics    = korl_gfx_font_getMetrics(string_getRawAcu16(&context->style.fontWindowText), context->style.windowTextPixelSizeY);
            const f32                   textLineDeltaY = (fontMetrics.ascent - fontMetrics.decent) /*+ fontMetrics.lineGap // we don't need the lineGap, since we don't expect multiple text lines */;
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, string_getRawUtf16(&context->style.fontWindowText), string_getRawUtf16(&widget->subType.inputText.string), context->style.windowTextPixelSizeY, context->style.colorText, context->style.textOutlinePixelSize, context->style.colorTextOutline);
            korl_gfx_batchSetPosition(batchText, (f32[]){widget->position.x, widget->position.y, z + 0.5f}, 3);
            const Korl_Math_Aabb2f32 batchTextAabb     = korl_gfx_batchTextGetAabb(batchText);
            const Korl_Math_V2f32    batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
            usedWidget->transient.aabbContent.min.y -= textLineDeltaY;
            /* draw a simple box around the input widget itself */
            korl_assert(usedWidgetParent);// for now, we want to just have the INPUT_TEXT widget fill the remaining X space of our parent
            usedWidget->transient.aabbContent.max.x = usedWidgetParent->widget->position.x + usedWidgetParent->widget->size.x;
            const Korl_Math_V2f32 contentAabbSize = korl_math_aabb2f32_size(usedWidget->transient.aabbContent);
            Korl_Gfx_Batch*const batchBox = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, contentAabbSize, ORIGIN_RATIO_UPPER_LEFT, KORL_COLOR4U8_BLACK);
            korl_gfx_batchSetPosition(batchBox, (f32[]){widget->position.x, widget->position.y, z}, 3);
            korl_gfx_batch(batchBox, KORL_GFX_BATCH_FLAGS_NONE);
            /* draw the selection region _behind_ the text, if our cursor defines a selection */
            const Korl_Math_V2f32      cursorSize          = {2, textLineDeltaY};
            const Korl_Math_V2f32      cursorOrigin        = {0, korl_math_f32_positive(fontMetrics.decent/*+ fontMetrics.lineGap // we don't need the lineGap, since we don't expect multiple text lines */ / textLineDeltaY)};
            const Korl_Vulkan_Color4u8 cursorColor         = {0, 255, 0, 100};
            const u$                   cursorBegin         = KORL_MATH_MIN(widget->subType.inputText.stringCursorGraphemeIndex
                                                                          ,widget->subType.inputText.stringCursorGraphemeIndex + widget->subType.inputText.stringCursorGraphemeSelection);
            const u$                   cursorEnd           = KORL_MATH_MAX(widget->subType.inputText.stringCursorGraphemeIndex
                                                                          ,widget->subType.inputText.stringCursorGraphemeIndex + widget->subType.inputText.stringCursorGraphemeSelection);
            const Korl_Math_V2f32      cursorPositionBegin = korl_gfx_font_textGraphemePosition(string_getRawAcu16(&context->style.fontWindowText)
                                                                                               ,context->style.windowTextPixelSizeY
                                                                                               ,korl_stringPool_getRawAcu8(&widget->subType.inputText.string)
                                                                                               ,cursorBegin);
            if(widget->identifierHash == context->identifierHashLeafWidgetActive && cursorBegin < cursorEnd)
            {
                const Korl_Math_V2f32 cursorPositionEnd = korl_gfx_font_textGraphemePosition(string_getRawAcu16(&context->style.fontWindowText)
                                                                                            ,context->style.windowTextPixelSizeY
                                                                                            ,korl_stringPool_getRawAcu8(&widget->subType.inputText.string)
                                                                                            ,cursorEnd);
                /* in this case, we need to draw a highlight region for the entire selected grapheme range */
                const Korl_Math_V2f32 lineSelectionSize = {cursorPositionEnd.x - cursorPositionBegin.x, textLineDeltaY};
                Korl_Gfx_Batch*const batchCursor = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, lineSelectionSize, cursorOrigin, cursorColor);
                korl_gfx_batchSetPosition(batchCursor, (f32[]){widget->position.x + cursorPositionBegin.x, widget->position.y - fontMetrics.ascent, z + 0.25f}, 3);
                korl_gfx_batch(batchCursor, KORL_GFX_BATCH_FLAGS_NONE);
            }
            if(batchText->_instanceCount)
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAGS_NONE);// draw the text buffer now, after any background elements
            /* draw a cursor _above_ the text, if the cursor defines a single grapheme index */
            if(widget->identifierHash == context->identifierHashLeafWidgetActive && cursorBegin >= cursorEnd)
            {
                if(widget->subType.inputText.inputMode)
                {
                    /* in this case, we need to draw a single vertical bar */
                    Korl_Gfx_Batch*const batchCursor = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, cursorSize, cursorOrigin, cursorColor);
                    korl_gfx_batchSetPosition(batchCursor, (f32[]){widget->position.x + cursorPositionBegin.x, widget->position.y - fontMetrics.ascent, z + 0.75f}, 3);
                    korl_gfx_batch(batchCursor, KORL_GFX_BATCH_FLAGS_NONE);
                }
                else
                {
                    const Korl_Math_V2f32 vimCursorSize = {10, 2};
                    /* draw an underscore instead I guess? */
                    Korl_Gfx_Batch*const batchCursor = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, vimCursorSize, cursorOrigin, cursorColor);
                    korl_gfx_batchSetPosition(batchCursor, (f32[]){widget->position.x + cursorPositionBegin.x, widget->position.y - fontMetrics.ascent, z + 0.75f}, 3);
                    korl_gfx_batch(batchCursor, KORL_GFX_BATCH_FLAGS_NONE);
                }
            }
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
        /* if we have have a root widget, and the current widget is capable of 
            being the context's LeafWidgetActive, update the root widget's 
            transient last LeafWidgetActive candidate */
        if(arrlenu(stbDaUsedWidgetStack) && widget->canBeActiveLeaf)
            stbDaUsedWidgetStack[0]->transient.candidateLeafWidgetActive = widget->identifierHash;
        /* adjust the parent widget's cursor to the "next line" */
        if(usedWidgetParent && useParentWidgetCursor)
        {
            const Korl_Math_V2f32 contentSize = widget->isSizeCustom 
                                                ? widget->size 
                                                : korl_math_aabb2f32_size(usedWidget->transient.aabbContent);
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
    _Korl_Gui_Context*const context = _korl_gui_context;
    context->loopIndex = loopIndex;
}
korl_internal KORL_FUNCTION_korl_gui_realignY(korl_gui_realignY)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    if(   context->currentUserWidgetIndex < 0
       || context->currentUserWidgetIndex >= korl_checkCast_u$_to_i$(arrlenu(context->stbDaWidgets)))
        return;// silently do nothing if user has not created a widget yet for the current window
    context->stbDaWidgets[context->currentUserWidgetIndex].realignY = true;
}
korl_internal KORL_FUNCTION_korl_gui_widgetTextFormat(korl_gui_widgetTextFormat)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(textFormat), KORL_GUI_WIDGET_TYPE_TEXT, &newAllocation);
    context->currentUserWidgetIndex = korl_checkCast_u$_to_i16(widget - context->stbDaWidgets);
    va_list vaList;
    va_start(vaList, textFormat);
    const wchar_t*const stackDisplayText = korl_string_formatVaListUtf16(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
    widget->subType.text.displayText = KORL_RAW_CONST_UTF16(stackDisplayText);
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
    /**/
    _korl_gui_resetTransientNextWidgetModifiers();
}
korl_internal KORL_FUNCTION_korl_gui_widgetText(korl_gui_widgetText)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    bool newAllocation = false;
    //KORL-ISSUE-000-000-128: gui: (minor) WARNING logged on memory state load due to frivolous resource destruction
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(identifier), KORL_GUI_WIDGET_TYPE_TEXT, &newAllocation);
    context->currentUserWidgetIndex = korl_checkCast_u$_to_i16(widget - context->stbDaWidgets);
    if(newAllocation)
    {
        korl_assert(korl_memory_isNull(&widget->subType.text, sizeof(widget->subType.text)));
        widget->subType.text.gfxText = korl_gfx_text_create(context->allocatorHandleHeap, string_getRawAcu16(&context->style.fontWindowText), context->style.windowTextPixelSizeY);
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
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
    /**/
    _korl_gui_resetTransientNextWidgetModifiers();
}
korl_internal KORL_FUNCTION_korl_gui_widgetButtonFormat(korl_gui_widgetButtonFormat)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(textFormat), KORL_GUI_WIDGET_TYPE_BUTTON, &newAllocation);
    context->currentUserWidgetIndex = korl_checkCast_u$_to_i16(widget - context->stbDaWidgets);
    widget->canBeActiveLeaf = true;
    if(textFormat == _KORL_GUI_WIDGET_BUTTON_WINDOW_CLOSE)
        widget->subType.button.display = _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_CLOSE;
    else if(textFormat == _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZE)
    {
        widget->subType.button.display                       = _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_MINIMIZE;
        widget->subType.button.specialButtonAlternateDisplay = false;
    }
    else if(textFormat == _KORL_GUI_WIDGET_BUTTON_WINDOW_MINIMIZED)
    {
        widget->subType.button.display                       = _KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_MINIMIZE;
        widget->subType.button.specialButtonAlternateDisplay = true;
    }
    else
    {
        widget->subType.button.display = _KORL_GUI_WIDGET_BUTTON_DISPLAY_TEXT;
        /* store the formatted display text on the stack */
        va_list vaList;
        va_start(vaList, textFormat);
        const wchar_t*const stackDisplayText = korl_string_formatVaListUtf16(context->allocatorHandleStack, textFormat, vaList);
        va_end(vaList);
        widget->subType.button.displayText = KORL_RAW_CONST_UTF16(stackDisplayText);
    }
    const u8 resultActuationCount = widget->subType.button.actuationCount;
    widget->subType.button.actuationCount = 0;
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
    /**/
    _korl_gui_resetTransientNextWidgetModifiers();
    return resultActuationCount;
}
korl_internal KORL_FUNCTION_korl_gui_widgetScrollAreaBegin(korl_gui_widgetScrollAreaBegin)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(label.data), KORL_GUI_WIDGET_TYPE_SCROLL_AREA, &newAllocation);
    if(newAllocation)
        widget->subType.scrollArea.isScrolledToEndY = true;
    /* conditionally spawn SCROLL_BAR widgets based on whether or not the 
        SCROLL_AREA widget can contain all of its contents (based on cached data 
        from the previous frame stored in the Widget) */
    widget->subType.scrollArea.hasScrollBarX = 
    widget->subType.scrollArea.hasScrollBarY = false;
    // the presence of a scroll bar in any dimension depends on the size of scrollable region, calculated from the accumulated child widget AABB; 
    //  this region will change, based on the presence of a scroll bar in a perpendicular axis; this is to allow the user to scroll the visible contents
    //  such that they are not obscured by a scroll bar
    widget->subType.scrollArea.aabbScrollableSize = widget->subType.scrollArea.aabbChildrenSize;
    if(widget->subType.scrollArea.aabbScrollableSize.x > widget->size.x)
    {
        widget->subType.scrollArea.aabbScrollableSize.y += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarX = true;
    }
    if(widget->subType.scrollArea.aabbScrollableSize.y > widget->size.y)
    {
        widget->subType.scrollArea.aabbScrollableSize.x += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarY = true;
    }
    // we have to perform the check again, as a scroll bar in a perpendicular axis may now be required if we now require a scroll bar in any given axis; 
    //  of course, making sure that we don't account for a scroll bar in any given axis more than once (hence the `hasScrollBar*` flags)
    if(widget->subType.scrollArea.aabbScrollableSize.x > widget->size.x && !widget->subType.scrollArea.hasScrollBarX)
    {
        widget->subType.scrollArea.aabbScrollableSize.y += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarX = true;
    }
    if(widget->subType.scrollArea.aabbScrollableSize.y > widget->size.y && !widget->subType.scrollArea.hasScrollBarY)
    {
        widget->subType.scrollArea.aabbScrollableSize.x += context->style.windowScrollBarPixelWidth;
        widget->subType.scrollArea.hasScrollBarY = true;
    }
    /* now we can invoke the SCROLL_BAR widgets for this scroll area, if they are required/allowed */
    if(widget->subType.scrollArea.hasScrollBarX)
    {
        korl_assert(widget->subType.scrollArea.aabbScrollableSize.x > widget->size.x);
        korl_gui_setNextWidgetSize((Korl_Math_V2f32){widget->size.x - (widget->subType.scrollArea.hasScrollBarY ? context->style.windowScrollBarPixelWidth/*prevent overlap w/ y-axis SCROLL_BAR*/ : 0), context->style.windowScrollBarPixelWidth});
        korl_gui_setNextWidgetParentAnchor((Korl_Math_V2f32){0, 1});
        korl_gui_setNextWidgetParentOffset((Korl_Math_V2f32){0, context->style.windowScrollBarPixelWidth});
        _korl_gui_setNextWidgetOrderIndex(KORL_C_CAST(u16, -2));
        _korl_gui_widget_scrollArea_scroll(widget, KORL_GUI_SCROLL_BAR_AXIS_X
                                          ,-korl_gui_widgetScrollBar(KORL_RAW_CONST_UTF16(L"_KORL_GUI_SCROLL_AREA_BAR_X"), KORL_GUI_SCROLL_BAR_AXIS_X, widget->size.x, widget->subType.scrollArea.aabbScrollableSize.x, widget->subType.scrollArea.contentOffset.x));
    }
    if(widget->subType.scrollArea.hasScrollBarY)
    {
        korl_assert(widget->subType.scrollArea.aabbScrollableSize.y > widget->size.y);
        korl_gui_setNextWidgetSize((Korl_Math_V2f32){context->style.windowScrollBarPixelWidth, widget->size.y});
        korl_gui_setNextWidgetParentAnchor((Korl_Math_V2f32){1, 0});
        korl_gui_setNextWidgetParentOffset((Korl_Math_V2f32){-context->style.windowScrollBarPixelWidth, 0});
        _korl_gui_setNextWidgetOrderIndex(KORL_C_CAST(u16, -1));
        _korl_gui_widget_scrollArea_scroll(widget, KORL_GUI_SCROLL_BAR_AXIS_Y
                                          ,-korl_gui_widgetScrollBar(KORL_RAW_CONST_UTF16(L"_KORL_GUI_SCROLL_AREA_BAR_Y"), KORL_GUI_SCROLL_BAR_AXIS_Y, widget->size.y, widget->subType.scrollArea.aabbScrollableSize.y, widget->subType.scrollArea.contentOffset.y));
    }
    /**/
    if((flags & KORL_GUI_WIDGET_SCROLL_AREA_FLAG_STICK_MAX_SCROLL) && widget->subType.scrollArea.isScrolledToEndY)
        widget->subType.scrollArea.contentOffset.y = widget->subType.scrollArea.aabbScrollableSize.y - widget->size.y;
    context->currentUserWidgetIndex = korl_checkCast_u$_to_i16(widget - context->stbDaWidgets);
    _korl_gui_resetTransientNextWidgetModifiers();
}
korl_internal KORL_FUNCTION_korl_gui_widgetScrollAreaEnd(korl_gui_widgetScrollAreaEnd)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    korl_assert(arrlenu(context->stbDaWidgetParentStack) > 0);
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    const _Korl_Gui_Widget*const widget = context->stbDaWidgets + widgetIndex;
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_SCROLL_AREA);
    _korl_gui_resetTransientNextWidgetModifiers();
}
korl_internal KORL_FUNCTION_korl_gui_widgetInputText(korl_gui_widgetInputText)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    bool newAllocation = false;
    /* create a widget id hash using string.pool + string.handle */
    u64 identifierHashComponents[2];
    identifierHashComponents[0] = string.handle;
    identifierHashComponents[1] = korl_checkCast_cvoidp_to_u64(string.pool);
    identifierHashComponents[0] = korl_memory_acu16_hash(KORL_STRUCT_INITIALIZE(acu16){.data = KORL_C_CAST(u16*, &(identifierHashComponents[0]))
                                                                                      ,.size = sizeof(identifierHashComponents) / sizeof(u16)});
    /* invoke the widget */
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(identifierHashComponents[0], KORL_GUI_WIDGET_TYPE_INPUT_TEXT, &newAllocation);
    context->currentUserWidgetIndex = korl_checkCast_u$_to_i16(widget - context->stbDaWidgets);
    /* configure subType-specific widget data */
    widget->canBeActiveLeaf = true;
    widget->subType.inputText.string = string;
    const u32 stringGraphemes = korl_stringPool_getGraphemeSize(string);
    if(newAllocation)
    {
        widget->subType.inputText.stringCursorGraphemeIndex     = stringGraphemes;
        widget->subType.inputText.stringCursorGraphemeSelection = 0;
        widget->subType.inputText.inputMode                     = true;
    }
    else
    {
        KORL_MATH_ASSIGN_CLAMP_MAX(widget->subType.inputText.stringCursorGraphemeIndex, stringGraphemes);
        if(korl_checkCast_u$_to_i$(widget->subType.inputText.stringCursorGraphemeIndex) + widget->subType.inputText.stringCursorGraphemeSelection < 0)
            widget->subType.inputText.stringCursorGraphemeSelection = -korl_checkCast_u$_to_i$(widget->subType.inputText.stringCursorGraphemeIndex);
        if(widget->subType.inputText.stringCursorGraphemeIndex + widget->subType.inputText.stringCursorGraphemeSelection > stringGraphemes)
            widget->subType.inputText.stringCursorGraphemeSelection = stringGraphemes - widget->subType.inputText.stringCursorGraphemeIndex;
    }
    widget->subType.inputText.isInputEnabled = inputEnabled;
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
    /**/
    const u8 enterKeyEventsReceived = widget->subType.inputText.enterKeyEventsReceived;
    widget->subType.inputText.enterKeyEventsReceived = 0;
    _korl_gui_resetTransientNextWidgetModifiers();
    return enterKeyEventsReceived;
}
korl_internal f32 korl_gui_widgetScrollBar(acu16 label, Korl_Gui_ScrollBar_Axis axis, f32 scrollRegionVisible, f32 scrollRegionContent, f32 contentOffset)
{
    f32 contentScrollDelta = 0;
    if(korl_math_isNearlyZero(scrollRegionVisible) || korl_math_isNearlyZero(scrollRegionContent))
        goto return_contentScrollDelta;
    _Korl_Gui_Context*const context = _korl_gui_context;
    bool newAllocation = false;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(korl_checkCast_cvoidp_to_u64(label.data), KORL_GUI_WIDGET_TYPE_SCROLL_BAR, &newAllocation);
    context->currentUserWidgetIndex = korl_checkCast_u$_to_i16(widget - context->stbDaWidgets);
    /**/
    korl_assert(scrollRegionContent > scrollRegionVisible);
    const f32 clippedSize = scrollRegionContent - scrollRegionVisible;
    f32 scrollPositionRatio = (axis == KORL_GUI_SCROLL_BAR_AXIS_X ? -contentOffset : contentOffset) / clippedSize;
    if(!korl_math_isNearlyZero(widget->subType.scrollBar.draggedMagnitude))
    {
        switch(widget->subType.scrollBar.dragMode)
        {
        case _KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_SLIDER:{
            /* if the bar was adjusted by the user, we need to transform this screen-space value into a ratio to return to the caller */
            const f32 ratioDeltaPerPixel = scrollRegionContent / scrollRegionVisible;
            contentScrollDelta = widget->subType.scrollBar.draggedMagnitude*ratioDeltaPerPixel;
            break;}
        case _KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_CONTROL:{
            contentScrollDelta = widget->subType.scrollBar.draggedMagnitude;// directly scroll by absolute mouse delta
            break;}
        }
        // recalculate scroll position ratio so the slider appears at the updated position for this frame //
        contentOffset -= contentScrollDelta;
        scrollPositionRatio = (axis == KORL_GUI_SCROLL_BAR_AXIS_X ? -contentOffset : contentOffset) / clippedSize;
    }
    KORL_MATH_ASSIGN_CLAMP(scrollPositionRatio, 0, 1);
    widget->subType.scrollBar.axis                = axis;
    widget->subType.scrollBar.visibleRegionRatio  = scrollRegionVisible / scrollRegionContent;
    widget->subType.scrollBar.scrollPositionRatio = scrollPositionRatio;
    widget->subType.scrollBar.draggedMagnitude    = 0;
    /* these widgets will not support children, so we must pop widget from the parent stack */
    const u16 widgetIndex = arrpop(context->stbDaWidgetParentStack);
    korl_assert(widgetIndex == widget - context->stbDaWidgets);
    /**/
    return_contentScrollDelta:
        _korl_gui_resetTransientNextWidgetModifiers();
        return contentScrollDelta;
}
korl_internal void korl_gui_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(!korl_memory_allocator_isFragmented(_korl_gui_context->allocatorHandleHeap))
        return;
    Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 16);
    KORL_MEMORY_STB_DA_DEFRAGMENT(stackAllocator, stbDaDefragmentPointers, _korl_gui_context);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_gui_context->stbDaUsedWidgets, _korl_gui_context);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_gui_context->stbDaWidgets    , _korl_gui_context);
    const _Korl_Gui_Widget*const widgetsEnd = _korl_gui_context->stbDaWidgets + arrlen(_korl_gui_context->stbDaWidgets);
    for(_Korl_Gui_Widget* widget = _korl_gui_context->stbDaWidgets; widget < widgetsEnd; widget++)
        _korl_gui_widget_collectDefragmentPointers(widget, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_gui_context->stbDaWidgets);
    korl_stringPool_collectDefragmentPointers(_korl_gui_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_gui_context);
    korl_memory_allocator_defragment(_korl_gui_context->allocatorHandleHeap, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
}
korl_internal u32 korl_gui_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.data = KORL_C_CAST(u8*, &_korl_gui_context), .size = sizeof(_korl_gui_context)});
    return byteOffset;
}
korl_internal void korl_gui_memoryStateRead(const u8* memoryState)
{
    _korl_gui_context = *KORL_C_CAST(_Korl_Gui_Context**, memoryState);
    _korl_gui_frameBegin();// begin a new frame, since it is entirely possible that we saved state with dirty transient data!
}
