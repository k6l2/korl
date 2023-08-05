#include "korl-functionDynamo.h"
#include "utility/korl-utility-stb-ds.h"
#include "korl-windows-globalDefines.h"
#define _LOCAL_STRING_POOL_POINTER (_korl_functionDynamo_context->stringPool)
typedef struct _Korl_FunctionDynamo_CodeModule
{
    Korl_StringPool_String name;
    HMODULE                handle;
} _Korl_FunctionDynamo_CodeModule;
typedef struct _Korl_FunctionDynamo_Function
{
    Korl_StringPool_String moduleName;// weak reference; _Korl_Command should never allocate a new string & store it here, nor should it ever free this String
    Korl_StringPool_String cExportName;
    FARPROC                pointer;
} _Korl_FunctionDynamo_Function;

typedef struct _Korl_FunctionDynamo_Context
{
    Korl_Memory_AllocatorHandle      allocator;
    Korl_StringPool*                 stringPool;
    Korl_StringPool_String           stringPlatformModuleName;
    _Korl_FunctionDynamo_CodeModule* stbDaModules;
    Korl_Pool                        functionPool;// pool of _Korl_FunctionDynamo_Function; it's a bit over-kill to use Korl_Pool for this purpose, since I don't think we're ever going to be _removing_ registered functions from the functionDynamo, but it should perfectly serve our needs here with little overhead
} _Korl_FunctionDynamo_Context;
korl_global_variable _Korl_FunctionDynamo_Context* _korl_functionDynamo_context;
korl_internal void _korl_functionDynamo_function_getAddress(_Korl_FunctionDynamo_Function* function, HMODULE moduleHandle)
{
    function->pointer = GetProcAddress(moduleHandle, string_getRawUtf8(&function->cExportName));
    if(!function->pointer)
        korl_log(ERROR, "function \"%hs\" address not found", string_getRawUtf8(&function->cExportName));
}
korl_internal void korl_functionDynamo_initialize(acu8 utf8PlatformModuleName)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(1);
    Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-functionDynamo", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_functionDynamo_context = korl_allocate(allocator, sizeof(*_korl_functionDynamo_context));
    _Korl_FunctionDynamo_Context*const context = _korl_functionDynamo_context;
    context->allocator                = allocator;
    context->stringPool               = korl_allocate(context->allocator, sizeof(*context->stringPool));
    *context->stringPool              = korl_stringPool_create(context->allocator);
    context->stringPlatformModuleName = string_newAcu8(utf8PlatformModuleName);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaModules, 8);
    korl_pool_initialize(&context->functionPool, context->allocator, sizeof(_Korl_FunctionDynamo_Function), 32);
    korl_functionDynamo_registerModule(GetModuleHandle(NULL), utf8PlatformModuleName);
}
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_functionDynamo_registerModule_forEach)
{
    _Korl_FunctionDynamo_CodeModule*const module   = KORL_C_CAST(_Korl_FunctionDynamo_CodeModule*, userData);
    _Korl_FunctionDynamo_Function*const   function = KORL_C_CAST(_Korl_FunctionDynamo_Function*, item);
    if(string_equals(&module->name, &function->moduleName))
        _korl_functionDynamo_function_getAddress(function, module->handle);
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal void korl_functionDynamo_registerModule(Korl_FunctionDynamo_CodeModuleHandle moduleHandle, acu8 utf8ModuleName)
{
    _Korl_FunctionDynamo_Context*const context = _korl_functionDynamo_context;
    _Korl_FunctionDynamo_CodeModule* module = NULL;
    {/* either obtain the existing module in our database, or create a new one */
        const _Korl_FunctionDynamo_CodeModule*const modulesEnd = context->stbDaModules + arrlen(context->stbDaModules);
        for(module = context->stbDaModules; module < modulesEnd; module++)
            if(string_equalsAcu8(&module->name, utf8ModuleName))
                break;
        if(module >= modulesEnd)
        {
            KORL_ZERO_STACK(_Korl_FunctionDynamo_CodeModule, newModule);
            newModule.name = string_newAcu8(utf8ModuleName);
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaModules, newModule);
            module = &arrlast(context->stbDaModules);
        }
    }
    /* set the module in our database to use the desired module handle */
    korl_assert(module && module >= context->stbDaModules && module < context->stbDaModules + arrlen(context->stbDaModules));
    module->handle = moduleHandle;
    /* iterate over all functions & re-obtain their addresses if they are registered to this code module */
    korl_pool_forEach(&context->functionPool, _korl_functionDynamo_registerModule_forEach, module);
}
korl_internal void korl_functionDynamo_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(!korl_memory_allocator_isFragmented(_korl_functionDynamo_context->allocator))
        return;
    Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 8);
    KORL_MEMORY_STB_DA_DEFRAGMENT(stackAllocator, stbDaDefragmentPointers, _korl_functionDynamo_context);
    korl_stringPool_collectDefragmentPointers(_korl_functionDynamo_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_functionDynamo_context);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_functionDynamo_context->stbDaModules, _korl_functionDynamo_context);
    korl_pool_collectDefragmentPointers(&_korl_functionDynamo_context->functionPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_functionDynamo_context);
    korl_memory_allocator_defragment(_korl_functionDynamo_context->allocator, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
}
korl_internal u32 korl_functionDynamo_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.data = KORL_C_CAST(u8*, &_korl_functionDynamo_context), .size = sizeof(_korl_functionDynamo_context)});
    return byteOffset;
}
korl_internal void korl_functionDynamo_memoryStateRead(const u8* memoryState)
{
    _korl_functionDynamo_context = *KORL_C_CAST(_Korl_FunctionDynamo_Context**, memoryState);
    korl_functionDynamo_registerModule(GetModuleHandle(NULL), string_getRawAcu8(&_korl_functionDynamo_context->stringPlatformModuleName));
}
typedef struct _Korl_FunctionDynamo_Register_ForEachContext
{
    _Korl_FunctionDynamo_Function* result;
    acu8                           utf8Function;
} _Korl_FunctionDynamo_Register_ForEachContext;
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_functionDynamo_register_forEach)
{
    _Korl_FunctionDynamo_Register_ForEachContext*const forEachContext = KORL_C_CAST(_Korl_FunctionDynamo_Register_ForEachContext*, userData);
    _Korl_FunctionDynamo_Function*const               function       = KORL_C_CAST(_Korl_FunctionDynamo_Function*, item);
    if(string_equalsAcu8(&function->cExportName, forEachContext->utf8Function))
    {
        forEachContext->result = function;
        return KORL_POOL_FOR_EACH_DONE;
    }
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal KORL_FUNCTION__korl_functionDynamo_register(_korl_functionDynamo_register)
{
    _Korl_FunctionDynamo_Context*const context = _korl_functionDynamo_context;
    /* check if this function already exists in the database */
    KORL_ZERO_STACK(_Korl_FunctionDynamo_Register_ForEachContext, forEachContext);
    forEachContext.utf8Function = utf8Function;
    korl_pool_forEach(&context->functionPool, _korl_functionDynamo_register_forEach, &forEachContext);
    /* if it doesn't, add a new entry to the database */
    if(!forEachContext.result)
    {
        korl_pool_add(&context->functionPool, 0/*itemType; unused; all pool items are uniform*/, &forEachContext.result);
        forEachContext.result->cExportName = string_newAcu8(utf8Function);
    }
    /* update the function's pointer to the one provided by the user */
    forEachContext.result->pointer = KORL_C_CAST(FARPROC, function);
    /* determine the code module based on the callback's function address */
    HMODULE moduleHandle = NULL;
    KORL_WINDOWS_CHECK(GetModuleHandleEx(  GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 
                                         | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
                                        ,KORL_C_CAST(LPCTSTR, function)
                                        ,&moduleHandle));
    /* find the code module in the database */
    _Korl_FunctionDynamo_CodeModule* module = NULL;
    const _Korl_FunctionDynamo_CodeModule*const modulesEnd = context->stbDaModules + arrlen(context->stbDaModules);
    for(module = context->stbDaModules; module < modulesEnd; module++)
        if(module->handle == moduleHandle)
            break;
    if(module >= modulesEnd)
    {
        korl_log(ERROR, "code module for function \"%.*hs\" is not registered", utf8Function.size, utf8Function.data);
        return 0;
    }
    /* store a weak reference to the code module in the function */
    forEachContext.result->moduleName = module->name;
    return korl_pool_itemHandle(&context->functionPool, forEachContext.result);
}
korl_internal KORL_FUNCTION_korl_functionDynamo_get(korl_functionDynamo_get)
{
    _Korl_FunctionDynamo_Context*const context = _korl_functionDynamo_context;
    Korl_FunctionDynamo_FunctionHandle tempHandle = functionHandle;
    _Korl_FunctionDynamo_Function* function = korl_pool_get(&context->functionPool, &tempHandle);
    if(function)
        return KORL_C_CAST(void*, function->pointer);
    korl_log(ERROR, "invalid function handle: %llu(0x%llX)", functionHandle, functionHandle);
    return NULL;
}
#undef _LOCAL_STRING_POOL_POINTER
