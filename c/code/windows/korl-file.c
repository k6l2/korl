#include "korl-file.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-assert.h"
typedef struct _Korl_File_AsyncOpertion
{
    OVERLAPPED overlapped;
    u16 salt;
    const void* writeBuffer;
    HANDLE handleFile;
} _Korl_File_AsyncOpertion;
typedef struct _Korl_File_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    wchar_t* directoryLocalData;
    wchar_t  directoryCurrentWorking[MAX_PATH];
    DWORD sizeCurrentWorking;/** NOT including the null-terminator! */
    DWORD sizeLocalData;     /** NOT including the null-terminator! */
    /** We cannot use a KORL_MEMORY_POOL here, because we need the memory 
     *  locations of used OVERLAPPED structs to remain completely STATIC once in 
     *  use by Windows.  
     *  The indices into this pool should be considered as "handles" to users of 
     *  this code module.
     *  Individual elements of this pool are considered to be "unused" if the 
     *  \c writeBuffer member == \c NULL */
    _Korl_File_AsyncOpertion asyncPool[64];
} _Korl_File_Context;
korl_global_variable _Korl_File_Context _korl_file_context;
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
    // clean up the async operation slot //
    pAsyncOp->writeBuffer = NULL;
    pAsyncOp->salt++;
}
korl_internal void korl_file_initialize(void)
{
    _Korl_File_Context*const context = &_korl_file_context;
    korl_memory_zero(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_kilobytes(64));
    /* determine where the current working directory is in Windows */
    context->sizeCurrentWorking = 
        GetCurrentDirectory(
            korl_arraySize(context->directoryCurrentWorking), 
            context->directoryCurrentWorking);
    if(context->sizeCurrentWorking == 0)
        korl_logLastError("GetCurrentDirectory failed!");
    korl_assert(context->sizeCurrentWorking <= korl_arraySize(context->directoryCurrentWorking));
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
            korl_assert(!"CreateDirectory(directoryLocalData) failed");
            break;}
        }
    }
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
    pAsyncOp->writeBuffer           = buffer;
    pAsyncOp->handleFile            = fileDescriptor.handle;
    pAsyncOp->overlapped.Offset     = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file
    pAsyncOp->overlapped.OffsetHigh = KORL_U32_MAX;// setting both offsets to u32_max writes to the end of the file //@todo: validate that this will actually work?...
    /* dispatch the async write command */
    const BOOL resultWriteFileEx = WriteFileEx(fileDescriptor.handle, 
                                               buffer, korl_checkCast_u$_to_u32(bufferBytes), 
                                               &(pAsyncOp->overlapped), _korl_file_LpoverlappedCompletionRoutine);
    korl_assert(resultWriteFileEx);
    /* return a handle to the async operation (OVERLAPPED slot) to the user */
    return (Korl_File_AsyncWriteHandle){.components={ .salt  = context->asyncPool[i].salt
                                                    , .index = korl_checkCast_u$_to_u16(i) }};
}
korl_internal bool korl_file_asyncWriteWait(Korl_File_AsyncWriteHandle* handle, u32 timeoutMilliseconds)
{
    _Korl_File_Context*const context = &_korl_file_context;
    korl_assert(handle->components.index < korl_arraySize(context->asyncPool));
    _Korl_File_AsyncOpertion*const pAsyncOp = &context->asyncPool[handle->components.index];
    korl_assert(pAsyncOp->salt == handle->components.salt);
    DWORD numberOfBytesTransferred = 0;
    const BOOL resultGetOverlappedResult = 
        GetOverlappedResultEx(pAsyncOp->handleFile, &(pAsyncOp->overlapped), 
                              &numberOfBytesTransferred, timeoutMilliseconds, 
                              TRUE/*yes, we want to call the I/O completion routine*/);
    if(!resultGetOverlappedResult)
    {
        const DWORD errorCode = GetLastError();
        if(     errorCode == ERROR_IO_INCOMPLETE)
            // operation is still in progress, timeoutMilliseconds was == 0
            return false;
        else if(errorCode == WAIT_IO_COMPLETION)
        {
            /* the async operation is complete, and the completion routine has 
                been queued */
            goto done_asyncComplete;
        }
        else if(errorCode == WAIT_TIMEOUT)
            // operation is still in progress, timeoutMilliseconds was > 0
            return false;
        /* if execution ever reaches here, we haven't accounted for some other 
            failure condition */
        korl_logLastError("GetOverlappedResultEx failed!");
    }
    /* the async operation is complete, and we didn't have to queue the I/O 
        completion routine */
    // clean up the async operation slot //
    pAsyncOp->writeBuffer = NULL;
    pAsyncOp->salt++;
done_asyncComplete:
    handle->components.index = KORL_U16_MAX;// invalidate the caller's async operation handle
    return true;
}
#if 0
korl_internal void korl_file_write(Korl_File_Handle fileHandle, const void* data, u$ dataBytes)
{
    const HANDLE hFile = fileHandle;
}
#endif
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
