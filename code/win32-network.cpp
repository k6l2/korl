#include "win32-network.h"
#include "win32-main.h"
#include <winsock2.h>
#include <WS2tcpip.h>
#include "platform-game-interfaces.h"
global_variable WSADATA g_wsa;
global_variable SOCKET g_sockets[32];
static_assert(CARRAY_SIZE(g_sockets) <= KPL_INVALID_SOCKET_INDEX);
global_variable CRITICAL_SECTION g_csLockNetworking;
internal bool w32InitializeNetwork()
{
	local_persist const WORD DESIRED_WINSOCK_VERSION = MAKEWORD(2,2);
	InitializeCriticalSection(&g_csLockNetworking);
	for(u8 s = 0; s < CARRAY_SIZE(g_sockets); s++)
	{
		g_sockets[s] = INVALID_SOCKET;
	}
	const int resultWsaStartup = WSAStartup(DESIRED_WINSOCK_VERSION, &g_wsa);
	if(resultWsaStartup != 0)
	{
		KLOG(ERROR, "Failed to start WinSock!");
		return false;
	}
	if(   LOBYTE(g_wsa.wVersion) != LOBYTE(DESIRED_WINSOCK_VERSION) 
	   || HIBYTE(g_wsa.wVersion) != HIBYTE(DESIRED_WINSOCK_VERSION))
	{
		KLOG(ERROR, "Could not find winsock v%i.%i compatible DLL!", 
		     HIBYTE(DESIRED_WINSOCK_VERSION), LOBYTE(DESIRED_WINSOCK_VERSION));
		WSACleanup();
		/* @TODO: figure out if I ever need to call WSACleanup anywhere else in 
			the program (probably not?..) */
		return false;
	}
	return true;
}
internal PLATFORM_SOCKET_OPEN_UDP(w32PlatformNetworkOpenSocketUdp)
{
	EnterCriticalSection(&g_csLockNetworking);
	defer(LeaveCriticalSection(&g_csLockNetworking));
	/* find the first available unused socket handle */
	KplSocketIndex s = 0;
	for(; s < CARRAY_SIZE(g_sockets); s++)
	{
		if(g_sockets[s] == INVALID_SOCKET)
			break;
	}
	if(s >= CARRAY_SIZE(g_sockets))
		return KPL_INVALID_SOCKET_INDEX;
	/* create the winsock socket */
	g_sockets[s] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(g_sockets[s] == INVALID_SOCKET)
	{
		KLOG(ERROR, "Failed to open UDP socket! WSAGetLastError=%i", 
		     WSAGetLastError());
		return KPL_INVALID_SOCKET_INDEX;
	}
	/* bind the socket to ANY address */
	sockaddr_in socketAddressIpv4;
	socketAddressIpv4.sin_family      = AF_INET;
	socketAddressIpv4.sin_addr.s_addr = htonl(INADDR_ANY);
	socketAddressIpv4.sin_port        = htons(listenPort);
	const int resultBind = 
		bind(g_sockets[s], reinterpret_cast<sockaddr*>(&socketAddressIpv4), 
		     sizeof(socketAddressIpv4));
	if(resultBind == SOCKET_ERROR)
	{
		KLOG(ERROR, "Failed to bind UDP socket to port %i! WSAGetLastError=%i", 
		     listenPort, WSAGetLastError());
		w32PlatformNetworkCloseSocket(s);
		return KPL_INVALID_SOCKET_INDEX;
	}
	/* configure the socket to be NON-BLOCKING mode - 
		1 == enabled, 0 == disabled*/
	{
		u_long nonBlockingEnabled = 1;
		const int resultSetNonBlocking = 
			ioctlsocket(g_sockets[s], FIONBIO, &nonBlockingEnabled);
		if(resultSetNonBlocking != 0)
		{
			KLOG(ERROR, "Failed to enable socket non-blocking mode! "
			     "WSAGetLastError=%i", WSAGetLastError());
			w32PlatformNetworkCloseSocket(s);
			return KPL_INVALID_SOCKET_INDEX;
		}
	}
	return s;
}
internal PLATFORM_SOCKET_CLOSE(w32PlatformNetworkCloseSocket)
{
	EnterCriticalSection(&g_csLockNetworking);
	defer(LeaveCriticalSection(&g_csLockNetworking));
	korlAssert(socketIndex < CARRAY_SIZE(g_sockets));
	korlAssert(g_sockets[socketIndex] != INVALID_SOCKET);
	const int resultCloseSocket = closesocket(g_sockets[socketIndex]);
	if(resultCloseSocket != 0)
	{
		KLOG(ERROR, "Failed to close socket[%i]! WSAGetLastError=%i", 
		     socketIndex, WSAGetLastError());
	}
	else
	{
		g_sockets[socketIndex] = INVALID_SOCKET;
	}
}
union WinSockAddress
{
	struct sockaddr         sa;
	struct sockaddr_in      s4;
	struct sockaddr_in6     s6;
	struct sockaddr_storage ss;
};
/**
 * @return the length of the winsock sockaddr structure
 */
internal int w32KplNetAddressToNative(const KplNetAddress& netAddress, u16 port, 
                                      WinSockAddress* o_winSockAddress)
{
	memset(o_winSockAddress, 0, sizeof(*o_winSockAddress));
	o_winSockAddress->s4.sin_family      = AF_INET;
	o_winSockAddress->s4.sin_addr.s_addr = htonl(netAddress.uInts[0]);
	o_winSockAddress->s4.sin_port        = htons(port);
	return sizeof(o_winSockAddress->s4);
}
internal PLATFORM_SOCKET_SEND(w32PlatformNetworkSend)
{
	EnterCriticalSection(&g_csLockNetworking);
	defer(LeaveCriticalSection(&g_csLockNetworking));
	korlAssert(socketIndex < CARRAY_SIZE(g_sockets));
	korlAssert(g_sockets[socketIndex] != INVALID_SOCKET);
	if(dataBufferSize > KPL_MAX_DATAGRAM_SIZE)
	{
		KLOG(WARNING, "Attempting to send %i bytes of data where the max is "
		     "%i!", dataBufferSize, KPL_MAX_DATAGRAM_SIZE);
		return 0;
	}
	/* send the data over the socket */
	WinSockAddress winSockAddressReceiver;
	const int winSockAddressLength = 
		w32KplNetAddressToNative(netAddressReceiver, netPortReceiver, 
		                         &winSockAddressReceiver);
	const int resultSendTo = 
		sendto(g_sockets[socketIndex], 
		       reinterpret_cast<const char*>(dataBuffer), 
		       kmath::safeTruncateI32(dataBufferSize), 0/*flags*/, 
		       &winSockAddressReceiver.sa, winSockAddressLength);
	if(resultSendTo == SOCKET_ERROR)
	{
		KLOG(ERROR, "Network send error! WSAGetLastError=%i", 
		     WSAGetLastError());
		return -1;
	}
	return resultSendTo;
}
internal PLATFORM_SOCKET_RECEIVE(w32PlatformNetworkReceive)
{
	EnterCriticalSection(&g_csLockNetworking);
	defer(LeaveCriticalSection(&g_csLockNetworking));
	korlAssert(socketIndex < CARRAY_SIZE(g_sockets));
	korlAssert(g_sockets[socketIndex] != INVALID_SOCKET);
	/* receive data from the socket */
	WinSockAddress winSockAddressFrom = {};
	int winSockAddressFromLength = sizeof(winSockAddressFrom);
	const int resultReceiveFrom = 
		recvfrom(g_sockets[socketIndex], reinterpret_cast<char*>(o_dataBuffer), 
		         kmath::safeTruncateI32(dataBufferSize), 0/*flags*/, 
		         &winSockAddressFrom.sa, &winSockAddressFromLength);
	if(resultReceiveFrom == SOCKET_ERROR)
	{
		const int winsockError = WSAGetLastError();
		switch(winsockError)
		{
			case WSAEWOULDBLOCK:{
				return 0;
			}break;
			case WSAECONNRESET:{
				/* On a UDP-datagram socket this error indicates a previous send 
					operation resulted in an ICMP Port Unreachable message. */
				return 0;
			}break;
		}
		KLOG(ERROR, "Network receive error! WSAGetLastError=%i", winsockError);
		return -1;
	}
	/* since we actually received data, we can now populate o_netAddressSender & 
		o_netPortSender with the proper address data */
	/*	@robustness: create a 'ipv4_to_kpl' conversion function */
	korlAssert(winSockAddressFromLength == sizeof(winSockAddressFrom.s4));
	memset(o_netAddressSender, 0, sizeof(*o_netAddressSender));
	o_netAddressSender->uInts[0] = ntohl(winSockAddressFrom.s4.sin_addr.s_addr);
	*o_netPortSender = ntohs(winSockAddressFrom.s4.sin_port);
	return resultReceiveFrom;
}
internal PLATFORM_NET_RESOLVE_ADDRESS(w32PlatformNetworkResolveAddress)
{
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	/* notice that the members of addrinfo here are essentially the same 
		datum that are passed to the `socket` function! */
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	addrinfo* addrInfoData;
	const INT resultGetAddrInfo = 
		getaddrinfo(ansiAddress, nullptr, &hints, &addrInfoData);
	if(resultGetAddrInfo != 0)
	{
		KLOG(WARNING, "getaddrinfo failed! result=%i", resultGetAddrInfo);
		return KPL_INVALID_ADDRESS;
	}
	defer(freeaddrinfo(addrInfoData));
	sockaddr_in socketAddressIpv4;
	socketAddressIpv4.sin_addr.s_addr = htonl(INADDR_ANY);
	u16 addrInfoDataIndex = 0;
	/* iterate over all the address infos associated with this address query 
		and select the first valid address we find (if it exists) */
	for(addrinfo* addrIterator = addrInfoData; addrIterator; 
		addrIterator = addrIterator->ai_next)
	{
		KLOG(INFO, "--- addrInfoData[%i] ---", addrInfoDataIndex++);
		KLOG(INFO, "\tflags: 0x%x", addrIterator->ai_flags);
		KLOG(INFO, "\tfamily:");
		switch(addrIterator->ai_family)
		{
			case AF_UNSPEC: {
				KLOG(INFO, "\t\tUnspecified");
			}break;
			case AF_INET: {
				char cStrBufferIpAddress[16];
				inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(
					      addrIterator->ai_addr)->sin_addr, cStrBufferIpAddress, 
					      CARRAY_SIZE(cStrBufferIpAddress));
				if(socketAddressIpv4.sin_addr.s_addr != htonl(INADDR_ANY))
				/* if we've already found a valid address, just print it out 
					for diagnostic purposes and keep going... */
				{
					KLOG(INFO, "\t\tIPv4='%s'", cStrBufferIpAddress);
					break;
				}
				/* just select the first valid address we find */
				socketAddressIpv4 = *reinterpret_cast<sockaddr_in*>(
					addrIterator->ai_addr);
				KLOG(INFO, "\t\tFIRST IPv4='%s'", cStrBufferIpAddress);
			}break;
			case AF_INET6: {
				WCHAR wcStrBufferIpAddress[46];
				DWORD wcStrBufferIpAddressLength = 
					CARRAY_SIZE(wcStrBufferIpAddress);
				const INT resultAddressToString = 
					WSAAddressToStringW(addrIterator->ai_addr, 
					                    kmath::safeTruncateI32(
					                        addrIterator->ai_addrlen), 
					                    nullptr, wcStrBufferIpAddress, 
					                    &wcStrBufferIpAddressLength);
				if(resultAddressToString == 0)
				{
					KLOG(INFO, "\t\tIPv6='%ws'", wcStrBufferIpAddress);
				}
				else
				{
					KLOG(INFO, "\t\tIPv6; WSAAddressToString failed! result=%i", 
					     resultAddressToString);
				}
			}break;
			case AF_NETBIOS: {
				KLOG(INFO, "\t\tNetBIOS");
			}break;
			default: {
				KLOG(INFO, "\t\tother (%i)", addrIterator->ai_family);
			}break;
		}
		KLOG(INFO, "\tSocket type: ");
		switch (addrIterator->ai_socktype) 
		{
			case 0:{
				KLOG(INFO, "\t\tUnspecified");
			}break;
			case SOCK_STREAM:{
				KLOG(INFO, "\t\tSOCK_STREAM (stream)");
			}break;
			case SOCK_DGRAM:{
				KLOG(INFO, "\t\tSOCK_DGRAM (datagram) ");
			}break;
			case SOCK_RAW:{
				KLOG(INFO, "\t\tSOCK_RAW (raw) ");
			}break;
			case SOCK_RDM:{
				KLOG(INFO, "\t\tSOCK_RDM (reliable message datagram)");
			}break;
			case SOCK_SEQPACKET:{
				KLOG(INFO, "\t\tSOCK_SEQPACKET (pseudo-stream packet)");
			}break;
			default:{
				KLOG(INFO, "\t\tOther %i", addrIterator->ai_socktype);
			}break;
		}
		KLOG(INFO, "\tProtocol: ");
		switch (addrIterator->ai_protocol) 
		{
			case 0:{
				KLOG(INFO, "\t\tUnspecified");
			}break;
			case IPPROTO_TCP:{
				KLOG(INFO, "\t\tIPPROTO_TCP (TCP)");
			}break;
			case IPPROTO_UDP:{
				KLOG(INFO, "\t\tIPPROTO_UDP (UDP) ");
			}break;
			default:{
				KLOG(INFO, "\t\tOther %i", addrIterator->ai_protocol);
			}break;
		}
		KLOG(INFO, "\tLength of this sockaddr: %ui", addrIterator->ai_addrlen);
		KLOG(INFO, "\tCanonical name: %s", addrIterator->ai_canonname);
	}
	if(socketAddressIpv4.sin_addr.s_addr == htonl(INADDR_ANY))
	{
		KLOG(ERROR, "Failed to resolve address! '%s'", ansiAddress);
		return KPL_INVALID_ADDRESS;
	}
	/* @robustness: create a 'ipv4_to_kpl' conversion function */
	KplNetAddress result = {};
	result.uInts[0] = ntohl(socketAddressIpv4.sin_addr.s_addr);
	return result;
}