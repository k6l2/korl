///@TODO: prepend L"\\?\" to all calls to CreateFile & CreateDirectory to extend file path limit to 32,767 characters
///@TODO: assert that all the Korl_File_PathType variables in this file are valid values (sanity check)
#include "korl-file.h"
#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-memory.h"
#include "korl-time.h"
#include "korl-windows-window.h"
#include "korl-assetCache.h"
#include "korl-stringPool.h"
#include "korl-checkCast.h"
#include <minidumpapiset.h>
#ifdef _LOCAL_STRING_POOL_POINTER
#undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_file_context.stringPool))
korl_global_const wchar_t _KORL_FILE_DIRECTORY_SAVE_STATES[] = L"save-states";///@TODO: delete this probably
korl_global_const i8 _KORL_SAVESTATE_UNIQUE_FILE_ID[] = "KORL-SAVESTATE";
korl_global_const u32 _KORL_SAVESTATE_VERSION = 0;
typedef struct _Korl_File_SaveStateEnumerateContext
{
    void* saveStateBuffer;
    u$ saveStateBufferBytes;
    u$ saveStateBufferBytesUsed;
    u8 allocatorCount;
    u32 allocationCounts[64];//KORL-FEATURE-000-000-007: dynamic resizing arrays, similar to stb_ds
    u8 currentAllocator;
} _Korl_File_SaveStateEnumerateContext;
typedef struct _Korl_File_AsyncOpertion
{
    const void* writeBuffer;// a non-NULL value indicates this AsyncOperation object is occupied
    HANDLE handleFile;
    OVERLAPPED overlapped;
    u16 salt;
    bool apcHasBeenCalled;
} _Korl_File_AsyncOpertion;
typedef struct _Korl_File_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool stringPool;
    Korl_StringPool_StringHandle directoryStrings[KORL_FILE_PATHTYPE_ENUM_COUNT];
    /** We cannot use a KORL_MEMORY_POOL here, because we need the memory 
     * locations of used OVERLAPPED structs to remain completely STATIC once in 
     * use by Windows.  
     * The indices into this pool should be considered as "handles" to users of 
     * this code module.
     * Individual elements of this pool are considered to be "unused" if the 
     * \c writeBuffer member == \c NULL .  */
    _Korl_File_AsyncOpertion asyncPool[64];
    _Korl_File_SaveStateEnumerateContext saveStateEnumContext;
} _Korl_File_Context;
korl_global_variable _Korl_File_Context _korl_file_context;
korl_internal bool _korl_file_open(Korl_File_PathType pathType, 
                                   const wchar_t* fileName, 
                                   Korl_File_Descriptor_Flags flags, 
                                   Korl_File_Descriptor* o_fileDescriptor)
{
    _Korl_File_Context*const context = &_korl_file_context;
    bool result = true;
    Korl_StringPool_StringHandle stringFilePath = string_copy(context->directoryStrings[pathType]);
    string_appendUtf16(stringFilePath, L"/");
    string_appendUtf16(stringFilePath, fileName);
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
    const HANDLE hFile = CreateFile(string_getRawUtf16(stringFilePath), 
                                    createDesiredAccess, 
                                    FILE_SHARE_READ, 
                                    NULL/*default security*/, 
                                    OPEN_ALWAYS, 
                                    createFileFlags, 
                                    NULL/*no template file*/);
    if(hFile == INVALID_HANDLE_VALUE)
    {
        korl_log(WARNING, "CreateFile('%ws') failed; GetLastError==0x%X", 
                 string_getRawUtf16(stringFilePath), GetLastError());
        result = false;
        goto cleanUp;
    }
    o_fileDescriptor->handle = hFile;
    o_fileDescriptor->flags  = flags;
cleanUp:
    string_free(stringFilePath);
    return true;
}
korl_internal void _korl_file_LpoverlappedCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(dwErrorCode == ERROR_SUCCESS);
    _Korl_File_AsyncOpertion* pAsyncOp = NULL;
    for(u$ i = 0; i < korl_arraySize(context->asyncPool); i++)
        if(lpOverlapped == &context->asyncPool[i].overlapped)
        {
            pAsyncOp = &context->asyncPool[i];
            break;
        }
    korl_assert(pAsyncOp);
    korl_assert(pAsyncOp->writeBuffer);
    pAsyncOp->apcHasBeenCalled = true;
}
korl_internal void _korl_file_LpoverlappedCompletionRoutine_noAsyncOp(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    korl_assert(dwErrorCode == ERROR_SUCCESS);
}
/** The caller is responsible for freeing the string returned via 
 * the \c o_filePathOldest parameter.
 * \return the # of matched files */
korl_internal u$ _korl_file_findOldestFile(Korl_StringPool_StringHandle findFileDirectory, const wchar_t* findFileNamePattern, 
                                           Korl_StringPool_StringHandle* o_filePathOldest)
{
    u$ findFileCount = 0;
    *o_filePathOldest = 0;
    Korl_StringPool_StringHandle findFilePattern = string_copy(findFileDirectory);
    string_appendUtf16(findFilePattern, L"\\");
    string_appendUtf16(findFilePattern, findFileNamePattern);
    WIN32_FIND_DATA findFileData;
    HANDLE findHandle = FindFirstFile(string_getRawUtf16(findFilePattern), &findFileData);
    if(findHandle == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() != ERROR_FILE_NOT_FOUND)
            korl_logLastError("FindFirstFile(%ws) failed", string_getRawUtf16(findFilePattern));
        goto cleanUp;
    }
    findFileCount = 1;
    FILETIME creationTimeOldest = findFileData.ftCreationTime;
    *o_filePathOldest = string_copy(findFileDirectory);
    string_appendUtf16(*o_filePathOldest, L"\\");
    string_appendUtf16(*o_filePathOldest, findFileData.cFileName);
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
            string_free(*o_filePathOldest);
            *o_filePathOldest = string_copy(findFileDirectory);
            string_appendUtf16(*o_filePathOldest, L"\\");
            string_appendUtf16(*o_filePathOldest, findFileData.cFileName);
        }
    }
    if(!FindClose(findHandle))
        korl_logLastError("FindClose failed");
cleanUp:
    string_free(findFilePattern);
    return findFileCount;
}
korl_internal void _korl_file_copyFiles(Korl_StringPool_StringHandle sourceDirectory, const wchar_t* sourcePattern, Korl_StringPool_StringHandle destinationDirectory)
{
    Korl_StringPool_StringHandle findFilePattern = string_copy(sourceDirectory);
    string_appendUtf16(findFilePattern, L"\\");
    string_appendUtf16(findFilePattern, sourcePattern);
    WIN32_FIND_DATA findFileData;
    HANDLE findHandle = FindFirstFile(string_getRawUtf16(findFilePattern), &findFileData);
    if(findHandle == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() != ERROR_FILE_NOT_FOUND)
            korl_logLastError("FindFirstFile(%ws) failed", string_getRawUtf16(findFilePattern));
        goto cleanUp;
    }
    for(;;)
    {
        Korl_StringPool_StringHandle pathSource = string_copy(sourceDirectory);
        string_appendUtf16(pathSource, L"\\");
        string_appendUtf16(pathSource, findFileData.cFileName);
        Korl_StringPool_StringHandle pathDestination = string_copy(destinationDirectory);
        string_appendUtf16(pathDestination, L"\\");
        string_appendUtf16(pathDestination, findFileData.cFileName);
        if(!CopyFile(string_getRawUtf16(pathSource), string_getRawUtf16(pathDestination), FALSE))
            korl_log(WARNING, "CopyFile(%ws, %ws) failed!  GetLastError=0x%X", 
                     string_getRawUtf16(pathSource), string_getRawUtf16(pathDestination), GetLastError());
        else
            korl_log(INFO, "copied file \"%ws\" to \"%ws\"", 
                     string_getRawUtf16(pathSource), string_getRawUtf16(pathDestination));
        string_free(pathSource);
        string_free(pathDestination);
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
    string_free(findFilePattern);
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_file_saveStateCreate_allocationEnumCallback)
{
    _Korl_File_Context*const context                       = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    /* copy the allocation meta data & memory to the save state buffer */
    const u16 fileCharacterCount = korl_checkCast_u$_to_u16(korl_memory_stringSize(meta->file));
    const u$ allocationBytesRequired = sizeof(fileCharacterCount) 
                                     + fileCharacterCount*sizeof(*meta->file) 
                                     + sizeof(meta->line) 
                                     + sizeof(allocation) 
                                     + sizeof(meta->bytes) 
                                     + meta->bytes;
    korl_assert(sizeof(allocation) == sizeof(u64));
    u8* bufferCursor    = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytesUsed;
    const u8* bufferEnd = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytes;
    if(bufferCursor + allocationBytesRequired > bufferEnd)
    {
        enumContext->saveStateBufferBytes = KORL_MATH_MAX(2*enumContext->saveStateBufferBytes, 
                                                          // at _least_ make sure that we are about to realloc enough room for the required bytes for this allocation:
                                                          enumContext->saveStateBufferBytes + allocationBytesRequired);
        enumContext->saveStateBuffer = korl_reallocate(context->allocatorHandle, enumContext->saveStateBuffer, enumContext->saveStateBufferBytes);
        korl_assert(enumContext->saveStateBuffer);
        bufferCursor = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytesUsed;
        bufferEnd    = bufferCursor + enumContext->saveStateBufferBytes;
    }
    korl_assert(sizeof(fileCharacterCount)             == korl_memory_packU16(fileCharacterCount, &bufferCursor, bufferEnd));
    korl_assert(fileCharacterCount*sizeof(*meta->file) == korl_memory_packStringU16(meta->file, fileCharacterCount, &bufferCursor, bufferEnd));
    korl_assert(sizeof(meta->line)                     == korl_memory_packI32(meta->line, &bufferCursor, bufferEnd));
    korl_assert(sizeof(allocation)                     == korl_memory_packU$(KORL_C_CAST(u$, allocation), &bufferCursor, bufferEnd));
    korl_assert(sizeof(meta->bytes)                    == korl_memory_packU64(meta->bytes, &bufferCursor, bufferEnd));
    korl_assert(meta->bytes                            == korl_memory_packStringI8(allocation, meta->bytes, &bufferCursor, bufferEnd));
    /* book-keeping */
    enumContext->saveStateBufferBytesUsed += allocationBytesRequired;
    enumContext->allocationCounts[enumContext->allocatorCount - 1]++;// accumulate the total # of allocations per allocator
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_file_saveStateCreate_allocatorEnumCallback_allocationPass)
{
    _Korl_File_Context*const context                       = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
        return;
    enumContext->allocatorCount++;
    korl_assert(enumContext->allocatorCount <= korl_arraySize(enumContext->allocationCounts));
    enumContext->allocationCounts[enumContext->allocatorCount - 1] = 0;
    /* enumarate over all allocations & write those to the save state buffer */
    korl_memory_allocator_enumerateAllocations(opaqueAllocator, allocatorUserData, _korl_file_saveStateCreate_allocationEnumCallback, enumContext, NULL/*don't care about allocatorVirtualAddressEnd*/);
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_file_saveStateCreate_allocatorEnumCallback_allocatorPass)
{
    _Korl_File_Context*const context                       = &_korl_file_context;
    _Korl_File_SaveStateEnumerateContext*const enumContext = KORL_C_CAST(_Korl_File_SaveStateEnumerateContext*, userData);
    if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
        return;
    /* now we can write the allocator descriptors to the save state buffer */
    const u16 nameCharacterCount = korl_checkCast_u$_to_u16(korl_memory_stringSize(allocatorName));
    const u$ allocatorBytesRequired = sizeof(nameCharacterCount) 
                                    + nameCharacterCount*sizeof(*allocatorName) 
                                    + sizeof(*enumContext->allocationCounts) 
                                    + sizeof(allocatorUserData);
    korl_assert(sizeof(allocatorUserData) == sizeof(u64));
    u8* bufferCursor    = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytesUsed;
    const u8* bufferEnd = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytes;
    if(bufferCursor + allocatorBytesRequired > bufferEnd)
    {
        enumContext->saveStateBufferBytes = KORL_MATH_MAX(2*enumContext->saveStateBufferBytes, 
                                                          // at _least_ make sure that we are about to realloc enough room for the required bytes for this allocator descriptor:
                                                          enumContext->saveStateBufferBytes + allocatorBytesRequired);
        enumContext->saveStateBuffer = korl_reallocate(context->allocatorHandle, enumContext->saveStateBuffer, enumContext->saveStateBufferBytes);
        korl_assert(enumContext->saveStateBuffer);
        bufferCursor = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytesUsed;
        bufferEnd    = bufferCursor + enumContext->saveStateBufferBytes;
    }
    korl_assert(sizeof(nameCharacterCount)                == korl_memory_packU16(nameCharacterCount, &bufferCursor, bufferEnd));
    korl_assert(nameCharacterCount*sizeof(*allocatorName) == korl_memory_packStringU16(allocatorName, nameCharacterCount, &bufferCursor, bufferEnd));
    korl_assert(sizeof(*enumContext->allocationCounts)    == korl_memory_packU32(enumContext->allocationCounts[enumContext->currentAllocator], &bufferCursor, bufferEnd));
    korl_assert(sizeof(allocatorUserData)                 == korl_memory_packU$(KORL_C_CAST(u$, allocatorUserData), &bufferCursor, bufferEnd));
    enumContext->saveStateBufferBytesUsed += allocatorBytesRequired;
    enumContext->currentAllocator++;
}
korl_internal void korl_file_initialize(void)
{
    //KORL-ISSUE-000-000-057: file: initialization occurs before korl_crash_initialize, despite calling korl_assert & korl_log(ERROR)
    _Korl_File_Context*const context = &_korl_file_context;
    korl_memory_zero(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_megabytes(16), L"korl-file", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL/*let platform choose address*/);
    context->stringPool      = korl_stringPool_create(context->allocatorHandle);
    /* determine where the current working directory is in Windows */
    const DWORD currentDirectoryCharacters = GetCurrentDirectory(0, NULL);// _including_ the null-terminator
    if(currentDirectoryCharacters == 0)
        korl_logLastError("GetCurrentDirectory failed!");
    context->directoryStrings[KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY] = string_newEmptyUtf16(currentDirectoryCharacters - 1/*ignore the null-terminator; string pool will add one internally*/);
    const DWORD currentDirectoryCharacters2 = 
        GetCurrentDirectory(currentDirectoryCharacters, 
                            string_getRawWriteableUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY]));
    if(currentDirectoryCharacters2 == 0)
        korl_logLastError("GetCurrentDirectory failed!");
    korl_log(INFO, "Current working directory: %ws", 
             string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY]));
    /* determine where the application's local storage is in Windows */
    {
        wchar_t* pathLocalAppData;
        const HRESULT resultGetKnownFolderPath = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 
                                                                      KF_FLAG_CREATE | KF_FLAG_INIT, 
                                                                      NULL/*token of current user*/,
                                                                      &pathLocalAppData);
        korl_assert(resultGetKnownFolderPath == S_OK);
        context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA] = string_newUtf16(pathLocalAppData);
        string_appendUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA], L"\\");
        string_appendUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA], KORL_APPLICATION_NAME);
        CoTaskMemFree(pathLocalAppData);
        korl_log(INFO, "directoryLocalData=%ws", string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]));
    }
    if(!CreateDirectory(string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]), NULL))
    {
        const int errorCode = GetLastError();
        switch(errorCode)
        {
        case ERROR_ALREADY_EXISTS:{
            korl_log(INFO, "directory '%ws' already exists", 
                     string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]));
            break;}
        default:{
            korl_logLastError("CreateDirectory('%ws') failed", 
                              string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_LOCAL_DATA]));
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
        string_appendUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA], KORL_APPLICATION_NAME);
    }
    korl_log(INFO, "directoryTemporaryData=%ws", 
             string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]));
    if(!CreateDirectory(string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]), NULL))
    {
        const int errorCode = GetLastError();
        switch(errorCode)
        {
        case ERROR_ALREADY_EXISTS:{
            korl_log(INFO, "directory '%ws' already exists", 
                     string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]));
            break;}
        default:{
            korl_logLastError("CreateDirectory('%ws') failed", 
                              string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_TEMPORARY_DATA]));
            break;}
        }
    }
    /* executable file directory */
    context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY] = string_newEmptyUtf16(MAX_PATH);// just an initial guess; you'll see why shortly...
    DWORD resultGetModuleFileName = string_getRawSizeUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]);
    // That's right, in order to properly use this shitty API, Microsoft forces 
    //  us to pull this reallocate loop bullshit... 🤡 //
    for(;;)
    {
        resultGetModuleFileName = GetModuleFileName(NULL/*executable file of current process*/, 
                                                    string_getRawWriteableUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]), 
                                                    string_getRawSizeUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]));
        if(resultGetModuleFileName == 0)
            korl_logLastError("GetModuleFileName failed!");
        if(resultGetModuleFileName >= string_getRawSizeUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]))
            string_reserveUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY], 
                                2*string_getRawSizeUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]));
        else
            break;
    }
    // null-terminate the module file path at the last backslash, so we 
    //  effectively get the parent directory of the exe file //
    const wchar_t* lastBackslash = string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]);
    for(const wchar_t* c = string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]); *c; c++)
        if(*c == '\\')
            lastBackslash = c;
    string_reserveUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY], 
                        korl_checkCast_i$_to_u32(lastBackslash - string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY])));
    //  //
    korl_log(INFO, "directoryExecutable=%ws", 
             string_getRawUtf16(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]));
    /* persistent storage of a save-state memory buffer */
    context->saveStateEnumContext.saveStateBufferBytes = korl_math_kilobytes(16);
    context->saveStateEnumContext.saveStateBuffer      = korl_allocate(context->allocatorHandle, context->saveStateEnumContext.saveStateBufferBytes);
    korl_assert(context->saveStateEnumContext.saveStateBuffer);
}
korl_internal bool korl_file_openAsync(Korl_File_PathType pathType, 
                                       const wchar_t* fileName, 
                                       Korl_File_Descriptor* o_fileDescriptor)
{
    return _korl_file_open(pathType, fileName, 
                           KORL_FILE_DESCRIPTOR_FLAG_ASYNC 
                           | KORL_FILE_DESCRIPTOR_FLAG_READ 
                           | KORL_FILE_DESCRIPTOR_FLAG_WRITE, 
                           o_fileDescriptor);
}
korl_internal void korl_file_close(Korl_File_Descriptor* fileDescriptor)
{
    if(!CloseHandle(fileDescriptor->handle))
        korl_logLastError("CloseHandle failed!");
    korl_memory_zero(fileDescriptor, sizeof(*fileDescriptor));
}
korl_internal void korl_file_renameReplace(Korl_File_PathType pathTypeFileName,    const wchar_t* fileName, 
                                           Korl_File_PathType pathTypeFileNameNew, const wchar_t* fileNameNew)
{
    _Korl_File_Context*const context = &_korl_file_context;
    Korl_StringPool_StringHandle filePathNew = 0;
    Korl_StringPool_StringHandle filePath = string_copy(context->directoryStrings[pathTypeFileName]);
    string_appendUtf16(filePath, L"\\");
    string_appendUtf16(filePath, fileName);
    if(GetFileAttributes(string_getRawUtf16(filePath)) == INVALID_FILE_ATTRIBUTES)
    {
        korl_log(INFO, "file '%ws' doesn't exist", string_getRawUtf16(filePath));
        goto cleanUp;
    }
    filePathNew = string_copy(context->directoryStrings[pathTypeFileNameNew]);
    string_appendUtf16(filePathNew, L"\\");
    string_appendUtf16(filePathNew, fileNameNew);
    if(GetFileAttributes(string_getRawUtf16(filePathNew)) != INVALID_FILE_ATTRIBUTES)
        if(DeleteFile(string_getRawUtf16(filePathNew)))
            korl_log(INFO, "file '%ws' deleted", string_getRawUtf16(filePathNew));
        else
        {
            korl_logLastError("DeleteFile('%ws') failed", 
                              string_getRawUtf16(filePathNew));
            goto cleanUp;
        }
    if(!MoveFile(string_getRawUtf16(filePath), 
                 string_getRawUtf16(filePathNew)))
        korl_logLastError("MoveFile('%ws', '%ws') failed", 
                          string_getRawUtf16(filePath), string_getRawUtf16(filePathNew));
cleanUp:
    string_free(filePath);
    string_free(filePathNew);
}
korl_internal Korl_File_AsyncWriteHandle korl_file_writeAsync(Korl_File_Descriptor fileDescriptor, const void* buffer, u$ bufferBytes)
{
    _Korl_File_Context*const context = &_korl_file_context;
    /* find an unused OVERLAPPED pool slot */
    u$ i = 0;
    for(; i < korl_arraySize(context->asyncPool); i++)
        if(NULL == context->asyncPool[i].writeBuffer)
            break;
    korl_assert(i < korl_arraySize(context->asyncPool));
    _Korl_File_AsyncOpertion*const pAsyncOp = &(context->asyncPool[i]);
    const u16 salt = pAsyncOp->salt;// maintain the previous salt value
    korl_memory_zero(pAsyncOp, sizeof(*pAsyncOp));
    pAsyncOp->salt                  = salt;
    pAsyncOp->writeBuffer           = buffer;
    pAsyncOp->handleFile            = fileDescriptor.handle;
    pAsyncOp->overlapped.Offset     = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
    pAsyncOp->overlapped.OffsetHigh = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
    /* dispatch the async write command */
    const BOOL resultWriteFileEx = WriteFileEx(fileDescriptor.handle, 
                                               buffer, korl_checkCast_u$_to_u32(bufferBytes), 
                                               &(pAsyncOp->overlapped), _korl_file_LpoverlappedCompletionRoutine);
    korl_assert(resultWriteFileEx);
    /* return a handle to the async operation (OVERLAPPED slot) to the user */
    return (Korl_File_AsyncWriteHandle){.components={ .salt  = context->asyncPool[i].salt
                                                    , .index = korl_checkCast_u$_to_u16(i) }};
}
korl_internal bool korl_file_writeAsyncWait(Korl_File_AsyncWriteHandle* handle, u32 timeoutMilliseconds)
{
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(handle->components.index < korl_arraySize(context->asyncPool));
    _Korl_File_AsyncOpertion*const pAsyncOp = &context->asyncPool[handle->components.index];
    if(pAsyncOp->salt != handle->components.salt)
    {
        // invalidate the caller's async operation handle //
        handle->components.index = KORL_U16_MAX;
        return true;
    }
    DWORD numberOfBytesTransferred = 0;
    DWORD dwTimeoutMilliseconds = timeoutMilliseconds;
    if(timeoutMilliseconds == KORL_U32_MAX)
        /* at first glance, this is generally going to be a completely useless 
            transformation, but I'm putting this here on the remote off chance 
            that these constants and/or types (DWORD, INFINITE) ever change from 
            being unsigned 32-bit for some reason, which at least will likely 
            give a compile-time warning/error. */
        dwTimeoutMilliseconds = INFINITE;
    const BOOL resultGetOverlappedResult = 
        GetOverlappedResultEx(pAsyncOp->handleFile, &(pAsyncOp->overlapped), 
                              &numberOfBytesTransferred, dwTimeoutMilliseconds, 
                              TRUE/*calls the I/O completion routine IF dwTimeoutMilliseconds != 0*/);
    if(!resultGetOverlappedResult)
    {
        const DWORD errorCode = GetLastError();
        switch(errorCode)
        {
        case WAIT_IO_COMPLETION:{// the async operation is complete, and the completion routine has been queued
            goto checkTaskCompletion;}
        case ERROR_IO_INCOMPLETE:// operation is still in progress, timeoutMilliseconds was == 0
        case WAIT_TIMEOUT:// operation is still in progress, timeoutMilliseconds was > 0
            return false;
        }
        /* if execution ever reaches here, we haven't accounted for some other 
            failure condition */
        korl_logLastError("GetOverlappedResultEx failed!");
    }
checkTaskCompletion:
    if(!pAsyncOp->apcHasBeenCalled)
        /* The "Asynchronous Procedure Call" will never be called until the 
            thread which started the overlapped I/O operation enters an 
            alertable wait state, and experimentally GetOverlappedResultEx does 
            NOT actually call the APCs even if bAlertable is set to TRUE.  Ergo, 
            we have no choice but to use some other API to do this, and SleepEx 
            is apparently able to achieve this behavior.  We need to do this 
            because the lifetime of the OVERLAPPED object used for the async 
            operation _must_ remain valid for the entire operation & APC call */
        if(dwTimeoutMilliseconds == 0)
            /* attempt to call the APC by entering an "alertable wait state" */
            SleepEx(0/*milliseconds*/, TRUE/*enter alertable wait state*/);
    if(pAsyncOp->apcHasBeenCalled)
    {
        // clean up the async operation slot //
        pAsyncOp->writeBuffer = NULL;
        pAsyncOp->salt++;
        // invalidate the caller's async operation handle //
        handle->components.index = KORL_U16_MAX;
        return true;
    }
    return false;
}
korl_internal void korl_file_write(Korl_File_Descriptor fileDescriptor, const void* data, u$ dataBytes)
{
    DWORD bytesWritten = 0;
    if(fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_ASYNC)
    {
        KORL_ZERO_STACK(OVERLAPPED, overlapped);
        overlapped.Offset     = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
        overlapped.OffsetHigh = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
        korl_assert(WriteFileEx(fileDescriptor.handle, data, korl_checkCast_u$_to_u32(dataBytes), 
                                &overlapped, _korl_file_LpoverlappedCompletionRoutine_noAsyncOp));
        korl_assert(GetOverlappedResultEx(fileDescriptor.handle, &overlapped, 
                                          &bytesWritten, INFINITE, 
                                          FALSE/*we don't care about calling the I/O completion function*/));
        goto done_checkBytesWritten;
    }
    if(!WriteFile(fileDescriptor.handle, data, korl_checkCast_u$_to_u32(dataBytes), &bytesWritten, NULL/*no overlapped*/))
        korl_logLastError("WriteFile failed!");
done_checkBytesWritten:
    korl_assert(bytesWritten == korl_checkCast_u$_to_u32(dataBytes));
}
korl_internal bool korl_file_load(
    const wchar_t*const fileName, Korl_File_PathType pathType, 
    Korl_Memory_AllocatorHandle allocatorHandle, 
    void** out_data, u32* out_dataBytes)
{
    _Korl_File_Context*const context = &_korl_file_context;
    Korl_StringPool_StringHandle filePath = string_copy(context->directoryStrings[pathType]);
    string_appendUtf16(filePath, L"\\");
    string_appendUtf16(filePath, fileName);
    const HANDLE hFile = 
        CreateFileW(string_getRawUtf16(filePath), 
                    GENERIC_READ, FILE_SHARE_READ, NULL/*default security*/, 
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL/*no template file*/);
    if(hFile == INVALID_HANDLE_VALUE)
        korl_logLastError("CreateFileW failed! filePath=%ws", 
                          string_getRawUtf16(filePath));
    const DWORD fileSize = GetFileSize(hFile, NULL/*high file size DWORD*/);
    if(fileSize == INVALID_FILE_SIZE)
        korl_logLastError("GetFileSize failed!");
    void*const allocation = korl_allocate(allocatorHandle, fileSize);
    korl_assert(allocation);
    DWORD bytesRead = 0;
    if(!ReadFile(hFile, allocation, fileSize, &bytesRead, NULL/*lpOverlapped*/))
        korl_logLastError("ReadFile failed!");
    korl_assert(bytesRead == fileSize);
    korl_assert(CloseHandle(hFile));
    *out_dataBytes = fileSize;
    *out_data      = allocation;
    string_free(filePath);
    return true;
}
korl_internal void korl_file_generateMemoryDump(void* exceptionData, Korl_File_PathType type, u32 maxDumpCount)
{
    _Korl_File_Context*const context = &_korl_file_context;
    Korl_StringPool_StringHandle pathFileRead        = 0;
    Korl_StringPool_StringHandle pathFileWrite       = 0;
    Korl_StringPool_StringHandle dumpDirectory       = 0;
    Korl_StringPool_StringHandle directoryMemoryDump = 0;
    // derived from MSDN sample code:
    //	https://docs.microsoft.com/en-us/windows/win32/dxtecharts/crash-dump-analysis
    SYSTEMTIME localTime;
    GetLocalTime( &localTime );
    /* create a memory dump directory in the temporary data directory */
    dumpDirectory = string_copy(context->directoryStrings[type]);
    string_appendUtf16(dumpDirectory, L"\\memory-dumps");
    if(!CreateDirectory(string_getRawUtf16(dumpDirectory), NULL/*default security*/))
        switch(GetLastError())
        {
        case ERROR_ALREADY_EXISTS:
            break;
        case ERROR_PATH_NOT_FOUND:
            korl_log(ERROR, "CreateDirectory(%ws) failed: path not found", string_getRawUtf16(dumpDirectory));
            goto cleanUp;
        }
    /* delete the oldest dump folder after we reach some maximum dump count */
    {
        Korl_StringPool_StringHandle filePathOldest = 0;
        korl_assert(maxDumpCount > 0);
        while(maxDumpCount <= _korl_file_findOldestFile(dumpDirectory, L"*-*-*-*-*", &filePathOldest))
        {
            /* apparently pFrom needs to be double null-terminated */
            const u32 filePathOldestSize = string_getRawSizeUtf16(filePathOldest);
            string_reserveUtf16(filePathOldest, filePathOldestSize + 1);
            string_getRawWriteableUtf16(filePathOldest)[filePathOldestSize] = L'\0';
            korl_log(INFO, "deleting oldest dump folder: %ws", string_getRawUtf16(filePathOldest));
            KORL_ZERO_STACK(SHFILEOPSTRUCT, fileOpStruct);
            fileOpStruct.wFunc  = FO_DELETE;
            fileOpStruct.pFrom  = string_getRawUtf16(filePathOldest);
            fileOpStruct.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;
            const int resultFileOpDeleteRecursive = SHFileOperation(&fileOpStruct);
            if(resultFileOpDeleteRecursive != 0)
                korl_log(WARNING, "recursive delete of \"%ws\" failed; result=%i", 
                         string_getRawUtf16(filePathOldest), resultFileOpDeleteRecursive);
            string_free(filePathOldest);
        }
        string_free(filePathOldest);
    }
    // Create a companion folder to store PDB files specifically for this dump! //
    directoryMemoryDump = string_copy(dumpDirectory);
    string_appendFormatUtf16(directoryMemoryDump, L"\\%ws-%04d%02d%02d-%02d%02d%02d-%ld-%ld", 
                             KORL_APPLICATION_VERSION,
                             localTime.wYear, localTime.wMonth, localTime.wDay,
                             localTime.wHour, localTime.wMinute, localTime.wSecond,
                             GetCurrentProcessId(), GetCurrentThreadId());
    if(!CreateDirectory(string_getRawUtf16(directoryMemoryDump), NULL))
    {
        korl_logLastError("CreateDirectory(%ws) failed!", 
                          string_getRawUtf16(directoryMemoryDump));
        goto cleanUp;
    }
    else
        korl_log(INFO, "created MiniDump directory: %ws", 
                 string_getRawUtf16(directoryMemoryDump));
    // Create the mini dump! //
    pathFileWrite = string_copy(directoryMemoryDump);
    string_appendFormatUtf16(pathFileWrite, L"\\%ws-%04d%02d%02d-%02d%02d%02d-0x%X-0x%X.dmp", 
                             KORL_APPLICATION_VERSION,
                             localTime.wYear, localTime.wMonth, localTime.wDay,
                             localTime.wHour, localTime.wMinute, localTime.wSecond,
                             GetCurrentProcessId(), GetCurrentThreadId());
    const HANDLE hDumpFile = CreateFile(string_getRawUtf16(pathFileWrite), 
                                        GENERIC_READ|GENERIC_WRITE, 
                                        FILE_SHARE_WRITE|FILE_SHARE_READ, 
                                        0, CREATE_ALWAYS, 0, 0);
    if(INVALID_HANDLE_VALUE == hDumpFile)
    {
        korl_logLastError("CreateFile(%ws) failed!", string_getRawUtf16(pathFileWrite));
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
    korl_log(INFO, "MiniDump written to: %ws", string_getRawUtf16(pathFileWrite));
    /* Attempt to copy the win32 application's symbol files to the dump 
        location.  This will allow us to at LEAST view the call stack properly 
        during development for all of our minidumps even after making source 
        changes and subsequently rebuilding. */
    // Copy the executable //
    pathFileRead = string_copy(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY]);
    string_appendUtf16(pathFileRead, L"\\");
    string_appendUtf16(pathFileRead, KORL_APPLICATION_NAME);
    string_appendUtf16(pathFileRead, L".exe");
    string_free(pathFileWrite);
    pathFileWrite = string_copy(directoryMemoryDump);
    string_appendUtf16(pathFileWrite, L"\\");
    string_appendUtf16(pathFileWrite, KORL_APPLICATION_NAME);
    string_appendUtf16(pathFileWrite, L".exe");
    if(!CopyFile(string_getRawUtf16(pathFileRead), string_getRawUtf16(pathFileWrite), FALSE))
        korl_log(WARNING, "CopyFile(%ws, %ws) failed!  GetLastError=0x%X", 
                 string_getRawUtf16(pathFileRead), string_getRawUtf16(pathFileWrite), GetLastError());
    else
        korl_log(INFO, "copied file \"%ws\" to \"%ws\"", string_getRawUtf16(pathFileRead), string_getRawUtf16(pathFileWrite));
    // Copy all PDB files //
    _korl_file_copyFiles(context->directoryStrings[KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY], L"*.pdb", directoryMemoryDump);
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
    string_free(pathFileRead);
    string_free(pathFileWrite);
    string_free(dumpDirectory);
    string_free(directoryMemoryDump);
}
/** In this function, we just write out each allocator to the file.  Instead of 
 * enumerating over each memory allocator and writing the contents to the file 
 * directly, we will copy all the desired contents of the save state file to a 
 * buffer, and just do a single file write operation at the end, or perhaps in 
 * another API.
 * 
 * ----- Save State File Format -----
 * [Allocation Descriptor] * Σ[Allocator Descriptor].allocationCount
 * - fileCharacterCount: u16
 * - file              : wchar_t[fileCharacterCount]
 * - line              : i32
 * - address           : u$
 * - bytes             : u$
 * - data              : u8[bytes]
 * [Allocator Descriptor] * [Header].allocatorCount
 * - nameCharacterCount      : u16
 * - name                    : wchar_t[nameCharacterCount]
 * - allocationCount         : u32
 * - allocatorUserDataAddress: u$
 * - type                    : u8
 * - flags                   : u32
 * - maxBytes                : u64
 * [Manifest]
 * - uniqueFileId             : "KORL-SAVESTATE"
 * - version                  : u32
 * - allocatorCount           : u8
 * - allocatorDescriptorOffset: u64 // relative to the beginning of the file
 * ----- End of Save State File Format -----
 * 
 * By placing the allocation descriptors at the top of the file, we can more 
 * easily just write out all the memory to the file buffer by just performing a 
 * single pass of all allocations in all allocators, while simultaneously 
 * calculating how much space will be required for the [Allocator Descriptor] 
 * segments, and thus total final save state file size.  When reading from a 
 * save state to load, we can simply start at the end of the file minus the 
 * known size of the file's [Manifest] segment.
 */
korl_internal void korl_file_saveStateCreate(void)
{
    _Korl_File_Context*const context = &_korl_file_context;
    /* recycle the context's persistent save-state enum context */
    context->saveStateEnumContext.saveStateBufferBytesUsed = 0;
    context->saveStateEnumContext.allocatorCount           = 0;
    context->saveStateEnumContext.currentAllocator         = 0;
    _Korl_File_SaveStateEnumerateContext*const enumContext = &context->saveStateEnumContext;
    /* code module specific data */
    const u64 windowDescriptorByteStart = enumContext->saveStateBufferBytesUsed;
    korl_windows_window_saveStateWrite(context->allocatorHandle, &enumContext->saveStateBuffer, &enumContext->saveStateBufferBytes, &enumContext->saveStateBufferBytesUsed);
    const u64 assetCacheDescriptorByteStart = enumContext->saveStateBufferBytesUsed;
    korl_assetCache_saveStateWrite(context->allocatorHandle, &enumContext->saveStateBuffer, &enumContext->saveStateBufferBytes, &enumContext->saveStateBufferBytesUsed);
    /* begin iteration over memory allocators, copying allocations to the save 
        state buffer, as well as count how many allocations there are per 
        allocator */
    const u64 allocationDescriptorByteStart = enumContext->saveStateBufferBytesUsed;
    korl_memory_allocator_enumerateAllocators(_korl_file_saveStateCreate_allocatorEnumCallback_allocationPass, enumContext);
    /* now that we have copied all the allocation descriptors, we can copy the 
        allocator descriptors */
    enumContext->currentAllocator = 0;
    const u$ allocatorDescriptorByteStart = enumContext->saveStateBufferBytesUsed;
    korl_memory_allocator_enumerateAllocators(_korl_file_saveStateCreate_allocatorEnumCallback_allocatorPass, enumContext);
    /* finally, we can write the save state manifest to the end of the save 
        state buffer */
    const u$ manifestBytesRequired = (sizeof(_KORL_SAVESTATE_UNIQUE_FILE_ID) - 1/*don't care about the '\0'*/) 
                                   + sizeof(_KORL_SAVESTATE_VERSION) 
                                   + sizeof(enumContext->allocatorCount) 
                                   + sizeof(allocatorDescriptorByteStart) 
                                   + sizeof(allocationDescriptorByteStart) 
                                   + sizeof(windowDescriptorByteStart) 
                                   + sizeof(assetCacheDescriptorByteStart);
    u8* bufferCursor    = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytesUsed;
    const u8* bufferEnd = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytes;
    if(bufferCursor + manifestBytesRequired > bufferEnd)
    {
        enumContext->saveStateBufferBytes = KORL_MATH_MAX(2*enumContext->saveStateBufferBytes, 
                                                          // at _least_ make sure that we are about to realloc enough room for the required bytes for the manifest:
                                                          enumContext->saveStateBufferBytes + manifestBytesRequired);
        enumContext->saveStateBuffer = korl_reallocate(context->allocatorHandle, enumContext->saveStateBuffer, enumContext->saveStateBufferBytes);
        korl_assert(enumContext->saveStateBuffer);
        bufferCursor = KORL_C_CAST(u8*, enumContext->saveStateBuffer) + enumContext->saveStateBufferBytesUsed;
        bufferEnd    = bufferCursor + enumContext->saveStateBufferBytes;
    }
    korl_assert((sizeof(_KORL_SAVESTATE_UNIQUE_FILE_ID) - 1) == korl_memory_packStringI8(_KORL_SAVESTATE_UNIQUE_FILE_ID, sizeof(_KORL_SAVESTATE_UNIQUE_FILE_ID) - 1, &bufferCursor, bufferEnd));
    korl_assert(sizeof(_KORL_SAVESTATE_VERSION)              == korl_memory_packU32(_KORL_SAVESTATE_VERSION, &bufferCursor, bufferEnd));
    korl_assert(sizeof(enumContext->allocatorCount)          == korl_memory_packU8(enumContext->allocatorCount, &bufferCursor, bufferEnd));
    korl_assert(sizeof(allocatorDescriptorByteStart)         == korl_memory_packU64(allocatorDescriptorByteStart, &bufferCursor, bufferEnd));
    korl_assert(sizeof(allocationDescriptorByteStart)        == korl_memory_packU64(allocationDescriptorByteStart, &bufferCursor, bufferEnd));
    korl_assert(sizeof(windowDescriptorByteStart)            == korl_memory_packU64(windowDescriptorByteStart, &bufferCursor, bufferEnd));
    korl_assert(sizeof(assetCacheDescriptorByteStart)        == korl_memory_packU64(assetCacheDescriptorByteStart, &bufferCursor, bufferEnd));
    enumContext->saveStateBufferBytesUsed += manifestBytesRequired;
}
///@TODO: move this up to the private API section
korl_internal void _korl_file_createParentDirectoriesRecursive(Korl_File_PathType pathType, const wchar_t* fileName)
{
    korl_assert("@TODO");
}
korl_internal void korl_file_saveStateSave(Korl_File_PathType pathType, const wchar_t* fileName)
{
    _Korl_File_Context*const context = &_korl_file_context;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    /* recursively create all directories implied by the file name withing the given path type */
    ///@TODO:
    /* create a directory to store save states in */
    korl_time_probeStart(write_buffer);
    Korl_StringPool_StringHandle directory = string_copy(context->directoryStrings[pathType]);
    string_appendUtf16(directory, L"\\");
    string_appendUtf16(directory, _KORL_FILE_DIRECTORY_SAVE_STATES);
    if(!CreateDirectory(string_getRawUtf16(directory), NULL/*default security*/))
        switch(GetLastError())
        {
        case ERROR_ALREADY_EXISTS:
            break;
        case ERROR_PATH_NOT_FOUND:
            korl_log(ERROR, "CreateDirectory(%ws) failed: path not found", string_getRawUtf16(directory));
            goto cleanUp;
        }
    /* we can just use fileName directly; maybe in the future we can do rolling 
        save state files or something */
    string_appendUtf16(directory, L"\\");
    string_appendUtf16(directory, fileName);
    /* open the save state file for writing */
    hFile = CreateFile(string_getRawUtf16(directory), GENERIC_WRITE, 
                       FILE_SHARE_WRITE|FILE_SHARE_READ, 
                       0, CREATE_ALWAYS, 0, 0);
    if(hFile == INVALID_HANDLE_VALUE)
    {
        korl_logLastError("CreateFile(%ws) failed!", string_getRawUtf16(directory));
        goto cleanUp;
    }
    /* write the save state to a file */
    DWORD bytesWritten;
    if(!WriteFile(hFile, context->saveStateEnumContext.saveStateBuffer, 
                  korl_checkCast_u$_to_u32(context->saveStateEnumContext.saveStateBufferBytesUsed), 
                  &bytesWritten, NULL/*no overlapped*/))
    {
        korl_logLastError("WriteFile failed!");
        goto cleanUp;
    }
    korl_log(INFO, "Wrote save state file: %ws", string_getRawUtf16(directory));
    korl_time_probeStop(write_buffer);
cleanUp:
    korl_time_probeStart(clean_up);
    string_free(directory);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        korl_time_probeStart(close_file_handle);
        korl_assert(CloseHandle(hFile));
        korl_time_probeStop(close_file_handle);
    }
    korl_time_probeStop(clean_up);
}
korl_internal void korl_file_saveStateLoad(Korl_File_PathType pathType, const wchar_t* fileName)
{
    _Korl_File_Context*const context = &_korl_file_context;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    Korl_StringPool_StringHandle allocationFileBuffer = 0;
    typedef struct _AllocatorDescriptor
    {
        u32 allocationCount;
        Korl_Memory_AllocatorHandle handle;
    } _AllocatorDescriptor;
    _AllocatorDescriptor* allocatorDescriptors = NULL;
    Korl_StringPool_StringHandle pathFile = string_copy(context->directoryStrings[pathType]);
    string_appendUtf16(pathFile, L"\\");
    string_appendUtf16(pathFile, _KORL_FILE_DIRECTORY_SAVE_STATES);
    string_appendUtf16(pathFile, L"\\");
    string_appendUtf16(pathFile, fileName);
    hFile = CreateFile(string_getRawUtf16(pathFile), GENERIC_READ, FILE_SHARE_READ, 
                       0/*default security*/, OPEN_EXISTING, 
                       0/*flags|attributes*/, 0/*no template*/);
    if(hFile == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() == ERROR_FILE_NOT_FOUND)
            korl_log(WARNING, "Save state file not found: %ws", string_getRawUtf16(pathFile));
        else
            korl_logLastError("CreateFile(%ws) failed!", string_getRawUtf16(pathFile));
        goto cleanUp;
    }
    /* wait for async file operations to be complete before changing application state! */
    for(u16 a = 0; a < korl_arraySize(context->asyncPool); a++)
    {
        if(!context->asyncPool[a].writeBuffer)
            continue;
        Korl_File_AsyncWriteHandle asyncHandle;
        asyncHandle.components.index = a;
        asyncHandle.components.salt  = context->asyncPool[a].salt;
        korl_assert(korl_file_writeAsyncWait(&asyncHandle, KORL_U32_MAX));
    }
    /* read & parse & process the save state file */
    /* first, we need to extract the savestate manifest which is placed at the 
        very end of the file */
    u8 allocatorCount;
    u$ allocatorDescriptorByteStart;
    u64 allocationDescriptorByteStart;
    u64 windowDescriptorByteStart;
    u64 assetCacheDescriptorByteStart;
    const u$ manifestBytesRequired = (sizeof(_KORL_SAVESTATE_UNIQUE_FILE_ID) - 1/*don't care about the '\0'*/) 
                                   + sizeof(_KORL_SAVESTATE_VERSION) 
                                   + sizeof(allocatorCount) 
                                   + sizeof(allocatorDescriptorByteStart) 
                                   + sizeof(allocationDescriptorByteStart) 
                                   + sizeof(windowDescriptorByteStart) 
                                   + sizeof(assetCacheDescriptorByteStart);
    LARGE_INTEGER filePointerDistanceToMove;
    filePointerDistanceToMove.QuadPart = -korl_checkCast_u$_to_i$(manifestBytesRequired);
    if(!SetFilePointerEx(hFile, filePointerDistanceToMove, NULL/*new file pointer*/, FILE_END))
    {
        korl_logLastError("SetFilePointerEx failed");
        goto cleanUp;
    }
    i8 bufferUniqueFileId[sizeof(_KORL_SAVESTATE_UNIQUE_FILE_ID)];
    if(!ReadFile(hFile, bufferUniqueFileId, sizeof(_KORL_SAVESTATE_UNIQUE_FILE_ID) - 1, NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        goto cleanUp;
    }
    korl_assert(0 == korl_memory_compare(bufferUniqueFileId, _KORL_SAVESTATE_UNIQUE_FILE_ID, sizeof(_KORL_SAVESTATE_UNIQUE_FILE_ID) - 1));
    u32 version;
    if(!ReadFile(hFile, &version, sizeof(version), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        goto cleanUp;
    }
    korl_assert(version == _KORL_SAVESTATE_VERSION);
    if(!ReadFile(hFile, &allocatorCount, sizeof(allocatorCount), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        goto cleanUp;
    }
    if(!ReadFile(hFile, &allocatorDescriptorByteStart, sizeof(allocatorDescriptorByteStart), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        goto cleanUp;
    }
    if(!ReadFile(hFile, &allocationDescriptorByteStart, sizeof(allocationDescriptorByteStart), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        goto cleanUp;
    }
    if(!ReadFile(hFile, &windowDescriptorByteStart, sizeof(windowDescriptorByteStart), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        goto cleanUp;
    }
    if(!ReadFile(hFile, &assetCacheDescriptorByteStart, sizeof(assetCacheDescriptorByteStart), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        goto cleanUp;
    }
    /* now we can jump to the allocator descriptors and read those in */
    filePointerDistanceToMove.QuadPart = allocatorDescriptorByteStart;
    if(!SetFilePointerEx(hFile, filePointerDistanceToMove, NULL/*new file pointer*/, FILE_BEGIN))
    {
        korl_logLastError("SetFilePointerEx failed");
        goto cleanUp;
    }
    allocatorDescriptors = korl_allocate(context->allocatorHandle, allocatorCount*sizeof(_AllocatorDescriptor));
    korl_assert(allocatorDescriptors);
    wchar_t bufferAllocatorName[32];
    for(u8 a = 0; a < allocatorCount; a++)
    {
        u16 allocatorNameCharacterCount = 0;//_excluding_ the null-terminator!
        if(!ReadFile(hFile, &allocatorNameCharacterCount, sizeof(allocatorNameCharacterCount), NULL/*bytes read*/, NULL/*no overlapped*/))
        {
            korl_logLastError("ReadFile failed");
            goto cleanUp;
        }
        korl_assert(allocatorNameCharacterCount < korl_arraySize(bufferAllocatorName) - 1/*leave 1 char in the buffer for the null terminator*/);
        bufferAllocatorName[allocatorNameCharacterCount] = L'\0';
        if(!ReadFile(hFile, bufferAllocatorName, allocatorNameCharacterCount*sizeof(*bufferAllocatorName), NULL/*bytes read*/, NULL/*no overlapped*/))
        {
            korl_logLastError("ReadFile failed");
            goto cleanUp;
        }
        if(!ReadFile(hFile, &(allocatorDescriptors[a].allocationCount), sizeof(allocatorDescriptors[a].allocationCount), NULL/*bytes read*/, NULL/*no overlapped*/))
        {
            korl_logLastError("ReadFile failed");
            goto cleanUp;
        }
        u64 allocatorAddress;
        if(!ReadFile(hFile, &allocatorAddress, sizeof(allocatorAddress), NULL/*bytes read*/, NULL/*no overlapped*/))
        {
            korl_logLastError("ReadFile failed");
            goto cleanUp;
        }
        if(!korl_memory_allocator_findByName(bufferAllocatorName, &allocatorDescriptors[a].handle))
        {
            korl_log(ERROR, "allocator \"%ws\" not found", bufferAllocatorName);
            goto cleanUp;
        }
        /* This allocation has been identified as being serialized in a 
            savestate, so we must completely empty it before populating it with 
            the saved data.  In addition, we need to recreate it such that the 
            starting virtual address of the allocator matches exactly with what 
            is contained in the save state! */
        korl_memory_allocator_recreate(allocatorDescriptors[a].handle, KORL_C_CAST(void*, allocatorAddress));
    }
    /* cool! so now we have all the information about which allocators need to 
        be repopulated, and how many allocations each one has in the save state; 
        we should now be able to jump back to the beginning of the savestate 
        file and iterate over all the allocations, populating them with the 
        saved allocation data in the savestate file */
    filePointerDistanceToMove.QuadPart = allocationDescriptorByteStart;
    if(!SetFilePointerEx(hFile, filePointerDistanceToMove, NULL/*new file pointer*/, FILE_BEGIN))
    {
        korl_logLastError("SetFilePointerEx failed");
        goto cleanUp;
    }
    u16 allocationFileCharacterCount;
    i32 allocationLine;
    u$ allocationAddress;
    u$ allocationBytes;
    allocationFileBuffer = string_newEmptyUtf16(MAX_PATH/*just a suggestion; likely will change later*/);
    for(u8 a = 0; a < allocatorCount; a++)
    {
        for(u32 aa = 0; aa < allocatorDescriptors[a].allocationCount; aa++)
        {
            if(!ReadFile(hFile, &allocationFileCharacterCount, sizeof(allocationFileCharacterCount), NULL/*bytes read*/, NULL/*no overlapped*/))
            {
                korl_logLastError("ReadFile failed");
                goto cleanUp;
            }
            if(string_getRawSizeUtf16(allocationFileBuffer) <= allocationFileCharacterCount)
                string_reserveUtf16(allocationFileBuffer, allocationFileCharacterCount);
            if(!ReadFile(hFile, string_getRawWriteableUtf16(allocationFileBuffer), allocationFileCharacterCount*sizeof(u16), NULL/*bytes read*/, NULL/*no overlapped*/))
            {
                korl_logLastError("ReadFile failed");
                goto cleanUp;
            }
            if(!ReadFile(hFile, &allocationLine, sizeof(allocationLine), NULL/*bytes read*/, NULL/*no overlapped*/))
            {
                korl_logLastError("ReadFile failed");
                goto cleanUp;
            }
            if(!ReadFile(hFile, &allocationAddress, sizeof(allocationAddress), NULL/*bytes read*/, NULL/*no overlapped*/))
            {
                korl_logLastError("ReadFile failed");
                goto cleanUp;
            }
            if(!ReadFile(hFile, &allocationBytes, sizeof(allocationBytes), NULL/*bytes read*/, NULL/*no overlapped*/))
            {
                korl_logLastError("ReadFile failed");
                goto cleanUp;
            }
            //KORL-ISSUE-000-000-067: load-save-state: allocation file name not supported
            void*const allocation = korl_memory_allocator_allocate(allocatorDescriptors[a].handle, allocationBytes, NULL/*file*/, allocationLine, KORL_C_CAST(void*, allocationAddress));
            if(!ReadFile(hFile, allocation, korl_checkCast_u$_to_u32(allocationBytes), NULL/*bytes read*/, NULL/*no overlapped*/))
            {
                korl_logLastError("ReadFile failed");
                goto cleanUp;
            }
        }
    }
    /* load the window module state */
    filePointerDistanceToMove.QuadPart = windowDescriptorByteStart;
    if(!SetFilePointerEx(hFile, filePointerDistanceToMove, NULL/*new file pointer*/, FILE_BEGIN))
    {
        korl_logLastError("SetFilePointerEx failed");
        goto cleanUp;
    }
    if(!korl_windows_window_saveStateRead(hFile))
    {
        korl_logLastError("korl_windows_window_saveStateRead failed");
        goto cleanUp;
    }
    /* load the assetCache module state */
    filePointerDistanceToMove.QuadPart = assetCacheDescriptorByteStart;
    if(!SetFilePointerEx(hFile, filePointerDistanceToMove, NULL/*new file pointer*/, FILE_BEGIN))
    {
        korl_logLastError("SetFilePointerEx failed");
        goto cleanUp;
    }
    if(!korl_assetCache_saveStateRead(hFile))
    {
        korl_logLastError("korl_assetCache_saveStateRead failed");
        goto cleanUp;
    }
    korl_log(INFO, "save state \"%ws\" loaded successfully!", string_getRawUtf16(pathFile));
cleanUp:
    string_free(pathFile);
    if(allocatorDescriptors)
        korl_free(context->allocatorHandle, allocatorDescriptors);
    if(hFile != INVALID_HANDLE_VALUE)
        korl_assert(CloseHandle(hFile));
}
#undef _LOCAL_STRING_POOL_POINTER// keep this at the end of file (after all code)!
