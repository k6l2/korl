#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kgtNetCommon.h"
struct KgtNetReliableDataBuffer
{
	/** treated as a circular buffer queue of network::ReliableMessage.  
	 * We subtract the sizes of 
	 * - `messageCount`
	 * - `frontMessageRollingIndex`
	 * - the size of a PacketType 
	 * for the reliable packet header to ensure that 
	 * the entire buffer will fit in a single datagram.  */
	u8 data[KPL_MAX_DATAGRAM_SIZE - sizeof(u16) - sizeof(u32) - 
	        sizeof(kgtNet::PacketType)];
	u16 messageCount;
	u16 frontMessageByteOffset;
	/** start at an index if 1 to ensure that the remote's rolling index is out 
	 * of date for the first reliable message we send, since on remote we start 
	 * at 0 */
	u32 frontMessageRollingIndex = 1;
};
/**
 * @param o_reliableDataBufferCursor 
 *        Return value for the position in the reliable data buffer immediately 
 *        following the last used byte in the circular buffer.  It should be 
 *        safe to start storing the next reliable message at this location.  If 
 *        this value == `nullptr`, then it is unused.
 */
internal u16 
	kgtNetReliableDataBufferUsedBytes(
		KgtNetReliableDataBuffer* rdb, u8** o_reliableDataBufferCursor);
internal void 
	kgtNetReliableDataBufferDequeue(
		KgtNetReliableDataBuffer* rdb, 
		u32 remoteReportedReliableMessageRollingIndex);
internal u32 
	kgtNetReliableDataBufferNetPack(
		KgtNetReliableDataBuffer* rdb, u8** dataCursor, const u8* dataEnd);
internal u32 
	kgtNetReliableDataBufferUnpackMeta(
		const u8** dataCursor, const u8* dataEnd, 
		u32* o_frontMessageRollingIndex, u16* o_reliableMessageCount);
internal void 
	kgtNetReliableDataBufferQueueMessage(
		KgtNetReliableDataBuffer* rdb, const u8* netPackedData, 
		u16 netPackedDataBytes);
