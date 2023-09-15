#include "korl-logConsole.h"
#include "utility/korl-utility-string.h"
korl_internal Korl_LogConsole korl_logConsole_create(Korl_StringPool* stringPool)
{
    KORL_ZERO_STACK(Korl_LogConsole, logConsole);
    logConsole.stringInput = korl_stringNewEmptyUtf8(stringPool, 0);
    return logConsole;
}
korl_internal void korl_logConsole_toggle(Korl_LogConsole* context)
{
    context->enable = !context->enable;
}
korl_internal void korl_logConsole_update(Korl_LogConsole* context, f32 deltaSeconds, fnSig_korl_log_getBuffer *_korl_log_getBuffer, Korl_Math_V2u32 windowSize, Korl_Memory_AllocatorHandle allocatorStack)
{
    context->fadeInRatio = korl_math_f32_exponentialDecay(context->fadeInRatio, context->enable ? 1.f : 0.f, 40.f, deltaSeconds);
    if(!context->enable && context->stringInputLastEnabled.handle)
    {
        /* retroactively undo any input events that may have been applied to the 
            input buffer due to any key press that is required to close the 
            console; we have to do this here because it's not safe to free 
            stringInput until we know for certain that input events are 
            finished, which is one of the preconditions of the update function */
        string_free(&context->stringInput);
        context->stringInput            = context->stringInputLastEnabled;
        context->stringInputLastEnabled = KORL_STRINGPOOL_STRING_NULL;
    }
    if(context->commandInvoked)
    {
        string_reserveUtf8(&context->stringInput, 0);
        context->commandInvoked = false;
    }
    if(context->enable || !korl_math_isNearlyEqualEpsilon(context->fadeInRatio, 0, .01f))
    {
        u$       loggedBytes      = 0;
        acu16    logBuffer        = _korl_log_getBuffer(&loggedBytes);
        const u$ loggedCharacters = loggedBytes / sizeof(*logBuffer.data);
        const u$ newCharacters    = KORL_MATH_MIN(loggedCharacters - context->lastLoggedCharacters, logBuffer.size);
        logBuffer.data = logBuffer.data + logBuffer.size - newCharacters;
        logBuffer.size = newCharacters;
        const f32 consoleSizeY = KORL_C_CAST(f32, windowSize.y)*0.5f;
        korl_gui_setNextWidgetSize(KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){KORL_C_CAST(f32, windowSize.x)
                                                                          ,consoleSizeY});
        korl_gui_setNextWidgetParentOffset(KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){0, KORL_C_CAST(f32, windowSize.y) + consoleSizeY*(1.f - context->fadeInRatio)});
        korl_gui_windowBegin(L"console", NULL, KORL_GUI_WINDOW_STYLE_FLAG_DEFAULT_ACTIVE);
            korl_gui_setNextWidgetSize(KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){KORL_C_CAST(f32, windowSize.x)
                                                                              ,consoleSizeY - 32/*allow room for text input widget*/});//KORL-ISSUE-000-000-111: gui: this sucks; is there a way for us to have korl-gui automatically determine how tall the text scroll area should be under the hood?
            korl_gui_widgetScrollAreaBegin(KORL_RAW_CONST_UTF16(L"console scroll area"), KORL_GUI_WIDGET_SCROLL_AREA_FLAG_STICK_MAX_SCROLL);
                if(korl_gui_widgetText(L"console text", logBuffer, 1'000/*max line count*/, NULL/*codepointTest*/, NULL/*codepointTestData*/, KORL_GUI_WIDGET_TEXT_FLAG_LOG))
                    context->lastLoggedCharacters = loggedCharacters;
            korl_gui_widgetScrollAreaEnd();
            string_free(&context->stringInputLastEnabled);
            if(context->enable)
                context->stringInputLastEnabled = string_copy(context->stringInput);
            else
                context->stringInputLastEnabled = KORL_STRINGPOOL_STRING_NULL;
            if(korl_gui_widgetInputText(context->stringInput, context->enable))
            {
                /* the user pressed [Enter]; submit stringInput to the platform layer */
                korl_command_invoke(string_getRawAcu8(&context->stringInput), allocatorStack);
                context->commandInvoked = true;
            }
        korl_gui_windowEnd();
    }
    else
        context->lastLoggedCharacters = 0;
}
