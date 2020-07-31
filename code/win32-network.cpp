#include "win32-network.h"
#include "win32-main.h"
#include <winsock2.h>
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
	if (LOBYTE(g_wsa.wVersion) != LOBYTE(DESIRED_WINSOCK_VERSION) 
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
internal KplSocketIndex w32NetworkOpenSocketUdp(u16 port)
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
	/* bind the socket to an address/port specification */
	sockaddr_in socketAddress;
	socketAddress.sin_family      = AF_INET;
	socketAddress.sin_addr.s_addr = INADDR_ANY;
	socketAddress.sin_port        = htons(port);
	const int resultBind = 
		bind(g_sockets[s], reinterpret_cast<sockaddr*>(&socketAddress), 
		     sizeof(socketAddress));
	if(resultBind == SOCKET_ERROR)
	{
		KLOG(ERROR, "Failed to bind UDP socket to port %i! WSAGetLastError=%i", 
		     port, WSAGetLastError());
		w32NetworkCloseSocket(s);
		return KPL_INVALID_SOCKET_INDEX;
	}
	return s;
}
internal void w32NetworkCloseSocket(KplSocketIndex socketIndex)
{
	EnterCriticalSection(&g_csLockNetworking);
	defer(LeaveCriticalSection(&g_csLockNetworking));
	kassert(g_sockets[socketIndex] != INVALID_SOCKET);
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