#include "korl-command.h"
#include "korl-memory.h"
#include "korl-string.h"
#include "korl-stb-ds.h"
#include "korl-stringPool.h"
#include "korl-windows-globalDefines.h"
typedef struct _Korl_Command
{
    Korl_StringPool_String stringCommand;
    fnSig_korl_command_callback* callback;// @TODO: this is insufficient; if we load a memory state OR reload a code module, the code module with this callback will no longer be loaded, so we need to know (1) the code module where this callback can be found, & (2) the name of the function itself so we can query for it at the appropriate times
} _Korl_Command;
typedef struct _Korl_Command_Context
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_StringPool stringPool;
    _Korl_Command* stbDaCommands;
} _Korl_Command_Context;
korl_global_variable _Korl_Command_Context _korl_command_context;
korl_internal KORL_FUNCTION_korl_command_callback(_korl_command_commandHelp)
{
    korl_log(INFO, "KORL command list:");
    const _Korl_Command*const commandsEnd = _korl_command_context.stbDaCommands + arrlen(_korl_command_context.stbDaCommands);
    for(const _Korl_Command* c = _korl_command_context.stbDaCommands; c < commandsEnd; c++)
        korl_log(INFO, "\t%hs", string_getRawUtf8(c->stringCommand));
}
korl_internal void korl_command_initialize(void)
{
    _korl_command_context.allocator  = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(1), L"korl-command", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL);
    _korl_command_context.stringPool = korl_stringPool_create(_korl_command_context.allocator);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_command_context.allocator), _korl_command_context.stbDaCommands, 32);
    korl_command_register(KORL_RAW_CONST_UTF8("help"), _korl_command_commandHelp);
}
korl_internal KORL_FUNCTION_korl_command_invoke(korl_command_invoke)
{
    // korl_log(VERBOSE, "korl_command_invoke: \"%.*hs\"", rawUtf8.size, rawUtf8.data);
    acu8* stbDaTokens = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_command_context.allocator), stbDaTokens, 16);
    for(Korl_String_CodepointTokenizerUtf8 tokenizer = korl_string_codepointTokenizerUtf8_initialize(rawUtf8.data, rawUtf8.size, ' '/*no need to transcode, as we know this code unit maps directly to a codepoint*/)
       ;!korl_string_codepointTokenizerUtf8_done(&tokenizer)
       ; korl_string_codepointTokenizerUtf8_next(&tokenizer))
    {
        const acu8 token = {.data=tokenizer.tokenStart, .size=tokenizer.tokenEnd - tokenizer.tokenStart};
        // korl_log(VERBOSE, "\ttoken: \"%.*hs\"", token.size, token.data);
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_command_context.allocator), stbDaTokens, token);
    }
    if(arrlenu(stbDaTokens))
    {
        /* use the first token to find the command whose string matches */
        const _Korl_Command*const commandsEnd = _korl_command_context.stbDaCommands + arrlen(_korl_command_context.stbDaCommands);
        const _Korl_Command*      command     = _korl_command_context.stbDaCommands;
        for(; command < commandsEnd; command++)
            if(string_equalsAcu8(command->stringCommand, stbDaTokens[0]))
                break;
        /* if we found a command, let's invoke it! */
        if(command < commandsEnd)
            command->callback();
        else
            korl_log(WARNING, "command \"%.*hs\" not found", stbDaTokens[0].size, stbDaTokens[0].data);
    }
    mcarrfree(KORL_STB_DS_MC_CAST(_korl_command_context.allocator), stbDaTokens);
}
korl_internal KORL_FUNCTION_korl_command_register(korl_command_register)
{
    _Korl_Command* command = NULL;
    /* check to see if this command already exists in the database */
    const _Korl_Command*const commandsEnd = _korl_command_context.stbDaCommands + arrlen(_korl_command_context.stbDaCommands);
    for(_Korl_Command* c = _korl_command_context.stbDaCommands; c < commandsEnd; c++)
        if(string_equalsAcu8(c->stringCommand, utf8CommandName))
        {
            command = c;
            goto command_set;
        }
    /* if it doesn't, we need to add a new command */
    if(!command)
    {
        KORL_ZERO_STACK(_Korl_Command, newCommand);
        newCommand.stringCommand = korl_stringNewAcu8(&_korl_command_context.stringPool, utf8CommandName);
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_command_context.allocator), _korl_command_context.stbDaCommands, newCommand);
        command = &arrlast(_korl_command_context.stbDaCommands);
    }
    command_set:
        /* update the registered command with the provided data */
        command->callback = callback;
}
korl_internal void korl_command_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_command_context, sizeof(_korl_command_context));
    ///
}
korl_internal bool korl_command_saveStateRead(HANDLE hFile)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    if(!KORL_WINDOWS_CHECK(ReadFile(hFile, &_korl_command_context, sizeof(_korl_command_context), NULL/*bytes read*/, NULL/*no overlapped*/)))
        return false;
    return true;
}
