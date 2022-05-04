#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
typedef enum Korl_File_PathType
    { KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY
    , KORL_FILE_PATHTYPE_LOCAL_DATA
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
    Korl_File_Descriptor_Flags flags;
} Korl_File_Descriptor;
typedef union Korl_File_AsyncWriteHandle
{
    struct
    {
        u16 salt;
        u16 index;
    } components;
    u32 value;
} Korl_File_AsyncWriteHandle;
korl_internal void korl_file_initialize(void);
/** \return \c true if the file was opened successfully, \c false otherwise.  
 * Upon successful execution, the file descriptor is stored in \c o_fileDescriptor. */
korl_internal bool korl_file_openAsync(Korl_File_PathType pathType, 
                                       const wchar_t* fileName, 
                                       Korl_File_Descriptor* o_fileDescriptor);
korl_internal void korl_file_close(Korl_File_Descriptor* fileDescriptor);
/** If \c fileNameNew exists, it is replaced with the contents of \c fileName .  
 * If \c fileName doesn't exist, the function will fail silently, and a warning 
 * is logged. */
korl_internal void korl_file_renameReplace(Korl_File_PathType pathTypeFileName,    const wchar_t* fileName, 
                                           Korl_File_PathType pathTypeFileNameNew, const wchar_t* fileNameNew);
/** The caller is responsible for keeping \c buffer alive until the write 
 * operation is complete. */
korl_internal Korl_File_AsyncWriteHandle korl_file_writeAsync(Korl_File_Descriptor fileDescriptor, const void* buffer, u$ bufferBytes);
/** \param timeoutMilliseconds 0 means the function returns immediately, 
 * KORL_U32_MAX means the function blocks until the write operation is complete.
 * \return \c true if the write operation finished, \c false otherwise. */
korl_internal bool korl_file_asyncWriteWait(Korl_File_AsyncWriteHandle* handle, u32 timeoutMilliseconds);
#if 0
korl_internal void korl_file_write(Korl_File_Descriptor fileDescriptor, const void* data, u$ dataBytes);
#endif
/** \return \c true if the file was loaded successfully, \c false otherwise. */
korl_internal bool korl_file_load(
    const wchar_t*const fileName, Korl_File_PathType pathType, 
    Korl_Memory_AllocatorHandle allocatorHandle, 
    void** out_data, u32* out_dataBytes);
