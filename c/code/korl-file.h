#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
typedef enum Korl_File_PathType
    { KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY
    , KORL_FILE_PATHTYPE_LOCAL_DATA
    , KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY
    , KORL_FILE_PATHTYPE_TEMPORARY_DATA
    , KORL_FILE_PATHTYPE_ENUM_COUNT// keep last!
} Korl_File_PathType;
typedef enum Korl_File_Descriptor_Flags
{
    KORL_FILE_DESCRIPTOR_FLAG_ASYNC = 1 << 0,
    KORL_FILE_DESCRIPTOR_FLAG_READ  = 1 << 1,
    KORL_FILE_DESCRIPTOR_FLAG_WRITE = 1 << 2,
} Korl_File_Descriptor_Flags;
typedef struct Korl_File_Descriptor
{
    void* handle;
    u64 asyncKey;// A unique identifier to link async operations to file handles, in the off chance file handle values are re-used; 0 => invalid key
    Korl_File_Descriptor_Flags flags;
} Korl_File_Descriptor;
/** A handle to a pending asynchronous I/O operation.  A handle value of \c 0 is 
 * considered to be _invalid_ (no associated pending operation). */
typedef u32 Korl_File_AsyncIoHandle;
typedef enum Korl_File_GetAsyncIoResult
    { KORL_FILE_GET_ASYNC_IO_RESULT_DONE
    , KORL_FILE_GET_ASYNC_IO_RESULT_PENDING
    , KORL_FILE_GET_ASYNC_IO_RESULT_INVALID_HANDLE
} Korl_File_GetAsyncIoResult;
korl_internal void korl_file_initialize(void);
/** \return \c true if the file was opened successfully, \c false otherwise.  
 * Upon successful execution, the file descriptor is stored in \c o_fileDescriptor. */
korl_internal bool korl_file_open(Korl_File_PathType pathType, 
                                  const wchar_t* fileName, 
                                  Korl_File_Descriptor* o_fileDescriptor, 
                                  bool async);
korl_internal void korl_file_close(Korl_File_Descriptor* fileDescriptor);
/** If \c fileNameNew exists, it is replaced with the contents of \c fileName .  
 * If \c fileName doesn't exist, the function will fail silently, and a warning 
 * is logged. */
korl_internal void korl_file_renameReplace(Korl_File_PathType pathTypeFileName,    const wchar_t* fileName, 
                                           Korl_File_PathType pathTypeFileNameNew, const wchar_t* fileNameNew);
/** The caller is responsible for keeping \c buffer alive until the write 
 * operation is complete. */
korl_internal Korl_File_AsyncIoHandle korl_file_writeAsync(Korl_File_Descriptor fileDescriptor, const void* buffer, u$ bufferBytes);
/** The caller is expected to gracefully handle the \c *_RESULT_INVALID_HANDLE 
 * result at any time, since it is possible for the file module to invalidate 
 * any async io handle at any time.  This is required, for example, to be able 
 * to perform application memory savestate features, as the loaded memory state 
 * is likely to contain async io handles which are no longer valid since they 
 * are from another session. */
korl_internal Korl_File_GetAsyncIoResult korl_file_getAsyncIoResult(Korl_File_AsyncIoHandle* handle, bool blockUntilComplete);
korl_internal void korl_file_write(Korl_File_Descriptor fileDescriptor, const void* data, u$ dataBytes);
korl_internal u32 korl_file_getTotalBytes(Korl_File_Descriptor fileDescriptor);
/** Read the entire contents of the file.
 * \return \c true if the file was read successfully, \c false otherwise. */
korl_internal bool korl_file_read(Korl_File_Descriptor fileDescriptor, 
                                  void* buffer, u32 bufferBytes);
korl_internal Korl_File_AsyncIoHandle korl_file_readAsync(Korl_File_Descriptor fileDescriptor, 
                                                          void* buffer, u32 bufferBytes);
korl_internal void korl_file_generateMemoryDump(void* exceptionData, Korl_File_PathType type, u32 maxDumpCount);
korl_internal void korl_file_saveStateCreate(void);
korl_internal void korl_file_saveStateSave(Korl_File_PathType pathType, const wchar_t* fileName);
korl_internal void korl_file_saveStateLoad(Korl_File_PathType pathType, const wchar_t* fileName);
