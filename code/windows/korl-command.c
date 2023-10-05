//KORL-ISSUE-000-000-182: command: refactor this to use korl-functionDynamo; move to korl/code folder since we shouldn't need platform-specific APIs to achieve this
#include "korl-command.h"
#include "korl-memory.h"
#include "korl-string.h"
#include "korl-stb-ds.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-utility-string.h"
#include "korl-windows-globalDefines.h"
typedef struct _Korl_Command_Module
{
    Korl_StringPool_String stringName;
    HMODULE                handle;
} _Korl_Command_Module;
typedef struct _Korl_Command
{
    Korl_StringPool_String       stringCommand;
    Korl_StringPool_String       stringCallback;
    Korl_StringPool_String       stringModule;// weak reference; _Korl_Command should never allocate a new string & store it here, nor should it ever free this String
    fnSig_korl_command_callback* callback;
} _Korl_Command;
typedef struct _Korl_Command_Context
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_StringPool*            stringPool;// Korl_StringPool structs _must_ be unmanaged allocations (allocations with an unchanging memory address), because we're likely going to have a shit-ton of Strings which point to the pool address for convenience
    Korl_StringPool_String      stringPlatformModuleName;
    _Korl_Command_Module*       stbDaModules;
    _Korl_Command*              stbDaCommands;
} _Korl_Command_Context;
korl_global_variable _Korl_Command_Context* _korl_command_context;// NOTE: we are _only_ storing a pointer to the context in data segment memory since we want to (re)store the entire context in korl-memoryState, so the context is allocated from its own `allocator` member
korl_internal void _korl_command_destroy(_Korl_Command* context)
{
    string_free(&context->stringCommand);
    string_free(&context->stringCallback);
    string_free(&context->stringModule);
    korl_memory_zero(context, sizeof(*context));
}
korl_internal void _korl_command_findCallbackAddress(_Korl_Command* command, HMODULE moduleHandle)
{
    command->callback = KORL_C_CAST(fnSig_korl_command_callback*, KORL_C_CAST(void*, GetProcAddress(moduleHandle, string_getRawUtf8(&command->stringCallback))));
    if(!command->callback)
        korl_log(WARNING, "command \"%hs\" callback \"%hs\" not found!", string_getRawUtf8(&command->stringCommand), string_getRawUtf8(&command->stringCallback));
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(_korl_command_commandHelp)
{
    korl_log(INFO, "KORL command list:");
    const _Korl_Command*const commandsEnd = _korl_command_context->stbDaCommands + arrlen(_korl_command_context->stbDaCommands);
    for(_Korl_Command* c = _korl_command_context->stbDaCommands; c < commandsEnd; c++)
        if(c->callback)// ignore commands that have an invalid callback
            korl_log(INFO, "\t%hs", string_getRawUtf8(&c->stringCommand));
}
korl_internal void korl_command_initialize(acu8 utf8PlatformModuleName)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(1);
    const Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-command", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_command_context = korl_allocate(allocator, sizeof(*_korl_command_context));
    _korl_command_context->allocator                = allocator;
    _korl_command_context->stringPool               = korl_allocate(allocator, sizeof(*_korl_command_context->stringPool));
    *_korl_command_context->stringPool              = korl_stringPool_create(_korl_command_context->allocator);
    _korl_command_context->stringPlatformModuleName = korl_stringNewAcu8(_korl_command_context->stringPool, utf8PlatformModuleName);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_command_context->allocator), _korl_command_context->stbDaModules , 8);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_command_context->allocator), _korl_command_context->stbDaCommands, 32);
    korl_command_registerModule(GetModuleHandle(NULL), utf8PlatformModuleName);
    korl_command_register(KORL_RAW_CONST_UTF8("help"), _korl_command_commandHelp);
}
korl_internal void korl_command_registerModule(HMODULE moduleHandle, acu8 utf8ModuleName)
{
    _Korl_Command_Module* module = NULL;
    _Korl_Command_Module* modulesEnd = _korl_command_context->stbDaModules + arrlen(_korl_command_context->stbDaModules);
    for(_Korl_Command_Module* m = _korl_command_context->stbDaModules; m < modulesEnd; m++)
        if(string_equalsAcu8(&m->stringName, utf8ModuleName))
        {
            module = m;
            goto module_set;
        }
    if(!module)
    {
        KORL_ZERO_STACK(_Korl_Command_Module, newModule);
        newModule.stringName = korl_stringNewAcu8(_korl_command_context->stringPool, utf8ModuleName);
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_command_context->allocator), _korl_command_context->stbDaModules, newModule);
        module = &arrlast(_korl_command_context->stbDaModules);
    }
    module_set:
        module->handle = moduleHandle;
        /* iterate over all commands, & re-obtain their callback address if they 
            are registered to this code module */
        const _Korl_Command*const commandsEnd = _korl_command_context->stbDaCommands + arrlen(_korl_command_context->stbDaCommands);
        for(_Korl_Command* command = _korl_command_context->stbDaCommands; command < commandsEnd; command++)
            if(string_equals(&command->stringModule, &module->stringName))
                _korl_command_findCallbackAddress(command, module->handle);
}
korl_internal KORL_FUNCTION_korl_command_invoke(korl_command_invoke)
{
    // korl_log(VERBOSE, "korl_command_invoke: \"%.*hs\"", rawUtf8.size, rawUtf8.data);
    /* we create a temporary string so that we can modify it such that each 
        token is null-terminated */
    const u$ tempUtf8BufferBytes = rawUtf8.size + 1/*include space for a null-terminator code unit*/;
    au8 tempUtf8 = {.data = korl_allocate(allocatorFrame, tempUtf8BufferBytes), .size = rawUtf8.size};
    korl_memory_copy(tempUtf8.data, rawUtf8.data, tempUtf8BufferBytes);
    /**/
    acu8* stbDaTokens = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_command_context->allocator), stbDaTokens, 16);
    for(Korl_String_CodepointTokenizerUtf8 tokenizer = korl_string_codepointTokenizerUtf8_initialize(tempUtf8.data, tempUtf8.size, ' '/*no need to transcode, as we know this code unit maps directly to a codepoint*/)
       ;!korl_string_codepointTokenizerUtf8_done(&tokenizer)
       ; korl_string_codepointTokenizerUtf8_next(&tokenizer))
    {
        const acu8 token = {.data=tokenizer.tokenStart, .size=tokenizer.tokenEnd - tokenizer.tokenStart};
        *KORL_C_CAST(u8*, tokenizer.tokenEnd) = '\0';// we can do this because we are tokenizing from a transient copy of the rawUtf8 string
        // korl_log(VERBOSE, "\ttoken: \"%.*hs\"", token.size, token.data);
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_command_context->allocator), stbDaTokens, token);
    }
    if(arrlenu(stbDaTokens))
    {
        /* use the first token to find the command whose string matches */
        const _Korl_Command*const commandsEnd = _korl_command_context->stbDaCommands + arrlen(_korl_command_context->stbDaCommands);
        _Korl_Command*            command     = _korl_command_context->stbDaCommands;
        for(; command < commandsEnd; command++)
            if(string_equalsAcu8(&command->stringCommand, stbDaTokens[0]))
                break;
        /* if we found a command, let's invoke it! */
        if(command < commandsEnd)
            if(command->callback)
                command->callback(arrlenu(stbDaTokens), stbDaTokens);
            else/* if the command doesn't have a valid callback, there's nothing we can do but to warn the user; this scenario is likely caused by re-registration of a module that no longer contains the callback code */
                korl_log(WARNING, "command \"%.*hs\" callback invalid", stbDaTokens[0].size, stbDaTokens[0].data);
        else
            korl_log(WARNING, "command \"%.*hs\" not found", stbDaTokens[0].size, stbDaTokens[0].data);
    }
    mcarrfree(KORL_STB_DS_MC_CAST(_korl_command_context->allocator), stbDaTokens);
}
korl_internal KORL_FUNCTION__korl_command_register(_korl_command_register)
{
    _Korl_Command* command = NULL;
    /* check to see if this command already exists in the database */
    const _Korl_Command*const commandsEnd = _korl_command_context->stbDaCommands + arrlen(_korl_command_context->stbDaCommands);
    for(_Korl_Command* c = _korl_command_context->stbDaCommands; c < commandsEnd; c++)
        if(string_equalsAcu8(&c->stringCommand, utf8CommandName))
        {
            korl_assert(string_equalsAcu8(&c->stringCallback, utf8Callback));// the callback's symbol _must_ be the same as when it was first registered!
            command = c;
            goto command_set;
        }
    /* if it doesn't, we need to add a new command */
    if(!command)
    {
        KORL_ZERO_STACK(_Korl_Command, newCommand);
        newCommand.stringCommand  = korl_stringNewAcu8(_korl_command_context->stringPool, utf8CommandName);
        newCommand.stringCallback = korl_stringNewAcu8(_korl_command_context->stringPool, utf8Callback);
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_command_context->allocator), _korl_command_context->stbDaCommands, newCommand);
        command = &arrlast(_korl_command_context->stbDaCommands);
    }
    command_set:
        /* update the registered command with the provided data */
        /* determine the code module based on the callback's function address */
        HMODULE moduleHandle = NULL;
        KORL_WINDOWS_CHECK(GetModuleHandleEx(  GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 
                                             | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
                                            ,KORL_C_CAST(LPCTSTR, callback)
                                            ,&moduleHandle));
        /* find the Command_Module in the database */
        const _Korl_Command_Module*      module     = _korl_command_context->stbDaModules;
        const _Korl_Command_Module*const modulesEnd = _korl_command_context->stbDaModules + arrlen(_korl_command_context->stbDaModules);
        for(; module < modulesEnd; module++)
            if(module->handle == moduleHandle)
                break;
        korl_assert(module < modulesEnd);// this code module _must_ be registered in our module database!!!
        /* store a weak reference to the Command_Module in the Command */
        command->stringModule = module->stringName;
        /* find the callback address of the Command/Module pair; note that we 
            _could_ just pass the function pointer directly, but we require the 
            ability to derive the function pointer at any time anyways (such as 
            when loading a memory state, or when a code module is hot-reloaded), 
            so we might as well just do it right away to verify that everything 
            is in order */
        _korl_command_findCallbackAddress(command, moduleHandle);
}
korl_internal void korl_command_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(!korl_memory_allocator_isFragmented(_korl_command_context->allocator))
        return;
    Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 8);
    KORL_MEMORY_STB_DA_DEFRAGMENT                (stackAllocator, stbDaDefragmentPointers, _korl_command_context);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_command_context->stbDaModules , _korl_command_context);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_command_context->stbDaCommands, _korl_command_context);
    korl_stringPool_collectDefragmentPointers(_korl_command_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_command_context);
    korl_memory_allocator_defragment(_korl_command_context->allocator, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
}
korl_internal u32 korl_command_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.data = KORL_C_CAST(u8*, &_korl_command_context), .size = sizeof(_korl_command_context)});
    return byteOffset;
}
korl_internal void korl_command_memoryStateRead(const u8* memoryState)
{
    _korl_command_context = *KORL_C_CAST(_Korl_Command_Context**, memoryState);
    korl_command_registerModule(GetModuleHandle(NULL), string_getRawAcu8(&_korl_command_context->stringPlatformModuleName));
}
