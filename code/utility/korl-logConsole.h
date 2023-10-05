#pragma once
#include "utility/korl-stringPool.h"
#include "korl-interface-platform.h"
typedef struct Korl_LogConsole
{
    bool enable;
    f32 fadeInRatio;
    u$ lastLoggedCharacters;
    Korl_StringPool_String stringInput;// the input buffer the user modifies via the korl-gui INPUT_TEXT widget
    Korl_StringPool_String stringInputLastEnabled;// allows us to retroactively undo string buffer inputs caused by whatever key was used to disable the console
    bool commandInvoked;// if set, stringInput will be cleared the following call to korl_logConsole_update
} Korl_LogConsole;
korl_internal Korl_LogConsole korl_logConsole_create(Korl_StringPool* stringPool);
korl_internal void            korl_logConsole_toggle(Korl_LogConsole* context);
korl_internal void            korl_logConsole_update(Korl_LogConsole* context, f32 deltaSeconds, fnSig_korl_log_getBuffer* _korl_log_getBuffer, Korl_Math_V2u32 windowSize, Korl_Memory_AllocatorHandle allocatorFrame);
