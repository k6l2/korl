/**
 * Most of the code in this module is derived from the Microsoft sample found here:
 * https://github.com/microsoftarchive/msdn-code-gallery-microsoft/tree/master/Official%20Windows%20Platform%20Sample/Bluetooth%20connection%20sample
 * Which was obtained from this page on MSDN:
 * https://docs.microsoft.com/en-us/windows/win32/bluetooth/bluetooth-programming-with-windows-sockets
 */
#include "korl-bluetooth.h"
#include "korl-crash.h"
#include "korl-log.h"
#include "korl-stb-ds.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-checkCast.h"
korl_internal void korl_bluetooth_initialize(void)
{
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
#if KORL_DEBUG
            korl_log(VERBOSE, "\t\"%ws\" ", wsaQuerySet->lpszServiceInstanceName);
#endif
            Korl_Bluetooth_QueryEntry newEntry;
            newEntry.nameSize = korl_checkCast_u$_to_u8(korl_memory_stringSize(wsaQuerySet->lpszServiceInstanceName));
            korl_assert(korl_arraySize(newEntry.name)    >= newEntry.nameSize + 1/*null-terminator*/);
            korl_assert(        sizeof(newEntry.address) >= sizeof(*KORL_C_CAST(SOCKADDR_BTH*, wsaQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr)));
            korl_memory_copy(newEntry.name   , wsaQuerySet->lpszServiceInstanceName, newEntry.nameSize*sizeof(*wsaQuerySet->lpszServiceInstanceName));
            korl_memory_copy(newEntry.address, wsaQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr, sizeof(*KORL_C_CAST(SOCKADDR_BTH*, wsaQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr)));
            newEntry.name[newEntry.nameSize] = L'\0';// null-terminate the raw device name string
            mcarrpush(KORL_C_CAST(void*, allocator), result, newEntry);
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
