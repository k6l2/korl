#pragma once
#include "kutil.h"
internal bool w32InitializeNetwork();
internal PLATFORM_SOCKET_OPEN_UDP(w32PlatformNetworkOpenSocketUdp);
internal PLATFORM_SOCKET_CLOSE(w32PlatformNetworkCloseSocket);
internal PLATFORM_SOCKET_SEND(w32PlatformNetworkSend);
internal PLATFORM_SOCKET_RECEIVE(w32PlatformNetworkReceive);
internal PLATFORM_NET_RESOLVE_ADDRESS(w32PlatformNetworkResolveAddress);