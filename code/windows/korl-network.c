#include "korl-network.h"
#include "korl-windows-globalDefines.h"
#include "utility/korl-checkCast.h"
#include "korl-interface-platform.h"
typedef struct _Korl_Network_Context
{
    WSADATA          wsa;
    SOCKET           sockets[32];
    CRITICAL_SECTION criticalSection;
} _Korl_Network_Context;
korl_global_variable _Korl_Network_Context _korl_network_context;
KORL_STATIC_ASSERT(korl_arraySize(_korl_network_context.sockets) < KORL_U8_MAX);
korl_internal void korl_network_initialize(void)
{
    korl_shared_const WORD DESIRED_WINSOCK_VERSION = MAKEWORD(2,2);
    _Korl_Network_Context*const context = &_korl_network_context;
    korl_memory_zero(context, sizeof(context));
    for(u$ i = 0; i < korl_arraySize(context->sockets); i++)
        context->sockets[i] = INVALID_SOCKET;
    InitializeCriticalSection(&context->criticalSection);
    const int resultWsaStartup = WSAStartup(DESIRED_WINSOCK_VERSION, &context->wsa);
    if(resultWsaStartup != 0)
    {
        korl_log(ERROR, "Failed to start WinSock!");
        return;
    }
    if(   LOBYTE(context->wsa.wVersion) != LOBYTE(DESIRED_WINSOCK_VERSION) 
       || HIBYTE(context->wsa.wVersion) != HIBYTE(DESIRED_WINSOCK_VERSION))
    {
        korl_log(ERROR, "Could not find winsock v%i.%i compatible DLL!"
                ,HIBYTE(DESIRED_WINSOCK_VERSION), LOBYTE(DESIRED_WINSOCK_VERSION));
        WSACleanup();
        /* do I ever need to call WSACleanup anywhere else in the program (probably not?..) */
        return;
    }
}
korl_internal KORL_FUNCTION_korl_network_resolveAddress(korl_network_resolveAddress)
{
    Korl_Network_Address result = (Korl_Network_Address){0};
    KORL_ZERO_STACK(ADDRINFO, hints);
    /* notice that the members of addrinfo here are essentially the same 
        datum that are passed to the `socket` function! */
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    ADDRINFO* addrInfoData;
    const INT resultGetAddrInfo = getaddrinfo(KORL_C_CAST(PCSTR, ansiAddress.data), NULL, &hints, &addrInfoData);
    if(resultGetAddrInfo != 0)
    {
        korl_log(WARNING, "getaddrinfo failed! result=%i", resultGetAddrInfo);
        goto cleanUp;
    }
    SOCKADDR_IN socketAddressIpv4;
    socketAddressIpv4.sin_addr.s_addr = htonl(INADDR_ANY);
    u16 addrInfoDataIndex = 0;
    /* iterate over all the address infos associated with this address query 
        and select the first valid address we find (if it exists) */
    for(ADDRINFO* addrIterator = addrInfoData; addrIterator; addrIterator = addrIterator->ai_next)
    {
        korl_log(VERBOSE, "--- addrInfoData[%i] ---", addrInfoDataIndex++);
        korl_log(VERBOSE, "\tflags: 0x%x", addrIterator->ai_flags);
        korl_log(VERBOSE, "\tfamily:");
        switch(addrIterator->ai_family)
        {
        case AF_UNSPEC: {
            korl_log(VERBOSE, "\t\tUnspecified");
        break;}
        case AF_INET: {
            CHAR cStrBufferIpAddress[16];// For an IPv4 address, this buffer should be large enough to hold at least 16 characters
            inet_ntop(AF_INET, &KORL_C_CAST(SOCKADDR_IN*, addrIterator->ai_addr)->sin_addr, cStrBufferIpAddress, korl_arraySize(cStrBufferIpAddress));
            if(socketAddressIpv4.sin_addr.s_addr != htonl(INADDR_ANY))
            /* if we've already found a valid address, just print it out 
                for diagnostic purposes and keep going... */
            {
                korl_log(VERBOSE, "\t\tIPv4='%hs'", cStrBufferIpAddress);
                break;
            }
            /* just select the first valid address we find */
            socketAddressIpv4 = *KORL_C_CAST(SOCKADDR_IN*, addrIterator->ai_addr);
            korl_log(VERBOSE, "\t\tFIRST IPv4='%hs'", cStrBufferIpAddress);
        break;}
        case AF_INET6: {
            WCHAR wcStrBufferIpAddress[46];
            DWORD wcStrBufferIpAddressLength = korl_arraySize(wcStrBufferIpAddress);
            const INT resultAddressToString = 
                WSAAddressToStringW(addrIterator->ai_addr
                                   ,korl_checkCast_u$_to_u32(addrIterator->ai_addrlen)
                                   ,NULL, wcStrBufferIpAddress
                                   ,&wcStrBufferIpAddressLength);
            if(resultAddressToString == 0)
            {
                korl_log(VERBOSE, "\t\tIPv6='%ws'", wcStrBufferIpAddress);
            }
            else
            {
                korl_log(VERBOSE, "\t\tIPv6; WSAAddressToString failed! result=%i", 
                     resultAddressToString);
            }
        break;}
        case AF_NETBIOS: {
            korl_log(VERBOSE, "\t\tNetBIOS");
        break;}
        default: {
            korl_log(VERBOSE, "\t\tother (%i)", addrIterator->ai_family);
        break;}
        }
        korl_log(VERBOSE, "\tSocket type: ");
        switch (addrIterator->ai_socktype) 
        {
        case 0             :{korl_log(VERBOSE, "\t\tUnspecified"); break;}
        case SOCK_STREAM   :{korl_log(VERBOSE, "\t\tSOCK_STREAM (stream)"); break;}
        case SOCK_DGRAM    :{korl_log(VERBOSE, "\t\tSOCK_DGRAM (datagram) "); break;}
        case SOCK_RAW      :{korl_log(VERBOSE, "\t\tSOCK_RAW (raw) "); break;}
        case SOCK_RDM      :{korl_log(VERBOSE, "\t\tSOCK_RDM (reliable message datagram)"); break;}
        case SOCK_SEQPACKET:{korl_log(VERBOSE, "\t\tSOCK_SEQPACKET (pseudo-stream packet)"); break;}
        default            :{korl_log(VERBOSE, "\t\tOther %i", addrIterator->ai_socktype); break;}
        }
        korl_log(VERBOSE, "\tProtocol: ");
        switch (addrIterator->ai_protocol) 
        {
        case 0          :{korl_log(VERBOSE, "\t\tUnspecified"); break;}
        case IPPROTO_TCP:{korl_log(VERBOSE, "\t\tIPPROTO_TCP (TCP)"); break;}
        case IPPROTO_UDP:{korl_log(VERBOSE, "\t\tIPPROTO_UDP (UDP) "); break;}
        default         :{korl_log(VERBOSE, "\t\tOther %i", addrIterator->ai_protocol); break;}
        }
        korl_log(VERBOSE, "\tLength of this sockaddr: %ui", addrIterator->ai_addrlen);
        korl_log(VERBOSE, "\tCanonical name: %hs", addrIterator->ai_canonname);
    }
    if(socketAddressIpv4.sin_addr.s_addr == htonl(INADDR_ANY))
    {
        korl_log(ERROR, "Failed to resolve address! '%hs'", ansiAddress.data);
        goto cleanUp;
    }
    /* @robustness: create a 'ipv4_to_kpl' conversion function */
    result.uInts[0] = ntohl(socketAddressIpv4.sin_addr.s_addr);
    cleanUp:
        if(resultGetAddrInfo == 0)
            freeaddrinfo(addrInfoData);
    return result;
}
