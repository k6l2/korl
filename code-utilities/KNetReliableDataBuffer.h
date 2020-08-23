#pragma once
#include "kutil.h"
#include "kNetCommon.h"
#include "platform-game-interfaces.h"
struct KNetReliableDataBuffer
{
	/** treated as a circular buffer queue of network::ReliableMessage.  
	 * We subtract the sizes of 
	 * - `messageCount`
	 * - `frontMessageRollingIndex`
	 * - the size of a PacketType 
	 * for the reliable packet header to ensure that 
	 * the entire buffer will fit in a single datagram.  */
	u8 data[KPL_MAX_DATAGRAM_SIZE - sizeof(u16) - sizeof(u32) - 
	        sizeof(network::PacketType)];
	u16 messageCount;
	u16 frontMessageByteOffset;
	/** start at an index if 1 to ensure that the server's client rolling index 
	 * is out of date for the first reliable message we send, since on the 
	 * server we start at 0 */
	u32 frontMessageRollingIndex = 1;
};
/**
 * @param o_reliableDataBufferCursor 
 *        Return value for the position in the reliable data buffer immediately 
 *        following the last used byte in the circular buffer.  It should be 
 *        safe to start storing the next reliable message at this location.  If 
 *        this value == `nullptr`, then it is unused.
 */
internal u16 kNetReliableDataBufferUsedBytes(
	KNetReliableDataBuffer* rdb, u8** o_reliableDataBufferCursor);
internal void kNetReliableDataBufferDequeue(
	KNetReliableDataBuffer* rdb, u32 remoteReportedReliableMessageRollingIndex);
internal u32 kNetReliableDataBufferNetPack(
	KNetReliableDataBuffer* rdb, u8** dataCursor, const u8* dataEnd);
internal void kNetReliableDataBufferQueueMessage(
	KNetReliableDataBuffer* rdb, const u8* netPackedData, 
	u16 netPackedDataBytes);