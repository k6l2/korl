#pragma once
#include "kutil.h"
internal bool w32InitializeNetwork();
/** 
 * @param address if this is nullptr, the socket is bound to ANY address
 * @return KPL_INVALID_SOCKET_INDEX if the socket could not be created for any 
 *         reason
*/
internal KplSocketIndex w32NetworkOpenSocketUdp(u16 port, const char* address);
internal void w32NetworkCloseSocket(KplSocketIndex socketIndex);