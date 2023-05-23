#pragma once
#include "korl-globalDefines.h"
/* IPv4 UDP datagrams cannot be larger than this amount.  Source:
https://en.wikipedia.org/wiki/User_Datagram_Protocol#:~:text=The%20field%20size%20sets%20a,%E2%88%92%2020%20byte%20IP%20header). */
#define KORL_NETWORK_MAX_DATAGRAM_BYTES 65507
typedef u8 Korl_Network_Socket;// 0 => invalid/uninitialized socket
typedef union Korl_Network_Address
{
    /* 16 bytes is enough to store an IPv6 address.  IPv4 addresses only use 4 
        bytes, but who cares?  Memory is free.™ */
    u8  uBytes[16];
    u32 uInts[4];
    u64 uLongs[2];
} Korl_Network_Address;
/** This function often takes a long time to complete, so it's probably best to 
 * run this as an asynchronous job */
#define KORL_FUNCTION_korl_network_resolveAddress(name) Korl_Network_Address name(acu8 ansiAddress)
/** 
 * \param listenPort If this is 0, the port which the socket listens on is 
 *                   chosen by the platform automatically.  This value does not 
 *                   prevent the user application from SENDING to other ports.
 * \param address if this is nullptr, the socket is bound to ANY address
 * \return \c 0 if the socket could not be created for any reason.  On success, 
 *         an opaque index to a unique resource representing a non-blocking UDP 
 *         socket bound to `listenPort` is returned.
 */
#define KORL_FUNCTION_korl_network_openUdp(name) Korl_Network_Socket name(u16 listenPort)
#define KORL_FUNCTION_korl_network_close(name) void name(Korl_Network_Socket socket)
/**
 * \return the # of bytes sent.  If an error occurs, a value < 0 is returned.  
 *         If the socket or the underlying platform networking implementation is 
 *         not ready to send right now, 0 is returned (no errors occurred)
 */
#define KORL_FUNCTION_korl_network_send(name) i32 name(Korl_Network_Socket socket\
                                                      ,const u8* dataBuffer, u$ dataBufferSize\
                                                      ,const Korl_Network_Address*const netAddressReceiver, u16 netPortReceiver)
/** 
 * \return (1) the # of elements written to o_dataBuffer  (2) the received data 
 *         into `o_dataBuffer` (3) the network address from which the data was 
 *         sent into `o_netAddressSender`.  If an error occurs, a value < 0 is 
 *         returned.
 */
#define KORL_FUNCTION_korl_network_receive(name) i32 name(Korl_Network_Socket socket\
                                                         ,u8* o_dataBuffer, u$ dataBufferSize\
                                                         ,Korl_Network_Address*const o_netAddressSender, u16*const o_netPortSender)
