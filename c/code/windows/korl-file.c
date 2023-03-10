#include "korl-file.h"
#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-time.h"
#include "korl-windows-window.h"
#include "korl-assetCache.h"
#include "korl-stringPool.h"
#include "korl-checkCast.h"
#include "korl-stb-ds.h"
#include "korl-resource.h"
#include "korl-command.h"
#include <minidumpapiset.h>
#ifdef _LOCAL_STRING_POOL_POINTER
#undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_file_context.stringPool))
korl_global_const i8 _KORL_SAVESTATE_UNIQUE_FILE_ID[] = "KORL-SAVESTATE";
korl_global_const u32 _KORL_SAVESTATE_VERSION = 0;
typedef struct _Korl_File_SaveStateEnumerateContext_Allocator
{
    u32 heaps;
    u32 allocations;
} _Korl_File_SaveStateEnumerateContext_Allocator;
typedef struct _Korl_File_SaveStateEnumerateContext
{
    u8*                                             stbDaSaveStateBuffer;
    _Korl_File_SaveStateEnumerateContext_Allocator* stbDaAllocatorData;
    u8                                              currentAllocator;
} _Korl_File_SaveStateEnumerateContext;
typedef struct _Korl_File_AsyncOpertion
{
    Korl_File_AsyncIoHandle handle;
    OVERLAPPED overlapped;
    DWORD bytesPending;
    DWORD bytesTransferred;// a value of 0 indicates the operation is incomplete
} _Korl_File_AsyncOpertion;
typedef struct _Korl_File_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool stringPool;
    Korl_StringPool_String directoryStrings[KORL_FILE_PATHTYPE_ENUM_COUNT];
    HANDLE                 directoryHandleOnSubtreeChangeLastWrite[KORL_FILE_PATHTYPE_ENUM_COUNT];
    /** We cannot use a KORL_MEMORY_POOL here, because we need the memory 
     * locations of used OVERLAPPED structs to remain completely STATIC once in 
     * use by Windows.  
     * The indices into this pool should be considered as "handles" to users of 
     * this code module.
     * Individual elements of this pool are considered to be "unused" if the 
     * \c handle member == \c 0 .  */
    _Korl_File_AsyncOpertion asyncPool[128];/// KORL-ISSUE-000-000-090: file: why did I have to change this 64=>128?  This feels so jank right now...
    Korl_File_AsyncIoHandle nextAsyncIoHandle;
    HANDLE handleIoCompletionPort;
    u64* stbDaUsedFileAsyncKeys;
    _Korl_File_SaveStateEnumerateContext saveStateEnumContext;
} _Korl_File_Context;
korl_global_variable _Korl_File_Context _korl_file_context;
korl_internal void _korl_file_sanitizeFilePath(Korl_StringPool_String* stringFilePath)
{
    /* So apparently, when you are doing the weird "\\?\" prefix on file paths, 
        this will no longer allow you to mix & match '\' & '/' characters for 
        directory separation.  We must sanitize stringFilePath manually...  
        We just replace each '/' character with '\\': */
    if(0 == string_findUtf16(stringFilePath, L"\\\\\?\\", 0))
    {
        for(u32 i = 0;;)
        {
            i = string_findUtf16(stringFilePath, L"/", i);
            if(i >= string_getRawSizeUtf16(stringFilePath))
                break;
            string_getRawWriteableUtf16(stringFilePath)[i] = L'\\';
        }
    }
}
korl_internal bool _korl_file_open(Korl_File_PathType pathType
                                  ,const wchar_t* fileName
                                  ,Korl_File_Descriptor_Flags flags
                                  ,Korl_File_Descriptor* o_fileDescriptor
                                  ,bool createNew
                                  ,bool clearFileContents/*only used if createNew is false*/)
{
    _Korl_File_Context*const context = &_korl_file_context;
    bool result = true;
    korl_assert(pathType < KORL_FILE_PATHTYPE_ENUM_COUNT);
    Korl_StringPool_String stringFilePath = string_copy(context->directoryStrings[pathType]);
    string_appendFormatUtf16(&stringFilePath, L"\\%ws", fileName);
    _korl_file_sanitizeFilePath(&stringFilePath);
    DWORD createDesiredAccess = 0;
    DWORD createFileFlags = 0;
    if(flags & KORL_FILE_DESCRIPTOR_FLAG_READ)
        createDesiredAccess |= GENERIC_READ;
    if(flags & KORL_FILE_DESCRIPTOR_FLAG_WRITE)
        createDesiredAccess |= GENERIC_WRITE;
    if(flags & KORL_FILE_DESCRIPTOR_FLAG_ASYNC)
        createFileFlags |= FILE_FLAG_OVERLAPPED;
    if(createFileFlags == 0)
        createFileFlags = FILE_ATTRIBUTE_NORMAL;
    const HANDLE hFile = CreateFile(string_getRawUtf16(&stringFilePath)
                                   ,createDesiredAccess
                                   ,FILE_SHARE_READ
                                   ,NULL/*default security*/
                                   ,createNew ? CREATE_NEW : (clearFileContents ? TRUNCATE_EXISTING : OPEN_EXISTING)
                                   ,createFileFlags
                                   ,NULL/*no template file*/);
    if(hFile == INVALID_HANDLE_VALUE)
    {
        korl_log(WARNING, "CreateFile('%ws') failed; GetLastError==0x%X", 
                 string_getRawUtf16(&stringFilePath), GetLastError());
        result = false;
        goto cleanUp;
    }
    korl_memory_zero(o_fileDescriptor, sizeof(*o_fileDescriptor));
    o_fileDescriptor->handle = hFile;
    o_fileDescriptor->flags  = flags;
    if(flags & KORL_FILE_DESCRIPTOR_FLAG_ASYNC)
    {
        /* create a unique async key for this file */
        u64 newKey = 0;// 0 => invalid key
        for(u64 k = 1; k <= arrlenu(context->stbDaUsedFileAsyncKeys) + 1; k++)
        {
            newKey = k;
            for(u64 ku = 0; ku < arrlenu(context->stbDaUsedFileAsyncKeys); ku++)
            {
                if(context->stbDaUsedFileAsyncKeys[ku] == k)
                {
                    newKey = 0;
                    break;
                }
            }
            if(newKey)
                break;
        }
        korl_assert(newKey);
        /* associate this file handle with the I/O completion port */
        const HANDLE resultCreateIoCompletionPort = CreateIoCompletionPort(hFile, context->handleIoCompletionPort, newKey, 0/*ignored*/);
        if(resultCreateIoCompletionPort == NULL)
            korl_logLastError("CreateIoCompletionPort failed!");
        /**/
        o_fileDescriptor->asyncKey = newKey;
    }
cleanUp:
    string_free(&stringFilePath);
    return result;
}
/** The caller is responsible for freeing the string returned via 
 * the \c o_filePathOldest parameter.
 * \return the # of matched files */
korl_internal u$ _korl_file_findOldestFile(Korl_StringPool_String findFileDirectory, const wchar_t* findFileNamePattern
                                          ,Korl_StringPool_String* o_filePathOldest)
{
    u$ findFileCount = 0;
    o_filePathOldest->handle = 0;
    o_filePathOldest->pool = 0;

    Korl_StringPool_String findFilePattern = string_copy(findFileDirectory);
    string_appendUtf16(&findFilePattern, L"\\");
    string_appendUtf16(&findFilePattern, findFileNamePattern);
    WIN32_FIND_DATA findFileData;
    HANDLE findHandle = FindFirstFile(string_getRawUtf16(&findFilePattern), &findFileData);
    if(findHandle == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() != ERROR_FILE_NOT_FOUND)
            korl_logLastError("FindFirstFile(%ws) failed", string_getRawUtf16(&findFilePattern));
        goto cleanUp;
    }
    findFileCount = 1;
    FILETIME creationTimeOldest = findFileData.ftCreationTime;
    *o_filePathOldest = string_copy(findFileDirectory);
    string_appendUtf16(o_filePathOldest, L"\\");
    string_appendUtf16(o_filePathOldest, findFileData.cFileName);
    for(;;)
    {
        const BOOL resultFindNextFile = FindNextFile(findHandle, &findFileData);
        if(!resultFindNextFile)
        {
            korl_assert(GetLastError() == ERROR_NO_MORE_FILES);
            break;
        }
        findFileCount++;
        if(CompareFileTime(&findFileData.ftCreationTime, &creationTimeOldest) < 0)
        {
            creationTimeOldest = findFileData.ftCreationTime;
            string_free(o_filePathOldest);
            *o_filePathOldest = string_copy(findFileDirectory);
            string_appendUtf16(o_filePathOldest, L"\\");
            string_appendUtf16(o_filePathOldest, findFileData.cFileName);
        }
    }
    if(!FindClose(findHandle))
        korl_logLastError("FindClose failed");
cleanUp:
    string_free(&findFilePattern);
    return findFileCount;
}
korl_internal void _korl_file_copyFiles(Korl_StringPool_String sourceDirectory, const wchar_t* sourcePattern, Korl_StringPool_String destinationDirectory)
{
    Korl_StringPool_String findFilePattern = string_copy(sourceDirectory);
    string_appendUtf16(&findFilePattern, L"\\");
    string_appendUtf16(&findFilePattern, sourcePattern);
    WIN32_FIND_DATA findFileData;
    HANDLE findHandle = FindFirstFile(string_getRawUtf16(&findFilePattern), &findFileData);
    if(findHandle == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() != ERROR_FILE_NOT_FOUND)
            korl_logLastError("FindFirstFile(%ws) failed", string_getRawUtf16(&findFilePattern));
        goto cleanUp;
    }
    for(;;)
    {
        Korl_StringPool_String pathSource = string_copy(sourceDirectory);
        string_appendUtf16(&pathSource, L"\\");
        string_appendUtf16(&pathSource, findFileData.cFileName);
        Korl_StringPool_String pathDestination = string_copy(destinationDirectory);
        string_appendUtf16(&pathDestination, L"\\");
        string_appendUtf16(&pathDestination, findFileData.cFileName);
        if(!CopyFile(string_getRawUtf16(&pathSource), string_getRawUtf16(&pathDestination), FALSE))
            korl_log(WARNING, "CopyFile(%ws, %ws) failed!  GetLastError=0x%X", 
                     string_getRawUtf16(&pathSource), string_getRawUtf16(&pathDestination), GetLastError());
        else
            korl_log(INFO, "copied file \"%ws\" to \"%ws\"", 
                     string_getRawUtf16(&pathSource), string_getRawUtf16(&pathDestination));
        string_free(&pathSource);
        string_free(&pathDestination);
        const BOOL resultFindNextFile = FindNextFile(findHandle, &findFileData);
        if(!resultFindNextFile)
        {
            korl_assert(GetLastError() == ERROR_NO_MORE_FILES);
            break;
        }
    }
    if(!FindClose(findHandle))
        korl_logLastError("FindClose failed");
cleanUp:
    string_free(&findFilePattern);
}
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_file_saveStateCreate_allocationEnumCallback)
{
    _Korl_File_Context*const                   context     = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    /* copy the allocation meta data & memory to the save state buffer */
    const u16 fileCharacterCount = korl_checkCast_u$_to_u16(korl_string_sizeUtf16(meta->file));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &fileCharacterCount, sizeof(fileCharacterCount));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, meta->file         , fileCharacterCount*sizeof(*meta->file));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &meta->line        , sizeof(meta->line));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &allocation        , sizeof(allocation));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &meta->bytes       , sizeof(meta->bytes));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, allocation         , meta->bytes);
    enumContext->stbDaAllocatorData[enumContext->currentAllocator].allocations++;// accumulate the total # of allocations per allocator
    return true;// true => continue enumerating
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_file_saveStateCreate_allocatorEnumCallback_allocationPass)
{
    _Korl_File_Context*const context                       = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
        return true;//true => continue iterating over allocators
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), enumContext->stbDaAllocatorData, KORL_STRUCT_INITIALIZE_ZERO(_Korl_File_SaveStateEnumerateContext_Allocator));
    /* enumarate over all allocations & write those to the save state buffer */
    korl_time_probeStart(enumerateAllocations);
    korl_memory_allocator_enumerateAllocations(opaqueAllocator, _korl_file_saveStateCreate_allocationEnumCallback, enumContext);
    korl_time_probeStop(enumerateAllocations);
    enumContext->currentAllocator++;
    return true;//true => continue iterating over allocators
}
korl_internal KORL_HEAP_ENUMERATE_CALLBACK(_korl_file_saveStateCreate_heapEnumCallback)
{
    _Korl_File_Context*const                   context     = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &virtualAddressStart, sizeof(virtualAddressStart));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &virtualAddressEnd  , sizeof(virtualAddressEnd));
    enumContext->stbDaAllocatorData[enumContext->currentAllocator].heaps++;
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_file_saveStateCreate_allocatorEnumCallback_heapsPass)
{
    _Korl_File_Context*const context                       = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
        return true;//true => continue iterating over allocators
    korl_memory_allocator_enumerateHeaps(opaqueAllocator, _korl_file_saveStateCreate_heapEnumCallback, enumContext);
    enumContext->currentAllocator++;
    return true;//true => continue iterating over allocators
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_file_saveStateCreate_allocatorEnumCallback_allocatorPass)
{
    _Korl_File_Context*const context                       = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
        return true;//true => continue iterating over allocators
    /* now we can write the allocator descriptors to the save state buffer */
    const u16 nameCharacterCount = korl_checkCast_u$_to_u16(korl_string_sizeUtf16(allocatorName));
    const _Korl_File_SaveStateEnumerateContext_Allocator*const allocator = enumContext->stbDaAllocatorData + enumContext->currentAllocator;
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &nameCharacterCount      , sizeof(nameCharacterCount));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, allocatorName            , nameCharacterCount*sizeof(*allocatorName));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &(allocator->heaps)      , sizeof(allocator->heaps));
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &enumContext->stbDaSaveStateBuffer, &(allocator->allocations), sizeof(allocator->allocations));
    enumContext->currentAllocator++;
    return true;//true => continue iterating over allocators
}
korl_internal void _korl_file_createParentDirectoriesRecursive(Korl_StringPool_String filePath)
{
    _Korl_File_Context*const context = &_korl_file_context;
    Korl_StringPool_String filePathLocalCopy = string_copy(filePath);
    const u32 filePathSize     = string_getRawSizeUtf16(&filePathLocalCopy);
    u16*const filePathRawUtf16 = string_getRawWriteableUtf16(&filePathLocalCopy);//KORL-ISSUE-000-000-076: file: replace working with raw writable UTF-16 pointers, we need codepoint iteration/get/set API
    u32 c = 0;
    if(0 == string_findUtf16(&filePathLocalCopy, L"\\\\\?\\", 0))
        c += 4;
    for(; c < filePathSize; c++)
    {
        if(filePathRawUtf16[c] == L'\\' || filePathRawUtf16[c] == L'/')
        {
            filePathRawUtf16[c] = L'\0';
            if(!CreateDirectory(filePathRawUtf16, NULL/*default security*/))
                switch(GetLastError())
                {
                case ERROR_ALREADY_EXISTS:
                    break;// no need to do anything!
                case ERROR_PATH_NOT_FOUND:
                    korl_log(ERROR, "CreateDirectory(%ws) failed: path not found", filePathRawUtf16);
                    goto cleanUp;
                }
            filePathRawUtf16[c] = L'\\';
        }
    }
cleanUp:
    string_free(&filePathLocalCopy);
}
korl_internal void korl_file_initialize(void)
{
    //KORL-ISSUE-000-000-057: file: initialization occurs before korl_crash_initialize, despite calling korl_assert & korl_log(ERROR)
    _Korl_File_Context*const context = &_korl_file_context;
    korl_memory_zero(context, sizeof(*context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(128);
    context->allocatorHandle        = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"korl-file", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    context->stringPool             = korl_stringPool_create(context->allocatorHandle);
    context->nextAsyncIoHandle      = 1;
    context->handleIoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0/*use default; # of processors in the system*/);
    if(context->handleIoCompletionPort == NULL)
        korl_logLastError("CreateIoCompletionPort failed!");
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaUsedFileAsyncKeys, 16);
    /* determine where the current working directory is in Windows */
    const DWORD currentDirectoryCharacters = GetCurrentDirectory(0, NULL);// _including_ the null-terminator
    if(currentDirectoryCharacters == 0)
        korl_logLastError("GetCurrentDirectory failed!");
    context->directoryStrings[KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY] = string_newEmptyUtf16(currentDirectoryCharacters - 1/*ignore the null-terminator; string pool will add one internally*/);
    const DWORD currentDirectoryCharacters2 = 
        GetCurrentDirectory(currentDirectoryCharacters, 
                            string_getRawWriteableUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY]));
    if(currentDirectoryCharacters2 == 0)
        korl_logLastError("GetCurrentDirectory failed!");
    korl_log(INFO, "Current working directory: %ws", 
             string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY]));
    /* determine where the application's local storage is in Windows */
    {
        wchar_t* pathLocalAppData;
        const HRESULT resultGetKnownFolderPath = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 
                                                                      KF_FLAG_CREATE | KF_FLAG_INIT, 
                                                                      NULL/*token of current user*/,
                                                                      &pathLocalAppData);
        korl_assert(resultGetKnownFolderPath == S_OK);
        context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA] = string_newUtf16(pathLocalAppData);
        string_appendUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA], L"\\");
        string_appendUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA], KORL_APPLICATION_NAME);
        CoTaskMemFree(pathLocalAppData);
        korl_log(INFO, "directoryLocalData=%ws", string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]));
    }
    if(!CreateDirectory(string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]), NULL))
    {
        const int errorCode = GetLastError();
        switch(errorCode)
        {
        case ERROR_ALREADY_EXISTS:{
            korl_log(INFO, "directory '%ws' already exists", 
                     string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]));
            break;}
        default:{
            korl_logLastError("CreateDirectory('%ws') failed", 
                              string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]));
            break;}
        }
    }
    /* temporary data directory */
    WCHAR bufferTempPath[MAX_PATH+1/*according to MSDN, this is the maximum possible return value for GetTempPath 🤡*/];
    const DWORD tempPathSize = GetTempPath(korl_arraySize(bufferTempPath), bufferTempPath);
    if(tempPathSize == 0)
        korl_logLastError("GetTempPath failed!");
    else
    {
        context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA] = string_newUtf16(bufferTempPath);
        string_appendUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA], KORL_APPLICATION_NAME);
    }
    korl_log(INFO, "directoryTemporaryData=%ws", 
             string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]));
    if(!CreateDirectory(string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]), NULL))
    {
        const int errorCode = GetLastError();
        switch(errorCode)
        {
        case ERROR_ALREADY_EXISTS:{
            korl_log(INFO, "directory '%ws' already exists", 
                     string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]));
            break;}
        default:{
            korl_logLastError("CreateDirectory('%ws') failed", 
                              string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]));
            break;}
        }
    }
    /* executable file directory */
    context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY] = string_newEmptyUtf16(MAX_PATH);// just an initial guess; you'll see why shortly...
    DWORD resultGetModuleFileName = string_getRawSizeUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]);
    // That's right, in order to properly use this shitty API, Microsoft forces 
    //  us to pull this reallocate loop bullshit... 🤡 //
    for(;;)
    {
        resultGetModuleFileName = GetModuleFileName(NULL/*executable file of current process*/, 
                                                    string_getRawWriteableUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]), 
                                                    string_getRawSizeUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]));
        if(resultGetModuleFileName == 0)
            korl_logLastError("GetModuleFileName failed!");
        if(resultGetModuleFileName >= string_getRawSizeUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]))
            string_reserveUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY], 
                                2*string_getRawSizeUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]));
        else
            break;
    }
    // null-terminate the module file path at the last backslash, so we 
    //  effectively get the parent directory of the exe file //
    const wchar_t* lastBackslash = string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]);
    for(const wchar_t* c = string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]); *c; c++)
        if(*c == '\\')
            lastBackslash = c;
    string_reserveUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY], 
                        korl_checkCast_i$_to_u32(lastBackslash - string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY])));
    //  //
    korl_log(INFO, "directoryExecutable=%ws", 
             string_getRawUtf16(&context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]));
    /* prepend special string of characters to directory strings, which will 
        allow the Windows API that consumes these paths (CreateDirectory, 
        CreateFile, etc...) to extend the file path string size limit from 
        MAX_PATH(260) to 32,767 */
    for(i32 pt = 0; pt < KORL_FILE_PATHTYPE_ENUM_COUNT; pt++)
        string_prependUtf16(&context->directoryStrings[pt], L"\\\\\?\\");
    /* persistent storage of a save-state memory buffer */
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->saveStateEnumContext.stbDaSaveStateBuffer, korl_math_kilobytes(16));
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->saveStateEnumContext.stbDaAllocatorData, 32);
    /* setup notification handles to detect subtree file changes for each path type */
    for(i32 pt = 0; pt < KORL_FILE_PATHTYPE_ENUM_COUNT; pt++)
    {
        context->directoryHandleOnSubtreeChangeLastWrite[pt] = FindFirstChangeNotification(string_getRawUtf16(&context->directoryStrings[pt]), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
        if(INVALID_HANDLE_VALUE == context->directoryHandleOnSubtreeChangeLastWrite[pt])
            korl_logLastError("FindFirstChangeNotification failed");
    }
}
korl_internal i32 korl_file_makePathString(Korl_File_PathType pathType, const wchar_t* fileName, u8* o_pathUtf8Buffer, u$ pathUtf8BufferSize)
{
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(pathType < KORL_FILE_PATHTYPE_ENUM_COUNT);
    Korl_StringPool_String stringFilePath = string_copy(context->directoryStrings[pathType]);
    string_appendFormatUtf16(&stringFilePath, L"\\%ws", fileName);
    _korl_file_sanitizeFilePath(&stringFilePath);
    acu8 rawFilePathUtf8 = string_getRawAcu8(&stringFilePath);
    const i32 resultUnitsWritten = rawFilePathUtf8.size + 1 > pathUtf8BufferSize
                                   ? -korl_checkCast_u$_to_i32(rawFilePathUtf8.size + 1)
                                   : korl_checkCast_u$_to_i32(rawFilePathUtf8.size + 1);
    if(resultUnitsWritten > 0)
        korl_memory_copy(o_pathUtf8Buffer, rawFilePathUtf8.data, (rawFilePathUtf8.size + 1) * sizeof(*rawFilePathUtf8.data));
    string_free(&stringFilePath);
    return resultUnitsWritten;
}
korl_internal bool _korl_file_directory_create(Korl_File_PathType pathType, const wchar_t* directoryName)
{
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(pathType < KORL_FILE_PATHTYPE_ENUM_COUNT);
    Korl_StringPool_String stringPath = string_copy(context->directoryStrings[pathType]);
    string_appendFormatUtf16(&stringPath, L"\\%ws", directoryName);
    _korl_file_sanitizeFilePath(&stringPath);
    bool resultSuccess = true;
    if(!CreateDirectory(string_getRawUtf16(&stringPath), NULL/*default security*/))
        switch(GetLastError())
        {
        case ERROR_ALREADY_EXISTS:
            break;
        case ERROR_PATH_NOT_FOUND:
            korl_log(ERROR, "CreateDirectory(%ws) failed: path not found", string_getRawUtf16(&stringPath));
        default:
            resultSuccess = false;
            korl_logLastError("CreateDirectory(%ws) failed", string_getRawUtf16(&stringPath));
            break;
        }
    string_free(&stringPath);
    return resultSuccess;
}
korl_internal bool korl_file_open(Korl_File_PathType pathType
                                 ,const wchar_t* fileName
                                 ,Korl_File_Descriptor* o_fileDescriptor
                                 ,bool async)
{
    Korl_File_Descriptor_Flags flags = KORL_FILE_DESCRIPTOR_FLAG_READ 
                                     | KORL_FILE_DESCRIPTOR_FLAG_WRITE;
    if(async)
        flags |= KORL_FILE_DESCRIPTOR_FLAG_ASYNC;
    return _korl_file_open(pathType, fileName, flags, o_fileDescriptor, false/*don't create new file*/, false/*don't clear the file*/);
}
korl_internal bool korl_file_create(Korl_File_PathType pathType
                                   ,const wchar_t* fileName
                                   ,Korl_File_Descriptor* o_fileDescriptor
                                   ,bool async)
{
    /* enumerate over all directories in the fileName & ensure that they exist 
        by attempting to create each one */
    for(const wchar_t* c = fileName; *c; c++)
    {
        if(*c != L'/' && *c != L'\"')
            continue;
        Korl_StringPool_String stringPath = string_newAcu16(((acu16){.data = fileName, .size = c - fileName}));
        const bool successCreateDirectory = _korl_file_directory_create(pathType, string_getRawUtf16(&stringPath));
        string_free(&stringPath);
        if(!successCreateDirectory)
            return false;
    }
    /**/
    Korl_File_Descriptor_Flags flags = KORL_FILE_DESCRIPTOR_FLAG_READ 
                                     | KORL_FILE_DESCRIPTOR_FLAG_WRITE;
    if(async)
        flags |= KORL_FILE_DESCRIPTOR_FLAG_ASYNC;
    return _korl_file_open(pathType, fileName, flags, o_fileDescriptor, true/*create new*/, false/*don't clear the file*/);
}
korl_internal bool korl_file_openClear(Korl_File_PathType pathType
                                      ,const wchar_t* fileName
                                      ,Korl_File_Descriptor* o_fileDescriptor
                                      ,bool async)
{
    Korl_File_Descriptor_Flags flags = KORL_FILE_DESCRIPTOR_FLAG_READ 
                                     | KORL_FILE_DESCRIPTOR_FLAG_WRITE;
    if(async)
        flags |= KORL_FILE_DESCRIPTOR_FLAG_ASYNC;
    return _korl_file_open(pathType, fileName, flags, o_fileDescriptor, false/*don't create new*/, true/*clear the file*/);
}
korl_internal void korl_file_close(Korl_File_Descriptor* fileDescriptor)
{
    if(!CloseHandle(fileDescriptor->handle))
        korl_logLastError("CloseHandle failed!");
    korl_memory_zero(fileDescriptor, sizeof(*fileDescriptor));
}
korl_internal HMODULE korl_file_loadDynamicLibrary(Korl_File_PathType pathType, const wchar_t* fileName)
{
    HMODULE hModule = NULL;
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(pathType < KORL_FILE_PATHTYPE_ENUM_COUNT);
    Korl_StringPool_String filePath = string_copy(context->directoryStrings[pathType]);
    string_appendFormatUtf16(&filePath, L"\\%ws", fileName);
    _korl_file_sanitizeFilePath(&filePath);
    hModule = LoadLibrary(string_getRawUtf16(&filePath));
    if(!hModule)
        korl_logLastError("LoadLibrary(\"%ws\") failed", string_getRawUtf16(&filePath));
    cleanUp_returnHModule:
        string_free(&filePath);
        return hModule;
}
korl_internal bool korl_file_copy(Korl_File_PathType pathTypeFileName   , const wchar_t* fileName, 
                                  Korl_File_PathType pathTypeFileNameNew, const wchar_t* fileNameNew, 
                                  bool replaceFileNameNewIfExists)
{
    bool result = true;
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(pathTypeFileName    < KORL_FILE_PATHTYPE_ENUM_COUNT);
    korl_assert(pathTypeFileNameNew < KORL_FILE_PATHTYPE_ENUM_COUNT);
    Korl_StringPool_String filePath    = string_copy(context->directoryStrings[pathTypeFileName]);
    Korl_StringPool_String filePathNew = string_copy(context->directoryStrings[pathTypeFileNameNew]);
    string_appendFormatUtf16(&filePath   , L"\\%ws", fileName);
    string_appendFormatUtf16(&filePathNew, L"\\%ws", fileNameNew);
    if(!CopyFile(string_getRawUtf16(&filePath), string_getRawUtf16(&filePathNew), replaceFileNameNewIfExists))
    {
        switch(GetLastError())
        {
        case ERROR_FILE_NOT_FOUND:{
            break;}
        }
        result = false;
        goto cleanUp;
    }
    cleanUp:
    string_free(&filePath);
    string_free(&filePathNew);
    return result;
}
korl_internal Korl_File_ResultRenameReplace korl_file_renameReplace(Korl_File_PathType pathTypeFileName,    const wchar_t* fileName, 
                                                                    Korl_File_PathType pathTypeFileNameNew, const wchar_t* fileNameNew)
{
    Korl_File_ResultRenameReplace result = KORL_FILE_RESULT_RENAME_REPLACE_SUCCESS;
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(pathTypeFileName    < KORL_FILE_PATHTYPE_ENUM_COUNT);
    korl_assert(pathTypeFileNameNew < KORL_FILE_PATHTYPE_ENUM_COUNT);
    Korl_StringPool_String filePathNew = {0};
    Korl_StringPool_String filePath = string_copy(context->directoryStrings[pathTypeFileName]);
    string_appendUtf16(&filePath, L"\\");
    string_appendUtf16(&filePath, fileName);
    _korl_file_sanitizeFilePath(&filePath);
    if(GetFileAttributes(string_getRawUtf16(&filePath)) == INVALID_FILE_ATTRIBUTES)
    {
        result = KORL_FILE_RESULT_RENAME_REPLACE_SOURCE_FILE_DOES_NOT_EXIST;
        goto cleanUp;
    }
    filePathNew = string_copy(context->directoryStrings[pathTypeFileNameNew]);
    string_appendUtf16(&filePathNew, L"\\");
    string_appendUtf16(&filePathNew, fileNameNew);
    _korl_file_sanitizeFilePath(&filePathNew);
    if(GetFileAttributes(string_getRawUtf16(&filePathNew)) != INVALID_FILE_ATTRIBUTES)
        if(DeleteFile(string_getRawUtf16(&filePathNew)))
            korl_log(INFO, "file '%ws' deleted", string_getRawUtf16(&filePathNew));
        else
        {
            result = KORL_FILE_RESULT_RENAME_REPLACE_FAIL_DELETE_FILE_TO_REPLACE;
            goto cleanUp;
        }
    if(!MoveFile(string_getRawUtf16(&filePath), 
                 string_getRawUtf16(&filePathNew)))
    {
        result = KORL_FILE_RESULT_RENAME_REPLACE_FAIL_MOVE_OLD_FILE;
        goto cleanUp;
    }
    cleanUp:
        string_free(&filePath);
        string_free(&filePathNew);
        return result;
}
korl_internal Korl_File_AsyncIoHandle korl_file_writeAsync(Korl_File_Descriptor fileDescriptor, const void* buffer, u$ bufferBytes)
{
    _Korl_File_Context*const context = &_korl_file_context;
    /* find an unused async io handle 
        NOTE: this is basically copy-pasta from stringPool handle implementation */
    const Korl_File_AsyncIoHandle maxHandles = KORL_U32_MAX - 1/*since 0 is an invalid handle*/;
    Korl_File_AsyncIoHandle newHandle = 0;
    for(Korl_File_AsyncIoHandle h = 0; h < maxHandles; h++)
    {
        newHandle = context->nextAsyncIoHandle;
        /* If another async op has this handle, we nullify the newHandle so that 
            we can move on to the next handle candidate */
        for(u$ i = 0; i < korl_arraySize(context->asyncPool); i++)
            if(context->asyncPool[i].handle == newHandle)
            {
                newHandle = 0;
                break;
            }
        if(context->nextAsyncIoHandle == KORL_U32_MAX)
            context->nextAsyncIoHandle = 1;
        else
            context->nextAsyncIoHandle++;
        if(newHandle)
            break;
    }
    korl_assert(newHandle);// sanity check
    /* find an unused OVERLAPPED pool slot */
    u$ i = 0;
    for(; i < korl_arraySize(context->asyncPool); i++)
        if(0 == context->asyncPool[i].handle)
            break;
    korl_assert(i < korl_arraySize(context->asyncPool));
    _Korl_File_AsyncOpertion*const pAsyncOp = &(context->asyncPool[i]);
    korl_memory_zero(pAsyncOp, sizeof(*pAsyncOp));
    pAsyncOp->handle                = newHandle;
    pAsyncOp->overlapped.Offset     = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
    pAsyncOp->overlapped.OffsetHigh = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
    pAsyncOp->bytesPending          = korl_checkCast_u$_to_u32(bufferBytes);
    /* dispatch the async write command */
    const BOOL resultWriteFile = WriteFile(fileDescriptor.handle, 
                                           buffer, pAsyncOp->bytesPending, 
                                           NULL/*# bytes written; ignored for async ops*/, 
                                           &(pAsyncOp->overlapped));
    if(!resultWriteFile)
        if(GetLastError() != ERROR_IO_PENDING)
            korl_logLastError("WriteFile failed!");
    return newHandle;
}
korl_internal Korl_File_GetAsyncIoResult korl_file_getAsyncIoResult(Korl_File_AsyncIoHandle* handle, bool blockUntilComplete)
{
    _Korl_File_Context*const context = &_korl_file_context;
    /* find the async operation associated with *handle */
    korl_assert(*handle);
    _Korl_File_AsyncOpertion* asyncOp = NULL;
    for(u$ ap = 0; ap < korl_arraySize(context->asyncPool); ap++)
    {
        if(context->asyncPool[ap].handle == *handle)
        {
            asyncOp = &(context->asyncPool[ap]);
            break;
        }
    }
    if(!asyncOp)
    {
        korl_log(WARNING, "handle %u is invalid!", *handle);
        *handle = 0;// invalidate the caller's handle
        return KORL_FILE_GET_ASYNC_IO_RESULT_INVALID_HANDLE;
    }
    /* check to see if the async operation returned its completion status during 
        another call, since the async calls can be dequeued in any order (the 
        completion queue is _NOT_ FIFO or FILO or anything like that) */
    if(asyncOp->bytesTransferred >= asyncOp->bytesPending)
        goto skipPopCompletionPackets;
    /* pop I/O completion packets from the completion port until it is empty */
    for(;;)
    {
        DWORD bytesTransferred       = 0;
        ULONG_PTR completionKey      = 0;
        LPOVERLAPPED pOverlapped     = NULL;
        const DWORD waitMilliseconds = blockUntilComplete ? INFINITE : 0;
        if(!GetQueuedCompletionStatus(context->handleIoCompletionPort, &bytesTransferred, &completionKey, &pOverlapped, waitMilliseconds))
            if(GetLastError() != WAIT_TIMEOUT)
                korl_logLastError("GetQueuedCompletionStatus failed");
        if(pOverlapped == NULL)
            break;
        bool foundAsyncOp = false;
        for(u$ ap = 0; ap < korl_arraySize(context->asyncPool); ap++)
        {
            if(context->asyncPool[ap].handle == 0)
                continue;
            if(pOverlapped == &(context->asyncPool[ap].overlapped))
            {
                korl_assert(context->asyncPool[ap].bytesTransferred == 0);
                korl_assert(bytesTransferred == context->asyncPool[ap].bytesPending);
                context->asyncPool[ap].bytesTransferred = bytesTransferred;
                foundAsyncOp = true;
                if(context->asyncPool[ap].handle == *handle)
                    goto skipPopCompletionPackets;
                break;
            }
        }
        korl_assert(foundAsyncOp);
    }
    skipPopCompletionPackets:
    if(asyncOp->bytesTransferred >= asyncOp->bytesPending)
    {
        asyncOp->handle = 0;// remove the async op from the pool
        *handle = 0;// invalidate the caller's handle
        return KORL_FILE_GET_ASYNC_IO_RESULT_DONE;
    }
    else
    {
        korl_assert(!blockUntilComplete);
        return KORL_FILE_GET_ASYNC_IO_RESULT_PENDING;
    }
}
korl_internal void korl_file_write(Korl_File_Descriptor fileDescriptor, const void* data, u$ dataBytes)
{
    _Korl_File_Context*const context = &_korl_file_context;
    DWORD bytesTransferred = 0;
    if(fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_ASYNC)
    {
        KORL_ZERO_STACK(OVERLAPPED, overlapped);
        overlapped.Offset     = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
        overlapped.OffsetHigh = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
        const BOOL resultWriteFile = WriteFile(fileDescriptor.handle, 
                                               data, korl_checkCast_u$_to_u32(dataBytes), 
                                               NULL/*# bytes written; ignored for async ops*/, 
                                               &overlapped);
        if(!resultWriteFile)
            if(GetLastError() != ERROR_IO_PENDING)
                korl_logLastError("WriteFile failed!");
        /* repeatedly get completed OVERLAPPED operations until we get the one 
            we just queued */
        for(;;)
        {
            ULONG_PTR completionKey  = 0;
            LPOVERLAPPED pOverlapped = NULL;
            if(!GetQueuedCompletionStatus(context->handleIoCompletionPort, &bytesTransferred, &completionKey, &pOverlapped, INFINITE))
                if(GetLastError() != WAIT_TIMEOUT)
                    korl_logLastError("GetQueuedCompletionStatus failed");
            if(pOverlapped == &overlapped)
                break;
            else
            {
                /* some other async io from the pool must have completed */
                bool foundAsyncOp = false;
                for(u$ ap = 0; ap < korl_arraySize(context->asyncPool); ap++)
                {
                    if(context->asyncPool[ap].handle == 0)
                        continue;
                    if(pOverlapped == &(context->asyncPool[ap].overlapped))
                    {
                        korl_assert(context->asyncPool[ap].bytesTransferred == 0);
                        korl_assert(bytesTransferred == context->asyncPool[ap].bytesPending);
                        context->asyncPool[ap].bytesTransferred = bytesTransferred;
                        foundAsyncOp = true;
                        break;
                    }
                }
                korl_assert(foundAsyncOp);
            }
        }
        goto done_checkBytesWritten;
    }
    if(!WriteFile(fileDescriptor.handle, data, korl_checkCast_u$_to_u32(dataBytes), &bytesTransferred, NULL/*no overlapped*/))
        korl_logLastError("WriteFile failed!");
done_checkBytesWritten:
    korl_assert(bytesTransferred == korl_checkCast_u$_to_u32(dataBytes));
}
korl_internal u32 korl_file_getTotalBytes(Korl_File_Descriptor fileDescriptor)
{
    _Korl_File_Context*const context = &_korl_file_context;
    const DWORD fileSize = GetFileSize(fileDescriptor.handle, NULL/*high file size DWORD*/);
    if(fileSize == INVALID_FILE_SIZE)
        korl_logLastError("GetFileSize failed!");
    return fileSize;
}
korl_internal KorlPlatformDateStamp korl_file_getDateStampLastWrite(Korl_File_Descriptor fileDescriptor)
{
    union
    {
        FILETIME fileTime;
        KorlPlatformDateStamp dateStamp;
    } dateStampUnionFile;
    if(!GetFileTime(fileDescriptor.handle, NULL, NULL, &dateStampUnionFile.fileTime))
        korl_logLastError("GetFileTime failed!");
    return dateStampUnionFile.dateStamp;
}
korl_internal bool korl_file_getDateStampLastWriteFileName(Korl_File_PathType pathType, const wchar_t* fileName, KorlPlatformDateStamp* out_dateStampLastWrite)
{
    bool success = true;
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(pathType < KORL_FILE_PATHTYPE_ENUM_COUNT);
    union
    {
        FILETIME fileTime;
        KorlPlatformDateStamp dateStamp;
    } dateStampUnionFile;
    korl_memory_zero(&dateStampUnionFile, sizeof(dateStampUnionFile));
    Korl_StringPool_String stringFilePath = string_copy(context->directoryStrings[pathType]);
    string_appendFormatUtf16(&stringFilePath, L"\\%ws", fileName);
    _korl_file_sanitizeFilePath(&stringFilePath);
    WIN32_FILE_ATTRIBUTE_DATA fileAttributeData;
    if(!GetFileAttributesEx(string_getRawUtf16(&stringFilePath), GetFileExInfoStandard, &fileAttributeData))
    {
        if(GetLastError() != ERROR_FILE_NOT_FOUND)
            korl_logLastError("GetFileAttributesEx failed!");
        success = false;
        goto cleanUp_returnResult;
    }
    dateStampUnionFile.fileTime = fileAttributeData.ftLastWriteTime;
    cleanUp_returnResult:
    string_free(&stringFilePath);
    if(success)
        *out_dateStampLastWrite = dateStampUnionFile.dateStamp;
    return success;
}
korl_internal bool korl_file_read(Korl_File_Descriptor fileDescriptor, void* buffer, u32 bufferBytes)
{
    _Korl_File_Context*const context = &_korl_file_context;
    DWORD bytesTransferred = 0;
    if(fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_ASYNC)
    {
        KORL_ZERO_STACK(OVERLAPPED, overlapped);
        // we leave the overlapped offsets zeroed so we read from the beginning of the file
        const BOOL resultReadFile = ReadFile(fileDescriptor.handle, 
                                             buffer, bufferBytes, 
                                             NULL/*# bytes read; ignored for async ops*/, 
                                             &overlapped);
        if(!resultReadFile)
            if(GetLastError() != ERROR_IO_PENDING)
            {
                korl_log(WARNING, "ReadFile failed; GetLastError=0x%X", GetLastError());
                return false;
            }
        /* repeatedly get completed OVERLAPPED operations until we get the one 
            we just queued */
        for(;;)
        {
            ULONG_PTR completionKey  = 0;
            LPOVERLAPPED pOverlapped = NULL;
            if(!GetQueuedCompletionStatus(context->handleIoCompletionPort, &bytesTransferred, &completionKey, &pOverlapped, INFINITE))
                if(GetLastError() != WAIT_TIMEOUT)
                    korl_logLastError("GetQueuedCompletionStatus failed");
            if(pOverlapped == &overlapped)
                break;
            else
            {
                /* some other async io from the pool must have completed */
                bool foundAsyncOp = false;
                for(u$ ap = 0; ap < korl_arraySize(context->asyncPool); ap++)
                {
                    if(context->asyncPool[ap].handle == 0)
                        continue;
                    if(pOverlapped == &(context->asyncPool[ap].overlapped))
                    {
                        korl_assert(context->asyncPool[ap].bytesTransferred == 0);
                        korl_assert(bytesTransferred == context->asyncPool[ap].bytesPending);
                        context->asyncPool[ap].bytesTransferred = bytesTransferred;
                        foundAsyncOp = true;
                        break;
                    }
                }
                korl_assert(foundAsyncOp);
            }
        }
        goto done_checkBytesRead;
    }
    if(!ReadFile(fileDescriptor.handle, buffer, bufferBytes, &bytesTransferred, NULL/*no overlapped*/))
    {
        korl_log(WARNING, "ReadFile failed; GetLastError=0x%X", GetLastError());
        return false;
    }
done_checkBytesRead:
    korl_assert(bytesTransferred == bufferBytes);
    return true;
}
korl_internal Korl_File_AsyncIoHandle korl_file_readAsync(Korl_File_Descriptor fileDescriptor, void* buffer, u32 bufferBytes)
{
    _Korl_File_Context*const context = &_korl_file_context;
    /* find an unused async io handle 
        NOTE: this is basically copy-pasta from stringPool handle implementation */
    const Korl_File_AsyncIoHandle maxHandles = KORL_U32_MAX - 1/*since 0 is an invalid handle*/;
    Korl_File_AsyncIoHandle newHandle = 0;
    for(Korl_File_AsyncIoHandle h = 0; h < maxHandles; h++)
    {
        newHandle = context->nextAsyncIoHandle;
        /* If another async op has this handle, we nullify the newHandle so that 
            we can move on to the next handle candidate */
        for(u$ i = 0; i < korl_arraySize(context->asyncPool); i++)
            if(context->asyncPool[i].handle == newHandle)
            {
                newHandle = 0;
                break;
            }
        if(context->nextAsyncIoHandle == KORL_U32_MAX)
            context->nextAsyncIoHandle = 1;
        else
            context->nextAsyncIoHandle++;
        if(newHandle)
            break;
    }
    korl_assert(newHandle);// sanity check
    /* find an unused OVERLAPPED pool slot */
    u$ i = 0;
    for(; i < korl_arraySize(context->asyncPool); i++)
        if(0 == context->asyncPool[i].handle)
            break;
    korl_assert(i < korl_arraySize(context->asyncPool));
    _Korl_File_AsyncOpertion*const pAsyncOp = &(context->asyncPool[i]);
    korl_memory_zero(pAsyncOp, sizeof(*pAsyncOp));
    pAsyncOp->handle       = newHandle;
    pAsyncOp->bytesPending = korl_checkCast_u$_to_u32(bufferBytes);
    // we leave the overlapped offsets zeroed so we read from the beginning of the file
    /* dispatch the async read command */
    const BOOL resultReadFile = ReadFile(fileDescriptor.handle, 
                                         buffer, pAsyncOp->bytesPending, 
                                         NULL/*# bytes read; ignored for async ops*/, 
                                         &(pAsyncOp->overlapped));
    if(!resultReadFile)
        if(GetLastError() != ERROR_IO_PENDING)
            korl_logLastError("ReadFile failed!");
    return newHandle;
}
korl_internal void korl_file_finishAllAsyncOperations(void)
{
    _Korl_File_Context*const context = &_korl_file_context;
    for(u16 a = 0; a < korl_arraySize(context->asyncPool); a++)
    {
        if(0 == context->asyncPool[a].handle)
            continue;
        Korl_File_AsyncIoHandle handle = context->asyncPool[a].handle;
        /* NOTE: here we are invalidating the original async io handle, but that 
            should be okay since we expect that when the save state is loaded, 
            it will likely contain any number of invalid async io handles, which 
            are expected to be handled gracefully by whatever code modules which 
            contain them */
        korl_assert(KORL_FILE_GET_ASYNC_IO_RESULT_DONE == korl_file_getAsyncIoResult(&handle, true/*block until complete*/));
    }
}
korl_internal void korl_file_generateMemoryDump(void* exceptionData, Korl_File_PathType type, u32 maxDumpCount)
{
    korl_shared_const wchar_t DUMP_SUBDIRECTORY[] = L"memory-dumps";
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(type < KORL_FILE_PATHTYPE_ENUM_COUNT);
    Korl_StringPool_String pathFileRead           = {0};
    Korl_StringPool_String pathFileWrite          = {0};
    Korl_StringPool_String dumpDirectory          = {0};
    Korl_StringPool_String pathMemoryDump         = {0};
    Korl_StringPool_String subDirectoryMemoryDump = {0};
    Korl_StringPool_String subPathMemoryState     = {0};
    // derived from MSDN sample code:
    //	https://docs.microsoft.com/en-us/windows/win32/dxtecharts/crash-dump-analysis
    SYSTEMTIME localTime;
    GetLocalTime( &localTime );
    /* create a memory dump directory in the temporary data directory */
    dumpDirectory = string_copy(context->directoryStrings[type]);
    string_appendFormatUtf16(&dumpDirectory, L"\\%ws", DUMP_SUBDIRECTORY);
    if(!CreateDirectory(string_getRawUtf16(&dumpDirectory), NULL/*default security*/))
        switch(GetLastError())
        {
        case ERROR_ALREADY_EXISTS:
            break;
        case ERROR_PATH_NOT_FOUND:
            korl_log(ERROR, "CreateDirectory(%ws) failed: path not found", string_getRawUtf16(&dumpDirectory));
            goto cleanUp;
        }
    /* delete the oldest dump folder after we reach some maximum dump count */
    {
        Korl_StringPool_String filePathOldest = {0};
        korl_assert(maxDumpCount > 0);
        while(maxDumpCount <= _korl_file_findOldestFile(dumpDirectory, L"*-*-*-*-*", &filePathOldest))
        {
            /* apparently pFrom needs to be double null-terminated */
            const u32 filePathOldestSize = string_getRawSizeUtf16(&filePathOldest);
            string_reserveUtf16(&filePathOldest, filePathOldestSize + 1);
            string_getRawWriteableUtf16(&filePathOldest)[filePathOldestSize] = L'\0';
            /**/
            const WCHAR* rawUtf16FilePathOldest = string_getRawUtf16(&filePathOldest);
            /* the special L"\\\\\?\\" file path prefix shit doesn't work on 
                this API, as it turns out... */
            if(0 == string_findUtf16(&filePathOldest, L"\\\\\?\\", 0))
                rawUtf16FilePathOldest += 4;
            /**/
            korl_log(INFO, "deleting oldest dump folder: %ws", rawUtf16FilePathOldest);
            KORL_ZERO_STACK(SHFILEOPSTRUCT, fileOpStruct);
            fileOpStruct.wFunc  = FO_DELETE;
            fileOpStruct.pFrom  = rawUtf16FilePathOldest;
            fileOpStruct.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;
            const int resultFileOpDeleteRecursive = SHFileOperation(&fileOpStruct);
            if(resultFileOpDeleteRecursive != 0)
                korl_log(WARNING, "recursive delete of \"%ws\" failed; result=%i", 
                         rawUtf16FilePathOldest, resultFileOpDeleteRecursive);
            string_free(&filePathOldest);
        }
        string_free(&filePathOldest);
    }
    // Create a companion folder to store PDB files specifically for this dump! //
    subDirectoryMemoryDump = string_newUtf16(L"");
    string_appendFormatUtf16(&subDirectoryMemoryDump, L"\\%ws-%04d%02d%02d-%02d%02d%02d-%ld-%ld", 
                             KORL_APPLICATION_VERSION,
                             localTime.wYear, localTime.wMonth, localTime.wDay,
                             localTime.wHour, localTime.wMinute, localTime.wSecond,
                             GetCurrentProcessId(), GetCurrentThreadId());
    pathMemoryDump = string_copy(dumpDirectory);
    string_append(&pathMemoryDump, &subDirectoryMemoryDump);
    if(!CreateDirectory(string_getRawUtf16(&pathMemoryDump), NULL))
    {
        korl_logLastError("CreateDirectory(%ws) failed!", 
                          string_getRawUtf16(&pathMemoryDump));
        goto cleanUp;
    }
    else
        korl_log(INFO, "created MiniDump directory: %ws", 
                 string_getRawUtf16(&pathMemoryDump));
    // Create the mini dump! //
    pathFileWrite = string_copy(pathMemoryDump);
    string_appendFormatUtf16(&pathFileWrite, L"\\%ws-%04d%02d%02d-%02d%02d%02d-0x%X-0x%X.dmp", 
                             KORL_APPLICATION_VERSION,
                             localTime.wYear, localTime.wMonth, localTime.wDay,
                             localTime.wHour, localTime.wMinute, localTime.wSecond,
                             GetCurrentProcessId(), GetCurrentThreadId());
    const HANDLE hDumpFile = CreateFile(string_getRawUtf16(&pathFileWrite), 
                                        GENERIC_READ|GENERIC_WRITE, 
                                        FILE_SHARE_WRITE|FILE_SHARE_READ, 
                                        0, CREATE_ALWAYS, 0, 0);
    if(INVALID_HANDLE_VALUE == hDumpFile)
    {
        korl_logLastError("CreateFile(%ws) failed!", string_getRawUtf16(&pathFileWrite));
        goto cleanUp;
    }
    PEXCEPTION_POINTERS pExceptionPointers = KORL_C_CAST(PEXCEPTION_POINTERS, exceptionData);
    MINIDUMP_EXCEPTION_INFORMATION expParam = 
        { .ThreadId          = GetCurrentThreadId()
        , .ExceptionPointers = pExceptionPointers
        , .ClientPointers    = TRUE };
    //KORL-ISSUE-000-000-062: crash: minidump memory optimization/focus
    const MINIDUMP_TYPE dumpType = MiniDumpWithDataSegs 
                                 | MiniDumpWithFullMemory 
                                 | MiniDumpIgnoreInaccessibleMemory;
    if(!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
                          hDumpFile, dumpType, 
                          pExceptionPointers ? &expParam : NULL, 
                          NULL/*UserStreamParam*/, NULL/*callback*/))
    {
        korl_log(ERROR, "MiniDumpWriteDump failed!  GetLastError(HRESULT)=0x%x, GetLastError(Win32 code)=0x%x", 
                 GetLastError(), /*derived by reverse engineering HRESULT_FROM_WIN32:*/GetLastError()&0xFFFF);
        goto cleanUp;
    }
    korl_log(INFO, "MiniDump written to: %ws", string_getRawUtf16(&pathFileWrite));
    /* let's also save the state of the top of this frame in the dump folder */
    subPathMemoryState = string_newUtf16(DUMP_SUBDIRECTORY);
    string_append(&subPathMemoryState, &subDirectoryMemoryDump);
    string_appendUtf16(&subPathMemoryState, L"\\last.kms");
    korl_windows_window_saveLastMemoryState(type, string_getRawUtf16(&subPathMemoryState));
    /* Attempt to copy the win32 application's symbol files to the dump 
        location.  This will allow us to at LEAST view the call stack properly 
        during development for all of our minidumps even after making source 
        changes and subsequently rebuilding. */
    // Copy the executable //
    pathFileRead = string_copy(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]);
    string_appendUtf16(&pathFileRead, L"\\");
    string_appendUtf16(&pathFileRead, KORL_APPLICATION_NAME);
    string_appendUtf16(&pathFileRead, L".exe");
    string_free(&pathFileWrite);
    pathFileWrite = string_copy(pathMemoryDump);
    string_appendUtf16(&pathFileWrite, L"\\");
    string_appendUtf16(&pathFileWrite, KORL_APPLICATION_NAME);
    string_appendUtf16(&pathFileWrite, L".exe");
    if(!CopyFile(string_getRawUtf16(&pathFileRead), string_getRawUtf16(&pathFileWrite), FALSE))
        korl_log(WARNING, "CopyFile(%ws, %ws) failed!  GetLastError=0x%X", 
                 string_getRawUtf16(&pathFileRead), string_getRawUtf16(&pathFileWrite), GetLastError());
    else
        korl_log(INFO, "copied file \"%ws\" to \"%ws\"", string_getRawUtf16(&pathFileRead), string_getRawUtf16(&pathFileWrite));
    // Copy all PDB files //
    _korl_file_copyFiles(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY], L"*.pdb", pathMemoryDump);
#if 0/* In the future, we will have to do something a little more sophisticated 
        like when we add the ability to hot-reload game code, since we'll have 
        to implement rolling PDB files due to Windows filesystem constraints.  I 
        have done this before in the original KORL prototype (KML), so I will 
        leave some of that commented out here just in case I delete all that 
        code before game hot-reloading is added: */
    // Find the most recent game*.pdb file, then place the filename into 
    //	`szFileNameCopySource` //
    StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s*.pdb"),
                    g_pathToExe, FILE_NAME_GAME_DLL);
    WIN32_FIND_DATA findFileData;
    HANDLE findHandleGameDll = 
        FindFirstFile(szFileNameCopySource, &findFileData);
    if(findHandleGameDll == INVALID_HANDLE_VALUE)
    {
        platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
                    "Failed to begin search for '%s'!  GetLastError=%i",
                    szFileNameCopySource, GetLastError());
        goto cleanUp;
    }
    FILETIME creationTimeLatest = findFileData.ftCreationTime;
    TCHAR fileNameGamePdb[MAX_PATH];
    StringCchPrintf(fileNameGamePdb, MAX_PATH, TEXT("%s"),
                    findFileData.cFileName);
    while(BOOL findNextFileResult = 
        FindNextFile(findHandleGameDll, &findFileData))
    {
        if(!findNextFileResult && GetLastError() != ERROR_NO_MORE_FILES)
        {
            platformLog("win32-crash", __LINE__, 
                        PlatformLogCategory::K_ERROR,
                        "Failed to find next for '%s'!  GetLastError=%i",
                        szFileNameCopySource, GetLastError());
            goto cleanUp;
        }
        if(CompareFileTime(&findFileData.ftCreationTime, 
                        &creationTimeLatest) > 0)
        {
            creationTimeLatest = findFileData.ftCreationTime;
            StringCchPrintf(fileNameGamePdb, MAX_PATH, TEXT("%s"),
                            findFileData.cFileName);
        }
    }
    if(!FindClose(findHandleGameDll))
    {
        platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
                    "Failed to close search for '%s*.pdb'!  "
                    "GetLastError=%i",
                    FILE_NAME_GAME_DLL, GetLastError());
        goto cleanUp;
    }
    // attempt to copy game's pdb file //
    StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s"),
                    g_pathToExe, fileNameGamePdb);
    StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s"), 
                    szPdbDirectory, fileNameGamePdb);
    if(!CopyFile(szFileNameCopySource, szFileName, false))
    {
        platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
                    "Failed to copy '%s' to '%s'!  GetLastError=%i",
                    szFileNameCopySource, szFileName, GetLastError());
        goto cleanUp;
    }
#endif// ^ either recycle this code at some point, or just delete it
cleanUp:
    string_free(&pathFileRead);
    string_free(&pathFileWrite);
    string_free(&dumpDirectory);
    string_free(&pathMemoryDump);
    string_free(&subDirectoryMemoryDump);
    string_free(&subPathMemoryState);
}
korl_internal bool korl_file_detectsChangedFileLastWrite(Korl_File_PathType type)
{
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(type < KORL_FILE_PATHTYPE_ENUM_COUNT);
    const DWORD resultWaitForObject = WaitForSingleObject(context->directoryHandleOnSubtreeChangeLastWrite[type], 0/*milliseconds*/);
    switch(resultWaitForObject)
    {
    case WAIT_OBJECT_0:{
        KORL_WINDOWS_CHECK(FindNextChangeNotification(context->directoryHandleOnSubtreeChangeLastWrite[type]));
        return true;
        break;}
    case WAIT_TIMEOUT:{
        break;}
    default:{
        korl_logLastError("WaitForSingleObject failed; result=%lu", resultWaitForObject);
        break;}
    }
    return false;
}
#undef _LOCAL_STRING_POOL_POINTER// keep this at the end of file (after all code)!
