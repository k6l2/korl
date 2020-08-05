#pragma once
#include "kutil.h"
internal bool w32InitializeNetwork();
/** 
 * @param address if this is nullptr, the socket is bound to ANY address
 * @return KPL_INVALID_SOCKET_INDEX if the socket could not be created for any 
 *         reason
*/
internal KplSocketIndex w32NetworkOpenSocketUdp(u16 port);
internal void w32NetworkCloseSocket(KplSocketIndex socketIndex);
/**
 * @return the # of bytes sent.  If an error occurs, a value < 0 is returned.  
 *         If the socket or underlying winsock system is not ready to send right 
 *         now, 0 is returned (no errors occurred)
 */
internal i32 w32NetworkSend(KplSocketIndex socketIndex, 
                            const u8* dataBuffer, size_t dataBufferSize, 
                            const KplNetAddress& netAddressReceiver, 
                            u16 netPortReceiver);
/**
 * @return the # of bytes received.
 */
internal size_t w32NetworkReceive(KplSocketIndex socketIndex, 
                                  u8* o_dataBuffer, size_t dataBufferSize, 
                                  KplNetAddress* o_netAddressSource, 
                                  u16* o_netPortSource);
internal KplNetAddress w32NetworkResolveAddress(const char* ansiAddress);