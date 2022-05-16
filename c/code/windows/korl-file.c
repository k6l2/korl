#include "korl-file.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include <minidumpapiset.h>
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
    wchar_t* directoryLocalData;
    wchar_t  directoryCurrentWorking[MAX_PATH];
    wchar_t  directoryTemporaryData[MAX_PATH];
    wchar_t  directoryExecutable[MAX_PATH];
    DWORD sizeCurrentWorking;/** NOT including the null-terminator! */
    DWORD sizeLocalData;     /** NOT including the null-terminator! */
    DWORD sizeTemporaryData; /** NOT including the null-terminator! */
    DWORD sizeExecutable;    /** NOT including the null-terminator! */
    /** We cannot use a KORL_MEMORY_POOL here, because we need the memory 
     * locations of used OVERLAPPED structs to remain completely STATIC once in 
     * use by Windows.  
     * The indices into this pool should be considered as "handles" to users of 
     * this code module.
     * Individual elements of this pool are considered to be "unused" if the 
     * \c writeBuffer member == \c NULL .  */
    _Korl_File_AsyncOpertion asyncPool[64];
} _Korl_File_Context;
korl_global_variable _Korl_File_Context _korl_file_context;
korl_internal const wchar_t* _korl_file_getPath(Korl_File_PathType type)
{
    switch(type)
    {
    case KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY:{
        return _korl_file_context.directoryCurrentWorking;}
    case KORL_FILE_PATHTYPE_LOCAL_DATA:{
        return _korl_file_context.directoryLocalData;}
    case KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY:{
        return _korl_file_context.directoryExecutable;}
    case KORL_FILE_PATHTYPE_TEMPORARY_DATA:{
        return _korl_file_context.directoryTemporaryData;}
    }
    korl_log(ERROR, "unknown path type: %d", type);
    return NULL;
}
//KORL-ISSUE-000-000-055: file: deprecate this API because it sucks; _korl_file_getPath seems so much better
korl_internal void _korl_file_resolvePath(
    const wchar_t*const fileName, Korl_File_PathType pathType, 
    wchar_t* o_filePath, u$ filePathMaxSize)
{
    _Korl_File_Context*const context = &_korl_file_context;
    const u$ fileNameSize = korl_memory_stringSize(fileName);
    u$ filePathRemainingSize = filePathMaxSize;
    i$ charsCopiedSansTerminator;
    switch(pathType)
    {
    case KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY:{
        /* copy the current working directory string into the filePath */
        charsCopiedSansTerminator = 
            korl_memory_stringCopy(context->directoryCurrentWorking, o_filePath, filePathRemainingSize) - 1;
        korl_assert(charsCopiedSansTerminator == context->sizeCurrentWorking);
        o_filePath            += charsCopiedSansTerminator;
        filePathRemainingSize -= charsCopiedSansTerminator;
        break;}
    case KORL_FILE_PATHTYPE_LOCAL_DATA:{
        /* copy the local application data directory string into the filePath */
        charsCopiedSansTerminator = 
            korl_memory_stringCopy(context->directoryLocalData, o_filePath, filePathRemainingSize) - 1;
        korl_assert(charsCopiedSansTerminator == context->sizeLocalData);
        o_filePath            += charsCopiedSansTerminator;
        filePathRemainingSize -= charsCopiedSansTerminator;
        break;}
    default:
        korl_log(ERROR, "pathType (%i) not implemented!", pathType);
    }
    /* copy a file path separator string into the filePath */
    charsCopiedSansTerminator = 
        korl_memory_stringCopy(L"/", o_filePath, filePathRemainingSize) - 1;
    korl_assert(charsCopiedSansTerminator == 1);
    o_filePath            += charsCopiedSansTerminator;
    filePathRemainingSize -= charsCopiedSansTerminator;
    /* copy the file name into the filePath */
    charsCopiedSansTerminator = 
        korl_memory_stringCopy(fileName, o_filePath, filePathRemainingSize) - 1;
    korl_assert(charsCopiedSansTerminator == korl_checkCast_u$_to_i$(fileNameSize));
    o_filePath            += charsCopiedSansTerminator;
    filePathRemainingSize -= charsCopiedSansTerminator;
}
korl_internal bool _korl_file_open(Korl_File_PathType pathType, 
                                   const wchar_t* fileName, 
                                   Korl_File_Descriptor_Flags flags, 
                                   Korl_File_Descriptor* o_fileDescriptor)
{
    wchar_t filePath[MAX_PATH];
    _korl_file_resolvePath(fileName, pathType, filePath, korl_arraySize(filePath));
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
    const HANDLE hFile = CreateFile(filePath, 
                                    createDesiredAccess, 
                                    FILE_SHARE_READ, 
                                    NULL/*default security*/, 
                                    OPEN_ALWAYS, 
                                    createFileFlags, 
                                    NULL/*no template file*/);
    if(hFile == INVALID_HANDLE_VALUE)
    {
        korl_log(WARNING, "CreateFile('%ws') failed; GetLastError==0x%X", filePath, GetLastError());
        return false;
    }
    o_fileDescriptor->handle = hFile;
    o_fileDescriptor->flags  = flags;
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
/** \return the # of matched files */
korl_internal u$ _korl_file_findOldestFile(const wchar_t* findFileDirectory, const wchar_t* findFileNamePattern, wchar_t* o_filePathOldest, u$ filePathOldestBytes, i$* o_filePathOldestSize)
{
    wchar_t findFilePattern[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(findFilePattern, sizeof(findFilePattern),
                                          L"%ws\\%ws", findFileDirectory, findFileNamePattern))
    {
        korl_logLastError("format findFilePattern failed");
        return 0;
    }
    WIN32_FIND_DATA findFileData;
    HANDLE findHandle = FindFirstFile(findFilePattern, &findFileData);
    if(findHandle == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() != ERROR_FILE_NOT_FOUND)
            korl_logLastError("FindFirstFile(%ws) failed", findFilePattern);
        return 0;
    }
    u$ findFileCount = 1;
    FILETIME creationTimeOldest = findFileData.ftCreationTime;
    *o_filePathOldestSize = 
        korl_memory_stringFormatBuffer(o_filePathOldest, filePathOldestBytes,
                                       L"%ws\\%ws", findFileDirectory, findFileData.cFileName);
    korl_assert(*o_filePathOldestSize > 0);
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
            *o_filePathOldestSize = 
                korl_memory_stringFormatBuffer(o_filePathOldest, filePathOldestBytes,
                                               L"%ws\\%ws", findFileDirectory, findFileData.cFileName);
            korl_assert(*o_filePathOldestSize > 0);
        }
    }
    if(!FindClose(findHandle))
        korl_logLastError("FindClose failed");
    return findFileCount;
}
korl_internal void korl_file_initialize(void)
{
    //KORL-ISSUE-000-000-057: file: initialization occurs before korl_crash_initialize, despite calling korl_assert & korl_log(ERROR)
    _Korl_File_Context*const context = &_korl_file_context;
    korl_memory_zero(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_kilobytes(64), false);
    /* determine where the current working directory is in Windows */
    context->sizeCurrentWorking = 
        GetCurrentDirectory(korl_arraySize(context->directoryCurrentWorking), 
                            context->directoryCurrentWorking);
    if(context->sizeCurrentWorking == 0)
        korl_logLastError("GetCurrentDirectory failed!");
    korl_assert(context->sizeCurrentWorking <= korl_arraySize(context->directoryCurrentWorking));
    korl_log(INFO, "Current working directory: %ws", context->directoryCurrentWorking);
    /* determine where the application's local storage is in Windows */
    {
        wchar_t* pathLocalAppData;
        const HRESULT resultGetKnownFolderPath = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 
                                                                      KF_FLAG_CREATE | KF_FLAG_INIT, 
                                                                      NULL/*token of current user*/,
                                                                      &pathLocalAppData);
        korl_assert(resultGetKnownFolderPath == S_OK);
        context->directoryLocalData = korl_memory_stringFormat(context->allocatorHandle, 
                                                               L"%ws\\%ws", 
                                                               pathLocalAppData, 
                                                               KORL_APPLICATION_NAME);
        CoTaskMemFree(pathLocalAppData);
        korl_log(INFO, "directoryLocalData=%ws", context->directoryLocalData);
        context->sizeLocalData = korl_checkCast_u$_to_u32(korl_memory_stringSize(context->directoryLocalData));
    }
    if(!CreateDirectory(context->directoryLocalData, NULL))
    {
        const int errorCode = GetLastError();
        switch(errorCode)
        {
        case ERROR_ALREADY_EXISTS:{
            korl_log(INFO, "directory '%ws' already exists", context->directoryLocalData);
            break;}
        default:{
            korl_logLastError("CreateDirectory('%ws') failed", context->directoryLocalData);
            break;}
        }
    }
    /* temporary data directory */
    context->sizeTemporaryData = GetTempPath(korl_arraySize(context->directoryTemporaryData), context->directoryTemporaryData);
    if(context->sizeTemporaryData == 0)
        korl_logLastError("GetTempPath failed!");
    {
        i$ charsCopied = 
            korl_memory_stringFormatBuffer(context->directoryTemporaryData + context->sizeTemporaryData, 
                                           korl_arraySize(context->directoryTemporaryData) - context->sizeTemporaryData, 
                                           L"%ws", KORL_APPLICATION_NAME);
        korl_assert(charsCopied > 0);
        context->sizeTemporaryData += korl_checkCast_i$_to_u32(charsCopied);
    }
    korl_log(INFO, "directoryTemporaryData=%ws", context->directoryTemporaryData);
    if(!CreateDirectory(context->directoryTemporaryData, NULL))
    {
        const int errorCode = GetLastError();
        switch(errorCode)
        {
        case ERROR_ALREADY_EXISTS:{
            korl_log(INFO, "directory '%ws' already exists", context->directoryTemporaryData);
            break;}
        default:{
            korl_logLastError("CreateDirectory('%ws') failed", context->directoryTemporaryData);
            break;}
        }
    }
    /* executable file directory */
    context->sizeExecutable = GetModuleFileName(NULL/*executable file of current process*/, 
                                                context->directoryExecutable, 
                                                korl_arraySize(context->directoryExecutable));
    if(context->sizeExecutable == 0)
        korl_logLastError("GetModuleFileName failed!");
    wchar_t* lastBackslash = context->directoryExecutable;
    for(wchar_t* c = context->directoryExecutable; *c; c++)
        if(*c == '\\')
            lastBackslash = c;
    *(lastBackslash + 1) = 0;
    korl_log(INFO, "directoryExecutable=%ws", context->directoryExecutable);
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
    wchar_t filePath[MAX_PATH];
    _korl_file_resolvePath(fileName, pathTypeFileName, filePath, korl_arraySize(filePath));
    if(GetFileAttributes(filePath) == INVALID_FILE_ATTRIBUTES)
    {
        korl_log(INFO, "file '%ws' doesn't exist", filePath);
        return;
    }
    wchar_t filePathNew[MAX_PATH];
    _korl_file_resolvePath(fileNameNew, pathTypeFileNameNew, filePathNew, korl_arraySize(filePathNew));
    if(GetFileAttributes(filePathNew) != INVALID_FILE_ATTRIBUTES)
        if(DeleteFile(filePathNew))
            korl_log(INFO, "file '%ws' deleted", filePathNew);
        else
        {
            korl_logLastError("DeleteFile('%ws') failed", filePathNew);
            return;
        }
    if(!MoveFile(filePath, filePathNew))
        korl_logLastError("MoveFile('%ws', '%ws') failed", filePath, filePathNew);
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
    korl_assert(pAsyncOp->salt == handle->components.salt);
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
    wchar_t filePath[MAX_PATH];
    _korl_file_resolvePath(fileName, pathType, filePath, korl_arraySize(filePath));
    const HANDLE hFile = 
        CreateFileW(
            filePath, GENERIC_READ, FILE_SHARE_READ, NULL/*default security*/, 
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL/*no template file*/);
    if(hFile == INVALID_HANDLE_VALUE)
        korl_logLastError("CreateFileW failed! fileName=%ls", fileName);
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
    return true;
}
korl_internal void korl_file_generateMemoryDump(void* exceptionData, Korl_File_PathType type, u32 maxDumpCount)
{
    // derived from MSDN sample code:
    //	https://docs.microsoft.com/en-us/windows/win32/dxtecharts/crash-dump-analysis
    SYSTEMTIME localTime;
    GetLocalTime( &localTime );
    /* create a memory dump directory in the temporary data directory */
    wchar_t dumpDirectory[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(dumpDirectory, sizeof(dumpDirectory), 
                                          L"%ws\\%ws", 
                                          _korl_file_getPath(KORL_FILE_PATHTYPE_TEMPORARY_DATA), 
                                          L"memory-dumps"))
    {
        korl_log(ERROR, "dumpDirectory failed");
        return;
    }
    if(!CreateDirectory(dumpDirectory, NULL/*default security*/))
        switch(GetLastError())
        {
        case ERROR_ALREADY_EXISTS:
            break;
        case ERROR_PATH_NOT_FOUND:
            korl_log(ERROR, "CreateDirectory(%ws) failed: path not found", dumpDirectory);
            return;
        }
    /* delete the oldest dump folder after we reach some maximum dump count */
    {
        wchar_t filePathOldest[MAX_PATH];
        i$ filePathOldestSize = 0;
        korl_assert(maxDumpCount > 0);
        while(maxDumpCount <= _korl_file_findOldestFile(dumpDirectory, 
                                                        L"*-*-*-*-*", 
                                                        filePathOldest, 
                                                        sizeof(filePathOldest), 
                                                        &filePathOldestSize))
        {
            /* apparently pFrom needs to be double null-terminated */
            korl_assert(filePathOldestSize < korl_arraySize(filePathOldest) - 1);
            filePathOldest[filePathOldestSize] = L'\0';
            /**/
            korl_log(INFO, "deleting oldest dump folder: %ws", filePathOldest);
            KORL_ZERO_STACK(SHFILEOPSTRUCT, fileOpStruct);
            fileOpStruct.wFunc  = FO_DELETE;
            fileOpStruct.pFrom  = filePathOldest;
            fileOpStruct.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;
            const int resultFileOpDeleteRecursive = SHFileOperation(&fileOpStruct);
            if(resultFileOpDeleteRecursive != 0)
                korl_log(WARNING, "recursive delete of \"%ws\" failed; result=%i", 
                         filePathOldest, resultFileOpDeleteRecursive);
        }
    }
    // Create a companion folder to store PDB files specifically for this dump! //
    wchar_t directoryMemoryDump[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(directoryMemoryDump, sizeof(directoryMemoryDump), 
                                          L"%ws\\%ws-%04d%02d%02d-%02d%02d%02d-%ld-%ld", 
                                          dumpDirectory, KORL_APPLICATION_VERSION, 
                                          localTime.wYear, localTime.wMonth, localTime.wDay, 
                                          localTime.wHour, localTime.wMinute, localTime.wSecond, 
                                          GetCurrentProcessId(), GetCurrentThreadId()))
    {
        korl_log(ERROR, "directoryMemoryDump failed");
        return;
    }
    if(!CreateDirectory(directoryMemoryDump, NULL))
    {
        korl_logLastError("CreateDirectory(%ws) failed!", directoryMemoryDump);
        return;
    }
    else
        korl_log(INFO, "created MiniDump directory: %ws", directoryMemoryDump);
    // Create the mini dump! //
    wchar_t pathFileWrite[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(pathFileWrite, sizeof(pathFileWrite),
                                          L"%s\\%s-%04d%02d%02d-%02d%02d%02d-0x%X-0x%X.dmp", 
                                          directoryMemoryDump, KORL_APPLICATION_VERSION, 
                                          localTime.wYear, localTime.wMonth, localTime.wDay, 
                                          localTime.wHour, localTime.wMinute, localTime.wSecond, 
                                          GetCurrentProcessId(), GetCurrentThreadId()))
    {
        korl_log(ERROR, "pathFileWrite failed");
        return;
    }
    const HANDLE hDumpFile = CreateFile(pathFileWrite, GENERIC_READ|GENERIC_WRITE, 
                                        FILE_SHARE_WRITE|FILE_SHARE_READ, 
                                        0, CREATE_ALWAYS, 0, 0);
    if(INVALID_HANDLE_VALUE == hDumpFile)
    {
        korl_logLastError("CreateFile(%ws) failed!", pathFileWrite);
        return;
    }
    PEXCEPTION_POINTERS pExceptionPointers = KORL_C_CAST(PEXCEPTION_POINTERS, exceptionData);
    MINIDUMP_EXCEPTION_INFORMATION ExpParam = 
        { .ThreadId          = GetCurrentThreadId()
        , .ExceptionPointers = pExceptionPointers
        , .ClientPointers    = TRUE };
    if(!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
                          hDumpFile, MiniDumpWithDataSegs, &ExpParam, 
                          NULL, NULL))
    {
        korl_logLastError("MiniDumpWriteDump failed!");
        return;
    }
    korl_log(INFO, "MiniDump written to: %ws", pathFileWrite);
    /* Attempt to copy the win32 application's symbol files to the dump 
        location.  This will allow us to at LEAST view the call stack properly 
        during development for all of our minidumps even after making source 
        changes and subsequently rebuilding. */
    wchar_t pathFileRead[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(pathFileRead, sizeof(pathFileRead), L"%ws\\%ws.exe", 
                                          _korl_file_getPath(KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY), 
                                          KORL_APPLICATION_NAME))
    {
        korl_log(ERROR, "pathFileRead failed");
        return;
    }
    if(0 > korl_memory_stringFormatBuffer(pathFileWrite, sizeof(pathFileWrite), L"%ws\\%ws.exe", 
                                          directoryMemoryDump, KORL_APPLICATION_NAME))
    {
        korl_log(ERROR, "pathFileWrite failed");
        return;
    }
    if(!CopyFile(pathFileRead, pathFileWrite, FALSE))
        korl_log(WARNING, "CopyFile(%ws, %ws) failed!  GetLastError=0x%X", 
                 pathFileRead, pathFileWrite, GetLastError());
    else
        korl_log(INFO, "copied file \"%ws\" to \"%ws\"", pathFileRead, pathFileWrite);
    if(0 > korl_memory_stringFormatBuffer(pathFileRead, sizeof(pathFileRead), L"%ws\\%ws.pdb", 
                                          _korl_file_getPath(KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY), 
                                          KORL_APPLICATION_NAME))
    {
        korl_log(ERROR, "pathFileRead failed");
        return;
    }
    if(0 > korl_memory_stringFormatBuffer(pathFileWrite, sizeof(pathFileWrite), L"%ws\\%ws.pdb", 
                                          directoryMemoryDump, KORL_APPLICATION_NAME))
    {
        korl_log(ERROR, "pathFileWrite failed");
        return;
    }
    if(!CopyFile(pathFileRead, pathFileWrite, FALSE))
        korl_log(WARNING, "CopyFile(%ws, %ws) failed!  GetLastError=0x%X", 
                 pathFileRead, pathFileWrite, GetLastError());
    else
        korl_log(INFO, "copied file \"%ws\" to \"%ws\"", pathFileRead, pathFileWrite);
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
        return EXCEPTION_EXECUTE_HANDLER;
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
            return EXCEPTION_EXECUTE_HANDLER;
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
        return EXCEPTION_EXECUTE_HANDLER;
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
        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif// ^ either recycle this code at some point, or just delete it
}
