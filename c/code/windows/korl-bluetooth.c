/**
 * Most of the code in this module is derived from the Microsoft sample found here:
 * https://github.com/microsoftarchive/msdn-code-gallery-microsoft/tree/master/Official%20Windows%20Platform%20Sample/Bluetooth%20connection%20sample
 * Which was obtained from this page on MSDN:
 * https://docs.microsoft.com/en-us/windows/win32/bluetooth/bluetooth-programming-with-windows-sockets
 * The above example is actually failing to connect to my Sparkfun Bluesmirf 
 * Silver, so the following example code was used to modify that MSDN example: 
 * https://stackoverflow.com/a/69474526/4526664
 */
#include "korl-bluetooth.h"
#include "korl-crash.h"
#include "korl-log.h"
#include "korl-stb-ds.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-checkCast.h"
// DEFINE_GUID(_KORL_BLUETOOTH_SERVICE_CLASS_GUID, 0xb62c4e8d, 0x62cc, 0x404b, 0xbb, 0xbf, 0xbf, 0x3e, 0x3b, 0xbb, 0x13, 0x74);// {B62C4E8D-62CC-404b-BBBF-BF3E3BBB1374}
typedef struct _Korl_Bluetooth_Socket
{
    Korl_Bluetooth_SocketHandle handle;// a value of 0 means that this socket is unused; otherwise, this value should be == (1 + socketArrayIndex) of this socket
    Korl_Bluetooth_QueryEntry queryEntry;
    SOCKET socket;
} _Korl_Bluetooth_Socket;
typedef struct _Korl_Bluetooth_Context
{
    _Korl_Bluetooth_Socket sockets[8];
} _Korl_Bluetooth_Context;
korl_global_variable _Korl_Bluetooth_Context _korl_bluetooth_context;
korl_internal void korl_bluetooth_initialize(void)
{
    korl_memory_zero(&_korl_bluetooth_context, sizeof(_korl_bluetooth_context));
    KORL_ZERO_STACK(WSADATA, wsaData);
    const WORD wsaVersionRequested = MAKEWORD(2,2);
    const int resultWsaStartup = WSAStartup(wsaVersionRequested, &wsaData);
    if(0 != resultWsaStartup)
        korl_log(ERROR, "WSAStartup failed: %i", resultWsaStartup);
    if(   LOBYTE(wsaData.wVersion) != LOBYTE(wsaVersionRequested)
       || HIBYTE(wsaData.wVersion) != HIBYTE(wsaVersionRequested))
        korl_log(ERROR, "WSAStartup: failed to find winsock library matching version {%hhi, %hhi}; version obtained: {%hhi, %hhi}", 
                 LOBYTE(wsaVersionRequested), HIBYTE(wsaVersionRequested), 
                 LOBYTE(wsaData.wVersion)   , HIBYTE(wsaData.wVersion));
}
korl_internal KORL_PLATFORM_BLUETOOTH_QUERY(korl_bluetooth_query)
{
    Korl_Bluetooth_QueryEntry* result        = NULL;
    ULONG wsaQuerySetBytes                   = sizeof(WSAQUERYSET);
    WSAQUERYSET* wsaQuerySet                 = korl_allocate(allocator, wsaQuerySetBytes);
    HANDLE wsaLookupServiceHandle            = INVALID_HANDLE_VALUE;
    const DWORD wsaLookupServiceControlFlags = LUP_CONTAINERS // query only devices (not services)
                                             | LUP_RETURN_NAME// Retrieves the name as lpszServiceInstanceName
                                             | LUP_RETURN_ADDR// Retrieves the addresses as lpcsaBuffer
                                             | LUP_FLUSHCACHE;// perform a fresh query
    bool continueQuery                       = true;
    wsaQuerySet->dwSize      = wsaQuerySetBytes;
    wsaQuerySet->dwNameSpace = NS_BTH;// Bluetooth SDP Namespace
    if(0 != WSALookupServiceBegin(wsaQuerySet, wsaLookupServiceControlFlags, &wsaLookupServiceHandle))
    {
        korl_log(ERROR, "WSALookupServiceBegin failed; error=%i", WSAGetLastError());
        goto cleanUp_returnQuery;
    }
#if KORL_DEBUG
    korl_log(VERBOSE, "----- new bluetooth query -----");
#endif
    while(continueQuery)
    {
        if(0 == WSALookupServiceNext(wsaLookupServiceHandle, wsaLookupServiceControlFlags, &wsaQuerySetBytes, wsaQuerySet))
        {
            /* add the new bluetooth device to the query result */
#if KORL_DEBUG
            korl_log(VERBOSE, "\t\"%ws\" ", wsaQuerySet->lpszServiceInstanceName);
#endif
            SOCKADDR_BTH*const socketAddressBluetooth = KORL_C_CAST(SOCKADDR_BTH*, wsaQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr);
            Korl_Bluetooth_QueryEntry newEntry;
            newEntry.nameSize = korl_checkCast_u$_to_u8(korl_memory_stringSize(wsaQuerySet->lpszServiceInstanceName));
            korl_assert(korl_arraySize(newEntry.name)    >= newEntry.nameSize + 1/*null-terminator*/);
            korl_assert(        sizeof(newEntry.address) >= sizeof(*socketAddressBluetooth));
            korl_assert(socketAddressBluetooth->addressFamily == AF_BTH);
            korl_assert(socketAddressBluetooth->port          == 0);
            // Prepare this bluetooth socket address for the next step (connection) ahead of time:
            socketAddressBluetooth->serviceClassId = RFCOMM_PROTOCOL_UUID;
            socketAddressBluetooth->port           = BT_PORT_ANY;
            korl_memory_copy(newEntry.name   , wsaQuerySet->lpszServiceInstanceName, newEntry.nameSize*sizeof(*wsaQuerySet->lpszServiceInstanceName));
            korl_memory_copy(newEntry.address, socketAddressBluetooth              , sizeof(*socketAddressBluetooth));
            newEntry.name[newEntry.nameSize] = L'\0';// null-terminate the raw device name string
            mcarrpush(KORL_STB_DS_MC_CAST(allocator), result, newEntry);
        }
        else
        {
            const int errorWsaLookupServiceNext = WSAGetLastError();
            switch(errorWsaLookupServiceNext)
            {
            case WSAENOMORE:    // conflicting error code in winsock 2 that we need to handle according to MSDN; thanks, μshaft
            case WSA_E_NO_MORE:{// no more data; the query is over
                continueQuery = false;
                goto cleanUp_returnQuery;}
            case WSAEFAULT:{// The lpqsResults buffer was too small to contain a WSAQUERYSET set
                /* the required size was returned in the third parameter of WSALookupServiceNext */
                wsaQuerySet = korl_reallocate(allocator, wsaQuerySet, wsaQuerySetBytes);
                /* now we can continue the outer query loop to attempt to get the next device again */
                break;}
            default:{
                korl_log(ERROR, "WSALookupServiceNext failed; error=%i", errorWsaLookupServiceNext);
                goto cleanUp_returnQuery;}
            }
        }
    }
    cleanUp_returnQuery:
#if KORL_DEBUG
    korl_log(VERBOSE, "----- END bluetooth query -----");
#endif
    WSALookupServiceEnd(wsaLookupServiceHandle);
    korl_free(allocator, wsaQuerySet);
    return result;
}
korl_internal KORL_PLATFORM_BLUETOOTH_CONNECT(korl_bluetooth_connect)
{
    _Korl_Bluetooth_Socket* korlSocket = NULL;
    /* find a socket that is unused */
    for(u$ i = 0; i < korl_arraySize(_korl_bluetooth_context.sockets); i++)
    {
        if(_korl_bluetooth_context.sockets[i].handle)
        {
            /* ensure that an existing connection doesn't exist for this queryEntry */
            //KORL-ISSUE-000-000-095: bluetooth: maybe compare addresses instead of device names?
            korl_assert(0 != korl_memory_stringCompare(_korl_bluetooth_context.sockets[i].queryEntry.name, queryEntry->name));
            continue;
        }
        korlSocket = &_korl_bluetooth_context.sockets[i];
        korlSocket->handle = korl_checkCast_u$_to_u32(i + 1);
        korl_memory_copy(&korlSocket->queryEntry, queryEntry, sizeof(*queryEntry));
        break;
    }
    if(!korlSocket)
        goto errorCleanUp_returnNullHandle;
    /* create & connect the bluetooth korlSocket to the query entry address */
    korlSocket->socket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if(korlSocket->socket == INVALID_SOCKET)
    {
        korl_log(ERROR, "socket failed; error=%i", WSAGetLastError());
        goto errorCleanUp_returnNullHandle;
    }
    if(SOCKET_ERROR == connect(korlSocket->socket, 
                               KORL_C_CAST(struct sockaddr*, &korlSocket->queryEntry.address), 
                               sizeof(SOCKADDR_BTH)))
    {
        korl_log(ERROR, "connect failed; error=%i", WSAGetLastError());
        goto errorCleanUp_returnNullHandle;
    }
    /* configure the socket to be non-blocking 
        https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-ioctlsocket?redirectedfrom=MSDN
        https://docs.microsoft.com/en-us/windows/win32/winsock/winsock-ioctls */
    u_long ioctlArgP = 1;// for FIONBIO: non-zero=>enable, zero=>disable
    if(0 != ioctlsocket(korlSocket->socket, FIONBIO/*(en/dis)able non-blocking mode*/, &ioctlArgP))
    {
        korl_log(ERROR, "ioctlsocket(FIONBIO, %lu) failed; error=%i", ioctlArgP, WSAGetLastError());
        goto errorCleanUp_returnNullHandle;
    }
    return korlSocket->handle;
    errorCleanUp_returnNullHandle:
    if(korlSocket)
    {
        if(0 != closesocket(korlSocket->socket))
            korl_log(ERROR, "closesocket failed; error=%i", WSAGetLastError());
        korl_memory_zero(korlSocket, sizeof(*korlSocket));
    }
    return 0;
}
korl_internal KORL_PLATFORM_BLUETOOTH_DISCONNECT(korl_bluetooth_disconnect)
{
    /* find the socket using the passed in handle */
    _Korl_Bluetooth_Socket* korlSocket = NULL;
    if(socketHandle > 0)
    {
        const u$ socketIndex = socketHandle - 1;
        if(_korl_bluetooth_context.sockets[socketIndex].handle == socketHandle)
            korlSocket = &(_korl_bluetooth_context.sockets[socketIndex]);
    }
    korl_assert(korlSocket);
    /* close, then nullify the associated KORL socket */
    if(0 != closesocket(korlSocket->socket))
        korl_log(ERROR, "closesocket failed; error=%i", WSAGetLastError());
    korl_memory_zero(korlSocket, sizeof(*korlSocket));
}
korl_internal KORL_PLATFORM_BLUETOOTH_READ(korl_bluetooth_read)
{
    /* find the socket using the passed in handle */
    _Korl_Bluetooth_Socket* korlSocket = NULL;
    if(socketHandle > 0)
    {
        const u$ socketIndex = socketHandle - 1;
        if(_korl_bluetooth_context.sockets[socketIndex].handle == socketHandle)
            korlSocket = &(_korl_bluetooth_context.sockets[socketIndex]);
    }
    if(!korlSocket)
        return KORL_BLUETOOTH_READ_INVALID_SOCKET_HANDLE;
    /* perform a read on the socket */
    // first we need to find out how much data is in the socket
    u_long bufferSize = 0;
    int resultRecv;
    resultRecv = recv(korlSocket->socket, NULL/*buffer*/, 0/*bufferBytes*/, 0/*flags*/);
    if(0 == resultRecv)
    {
#if 0/* MSDN says that when the result is 0, the connection is closed, but that 
        doesn't seem to be the case when running a non-blocking socket while 
        passing an empty buffer */
        /* the socket was closed */
        korl_log(VERBOSE, "recv=>0; socket disconnected");
        korl_memory_zero(korlSocket, sizeof(*korlSocket));
        return KORL_BLUETOOTH_READ_DISCONNECT;
#else
        /* check how much memory we need for the buffer */
        if(0 != ioctlsocket(korlSocket->socket, FIONREAD, &bufferSize))
        {
            korl_log(ERROR, "ioctlsocket(FIONREAD) failed; error=%i", WSAGetLastError());
            return KORL_BLUETOOTH_READ_ERROR;
        }
        if(!bufferSize)
        {
            korl_log(VERBOSE, "ioctlsocket(FIONREAD)=>0; socket disconnected");
            korl_memory_zero(korlSocket, sizeof(*korlSocket));
            return KORL_BLUETOOTH_READ_DISCONNECT;
        }
#endif
    }
    else if(SOCKET_ERROR == resultRecv)
    {
        const int errorRecv = WSAGetLastError();
        switch(errorRecv)
        {
        case WSAEWOULDBLOCK:{
            /* there was no data in the socket at this time */
            korl_memory_zero(out_data, sizeof(*out_data));
            return KORL_BLUETOOTH_READ_SUCCESS;
            break;}
#if 0/* experimentally (contrary to MSDN) message size must be checked in the above case (when 0 == resultRecv) */
        case WSAEMSGSIZE:{
            /* check how much memory we need for the buffer */
            if(0 != ioctlsocket(korlSocket->socket, FIONREAD, &bufferSize))
            {
                korl_log(ERROR, "ioctlsocket(FIONREAD) failed; error=%i", WSAGetLastError());
                return KORL_BLUETOOTH_READ_ERROR;
            }
            break;}
#endif
        default:{
            korl_log(ERROR, "recv failed; error=%i", errorRecv);
            return KORL_BLUETOOTH_READ_ERROR;}
        }
    }
    else
        /* is it even possible to end up in this case, since we are passing a NULL buffer? */
        korl_assert(false);
#if KORL_DEBUG && 0
    korl_log(VERBOSE, "bufferSize=%lu", bufferSize);
    korl_memory_zero(out_data, sizeof(*out_data));
#endif
    // now that we know that the socket is still connected & there is data, we 
    //  can actually fill up a buffer with data
    out_data->size = bufferSize;
    out_data->data = korl_allocate(allocator, out_data->size);
    resultRecv = recv(korlSocket->socket, KORL_C_CAST(char*, out_data->data), korl_checkCast_u$_to_i32(out_data->size), 0/*flags*/);
    if(0 == resultRecv)
    {
#if 1/* MSDN says that when the result is 0, the connection is closed, but that 
        doesn't seem to be the case when running a non-blocking socket while 
        passing an empty buffer */
        /* the socket was closed */
        korl_log(VERBOSE, "recv=>0; socket disconnected");
        korl_memory_zero(korlSocket, sizeof(*korlSocket));
        return KORL_BLUETOOTH_READ_DISCONNECT;
#else
        /* check how much memory we need for the buffer */
        if(0 != ioctlsocket(korlSocket->socket, FIONREAD, &bufferSize))
        {
            korl_log(ERROR, "ioctlsocket(FIONREAD) failed; error=%i", WSAGetLastError());
            return KORL_BLUETOOTH_READ_ERROR;
        }
        korl_assert(bufferSize);
#endif
    }
    else if(SOCKET_ERROR == resultRecv)
    {
#if 1
        korl_log(ERROR, "recv failed; error=%i", WSAGetLastError());
        return KORL_BLUETOOTH_READ_ERROR;
#else/* this code might be necessary if we get into the situation where our buffer wasn't big enough to store all the data */
        const int errorRecv = WSAGetLastError();
        switch(errorRecv)
        {
#if 0/* experimentally (contrary to MSDN) message size must be checked in the above case */
        case WSAEMSGSIZE:{
            /* check how much memory we need for the buffer */
            if(0 != ioctlsocket(korlSocket->socket, FIONREAD, &bufferSize))
            {
                korl_log(ERROR, "ioctlsocket(FIONREAD) failed; error=%i", WSAGetLastError());
                return KORL_BLUETOOTH_READ_ERROR;
            }
            break;}
#endif
        default:{
            korl_log(ERROR, "recv failed; error=%i", errorRecv);
            return KORL_BLUETOOTH_READ_ERROR;}
        }
#endif
    }
    return KORL_BLUETOOTH_READ_SUCCESS;
}
