#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_Bluetooth_QueryEntry
{
    wchar_t name[128];
    u8      nameSize;
    u8      address[30];// opaque platform-defined format
} Korl_Bluetooth_QueryEntry;
/** \return stb_ds array of \c Korl_Bluetooth_QueryEntry allocated in \c allocator */
#define KORL_PLATFORM_BLUETOOTH_QUERY(name)      Korl_Bluetooth_QueryEntry*  name(Korl_Memory_AllocatorHandle allocator)
typedef u32 Korl_Bluetooth_SocketHandle;
#define KORL_PLATFORM_BLUETOOTH_CONNECT(name)    Korl_Bluetooth_SocketHandle name(const Korl_Bluetooth_QueryEntry* queryEntry)
#define KORL_PLATFORM_BLUETOOTH_DISCONNECT(name) void                        name(Korl_Bluetooth_SocketHandle socketHandle)
typedef enum Korl_Bluetooth_ReadResult
    { KORL_BLUETOOTH_READ_SUCCESS
    , KORL_BLUETOOTH_READ_DISCONNECT
    , KORL_BLUETOOTH_READ_INVALID_SOCKET_HANDLE
    , KORL_BLUETOOTH_READ_ERROR
} Korl_Bluetooth_ReadResult;
#define KORL_PLATFORM_BLUETOOTH_READ(name)       Korl_Bluetooth_ReadResult   name(Korl_Bluetooth_SocketHandle socketHandle, Korl_Memory_AllocatorHandle allocator, au8* out_data)
