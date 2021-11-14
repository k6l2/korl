#pragma once
#include "korl-globalDefines.h"
typedef enum Korl_File_PathType
    { KORL_FILE_PATHTYPE_CURRENTWORKINGDIRECTORY
} Korl_File_PathType;
korl_internal void korl_file_initialize(void);
/**
 * \return \c true if the file was loaded successfully, \c false otherwise.
 */
korl_internal bool korl_file_load(
    const wchar_t*const fileName, Korl_File_PathType pathType, 
    const Korl_Memory_Allocator*const allocator, 
    void** out_data, u32* out_dataBytes);
