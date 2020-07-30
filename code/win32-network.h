#pragma once
#include "kutil.h"
internal bool w32InitializeNetwork();
/** 
 * @return KPL_INVALID_SOCKET_INDEX if the socket could not be created for any 
 *         reason
*/
internal KplSocketIndex w32NetworkOpenSocketUdp(u16 port);
internal void w32NetworkCloseSocket(KplSocketIndex socketIndex);