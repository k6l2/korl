#include "korl-file.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-assert.h"
typedef struct _Korl_File_Context
{
    wchar_t currentWorkingDirectory[MAX_PATH];
    /** NOT including the null-terminator! */
    DWORD   currentWorkingDirectorySize;
} _Korl_File_Context;
korl_global_variable _Korl_File_Context _korl_file_context;
korl_internal void _korl_file_resolvePath(
    const wchar_t*const fileName, Korl_File_PathType pathType, 
    wchar_t* o_filePath, u$ filePathMaxSize)
{
    _Korl_File_Context*const context = &_korl_file_context;
    const u$ fileNameSize = korl_memory_stringSize(fileName);
    switch(pathType)
    {
    case KORL_FILE_PATHTYPE_CURRENTWORKINGDIRECTORY:{
        u$ filePathRemainingSize = filePathMaxSize;
        i$ charsCopiedSansTerminator;
        /* copy the current working directory string into the filePath */
        charsCopiedSansTerminator = 
            korl_memory_stringCopy(context->currentWorkingDirectory, o_filePath, filePathRemainingSize) - 1;
        korl_assert(charsCopiedSansTerminator == context->currentWorkingDirectorySize);
        o_filePath            += charsCopiedSansTerminator;
        filePathRemainingSize -= charsCopiedSansTerminator;
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
    } break;
    default:
        korl_log(ERROR, "pathType (%i) not implemented!", pathType);
    }
}
korl_internal void korl_file_initialize(void)
{
    _Korl_File_Context*const context = &_korl_file_context;
    /* determine where the current working directory is in Windows */
    context->currentWorkingDirectorySize = 
        GetCurrentDirectory(
            korl_arraySize(context->currentWorkingDirectory), 
            context->currentWorkingDirectory);
    if(context->currentWorkingDirectorySize == 0)
        korl_logLastError("GetCurrentDirectory failed!");
    korl_assert(context->currentWorkingDirectorySize <= korl_arraySize(context->currentWorkingDirectory));
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
