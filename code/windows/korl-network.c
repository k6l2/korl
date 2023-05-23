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
typedef union _Korl_Network_WinSockAddress
{
    SOCKADDR         sa;
    SOCKADDR_IN      s4;
    SOCKADDR_IN6     s6;
    SOCKADDR_STORAGE ss;
} _Korl_Network_WinSockAddress;
/**
 * \return the length of the winsock sockaddr structure
 */
korl_internal int _korl_network_netAddressToNative(const Korl_Network_Address*const netAddress, u16 port, _Korl_Network_WinSockAddress*const o_winSockAddress)
{
    korl_memory_zero(o_winSockAddress, sizeof(*o_winSockAddress));
    o_winSockAddress->s4.sin_family      = AF_INET;
    o_winSockAddress->s4.sin_addr.s_addr = htonl(netAddress->uInts[0]);
    o_winSockAddress->s4.sin_port        = htons(port);
    return sizeof(o_winSockAddress->s4);
}
korl_global_variable _Korl_Network_Context _korl_network_context;
KORL_STATIC_ASSERT(korl_arraySize(_korl_network_context.sockets) < KORL_U8_MAX);
korl_internal Korl_Network_Socket _korl_network_makeSocket(Korl_Network_Socket index)
{
    const Korl_Network_Socket result = index + 1;
    korl_assert(result != 0);
    return result;
}
korl_internal Korl_Network_Socket _korl_network_getSocketIndex(Korl_Network_Socket socket)
{
    _Korl_Network_Context*const context = &_korl_network_context;
    korl_assert(socket != 0);
    const Korl_Network_Socket socketIndex = socket - 1;
    korl_assert(socketIndex < korl_arraySize(context->sockets));
    return socketIndex;
}
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
    ADDRINFO* addrInfoData = NULL;
    const INT resultGetAddrInfo = getaddrinfo(KORL_C_CAST(PCSTR, ansiAddress.data), NULL, &hints, &addrInfoData);
    if(resultGetAddrInfo != 0)
    {
        korl_log(WARNING, "getaddrinfo failed! result=%i", resultGetAddrInfo);
        goto cleanUp_return_result;
    }
    KORL_ZERO_STACK(SOCKADDR_IN, socketAddressIpv4);
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
            const INT resultAddressToString = WSAAddressToStringW(addrIterator->ai_addr
                                                                 ,korl_checkCast_u$_to_u32(addrIterator->ai_addrlen)
                                                                 ,NULL, wcStrBufferIpAddress
                                                                 ,&wcStrBufferIpAddressLength);
            if(resultAddressToString == 0)
                korl_log(VERBOSE, "\t\tIPv6='%ws'", wcStrBufferIpAddress);
            else
                korl_log(VERBOSE, "\t\tIPv6; WSAAddressToString failed! result=%i", resultAddressToString);
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
        goto cleanUp_return_result;
    }
    /* @robustness: create a 'ipv4_to_kpl' conversion function */
    result.uInts[0] = ntohl(socketAddressIpv4.sin_addr.s_addr);
    cleanUp_return_result:
        if(resultGetAddrInfo == 0)
            freeaddrinfo(addrInfoData);
    return result;
}
korl_internal KORL_FUNCTION_korl_network_openUdp(korl_network_openUdp)
{
    _Korl_Network_Context*const context = &_korl_network_context;
    EnterCriticalSection(&context->criticalSection);
    /* find the first available unused socket handle */
    Korl_Network_Socket s = 0;
    for(; s < korl_arraySize(context->sockets); s++)
        if(context->sockets[s] == INVALID_SOCKET)
            break;
    if(s >= korl_arraySize(context->sockets))
    {
        s = 0;
        goto cleanUp_return_s;
    }
    /* create the winsock socket */
    context->sockets[s] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(context->sockets[s] == INVALID_SOCKET)
    {
        korl_log(ERROR, "Failed to open UDP socket! WSAGetLastError=%i", WSAGetLastError());
        s = 0;
        goto cleanUp_return_s;
    }
    /* bind the socket to ANY address */
    SOCKADDR_IN socketAddressIpv4;
    socketAddressIpv4.sin_family      = AF_INET;
    socketAddressIpv4.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddressIpv4.sin_port        = htons(listenPort);
    const int resultBind = bind(context->sockets[s], KORL_C_CAST(SOCKADDR*, &socketAddressIpv4), sizeof(socketAddressIpv4));
    if(resultBind == SOCKET_ERROR)
    {
        korl_log(ERROR, "Failed to bind UDP socket to port %i! WSAGetLastError=%i", listenPort, WSAGetLastError());
        korl_network_close(s);
        s = 0;
        goto cleanUp_return_s;
    }
    /* configure the socket to be NON-BLOCKING mode */
    {
        u_long nonBlockingEnabled = 1;// 1 == enabled, 0 == disabled
        const int resultSetNonBlocking = ioctlsocket(context->sockets[s], FIONBIO, &nonBlockingEnabled);
        if(resultSetNonBlocking != 0)
        {
            korl_log(ERROR, "Failed to enable socket non-blocking mode! WSAGetLastError=%i", WSAGetLastError());
            korl_network_close(s);
            s = 0;
            goto cleanUp_return_s;
        }
    }
    cleanUp_return_s:
        LeaveCriticalSection(&context->criticalSection);
        return _korl_network_makeSocket(s);
}
korl_internal KORL_FUNCTION_korl_network_close(korl_network_close)
{
    if(socket == 0)
        return;// silently do nothing if the socket is NULL
    _Korl_Network_Context*const context = &_korl_network_context;
    EnterCriticalSection(&context->criticalSection);
    const Korl_Network_Socket socketIndex = _korl_network_getSocketIndex(socket);
    korl_assert(context->sockets[socketIndex] != INVALID_SOCKET);
    const int resultCloseSocket = closesocket(context->sockets[socketIndex]);
    if(resultCloseSocket != 0)
        korl_log(ERROR, "Failed to close socket[%i]! WSAGetLastError=%i", socketIndex, WSAGetLastError());
    else
        context->sockets[socketIndex] = INVALID_SOCKET;
    cleanUp:
        LeaveCriticalSection(&context->criticalSection);
}
korl_internal KORL_FUNCTION_korl_network_send(korl_network_send)
{
    if(socket == 0)
        return 0;// silently do nothing if the socket is NULL
    _Korl_Network_Context*const context = &_korl_network_context;
    EnterCriticalSection(&context->criticalSection);
    const Korl_Network_Socket socketIndex = _korl_network_getSocketIndex(socket);
    korl_assert(context->sockets[socketIndex] != INVALID_SOCKET);
    i32 result = 0;
    if(dataBufferSize > KORL_NETWORK_MAX_DATAGRAM_BYTES)
    {
        korl_log(WARNING, "Attempting to send %i bytes of data where the max is %i!", dataBufferSize, KORL_NETWORK_MAX_DATAGRAM_BYTES);
        goto cleanUp;
    }
    /* send the data over the socket */
    _Korl_Network_WinSockAddress winSockAddressReceiver;
    const int winSockAddressLength = _korl_network_netAddressToNative(netAddressReceiver, netPortReceiver, &winSockAddressReceiver);
    result = sendto(context->sockets[socketIndex]
                   ,KORL_C_CAST(const char*, dataBuffer)
                   ,korl_checkCast_u$_to_i32(dataBufferSize), 0/*flags*/
                   ,&winSockAddressReceiver.sa, winSockAddressLength);
    if(result == SOCKET_ERROR)
    {
        korl_log(ERROR, "Network send error! WSAGetLastError=%i", WSAGetLastError());
        result = -1;
        goto cleanUp;
    }
    cleanUp:
        LeaveCriticalSection(&context->criticalSection);
        return result;
}
korl_internal KORL_FUNCTION_korl_network_receive(korl_network_receive)
{
    if(socket == 0)
        return 0;// silently do nothing if the socket is NULL
    _Korl_Network_Context*const context = &_korl_network_context;
    EnterCriticalSection(&context->criticalSection);
    const Korl_Network_Socket socketIndex = _korl_network_getSocketIndex(socket);
    korl_assert(context->sockets[socketIndex] != INVALID_SOCKET);
    /* receive data from the socket */
    _Korl_Network_WinSockAddress winSockAddressFrom = {0};
    int winSockAddressFromLength = sizeof(winSockAddressFrom);
    int result = recvfrom(context->sockets[socketIndex], KORL_C_CAST(char*, o_dataBuffer)
                         ,korl_checkCast_u$_to_i32(dataBufferSize), 0/*flags*/
                         ,&winSockAddressFrom.sa, &winSockAddressFromLength);
    if(result == SOCKET_ERROR)
    {
        const int winsockError = WSAGetLastError();
        switch(winsockError)
        {
        case WSAEWOULDBLOCK:
        case WSAECONNRESET:{
            /* On a UDP-datagram socket this error indicates a previous send 
                operation resulted in an ICMP Port Unreachable message. */
            result = 0;
            goto cleanUp;}
        }
        korl_log(ERROR, "Network receive error! WSAGetLastError=%i", winsockError);
        result = -1;
        goto cleanUp;
    }
    /* since we actually received data, we can now populate o_netAddressSender & 
        o_netPortSender with the proper address data */
    /*    @robustness: create a 'ipv4_to_kpl' conversion function */
    korl_assert(winSockAddressFromLength == sizeof(winSockAddressFrom.s4));
    korl_memory_zero(o_netAddressSender, sizeof(*o_netAddressSender));
    o_netAddressSender->uInts[0] = ntohl(winSockAddressFrom.s4.sin_addr.s_addr);
    *o_netPortSender = ntohs(winSockAddressFrom.s4.sin_port);
    cleanUp:
        LeaveCriticalSection(&context->criticalSection);
        return result;
}
