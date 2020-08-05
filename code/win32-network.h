#pragma once
#include "kutil.h"
internal bool w32InitializeNetwork();
internal PLATFORM_SOCKET_OPEN_UDP(w32NetworkOpenSocketUdp);
internal PLATFORM_SOCKET_CLOSE(w32NetworkCloseSocket);
internal PLATFORM_SOCKET_SEND(w32NetworkSend);
internal PLATFORM_SOCKET_RECEIVE(w32NetworkReceive);
internal PLATFORM_NET_RESOLVE_ADDRESS(w32NetworkResolveAddress);