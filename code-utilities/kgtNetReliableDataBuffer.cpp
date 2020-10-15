#include "KgtNetReliableDataBuffer.h"
#include <cstring>
internal u16 kgtNetReliableDataBufferUsedBytes(
	KgtNetReliableDataBuffer* rdb, u8** o_reliableDataBufferCursor)
{
	u16 usedBytes = 0;
	u8* dataCursor = rdb->data + rdb->frontMessageByteOffset;
	const u8*const dataEnd = rdb->data + CARRAY_SIZE(rdb->data);
	for(u16 r = 0; r < rdb->messageCount; r++)
	/* for each reliable message, extract its byte length and move the buffer 
		cursor to the next contiguous message location */
	{
		/* extract message byte length */
		u8 messageBytesBuffer[2];
		for(size_t b = 0; b < CARRAY_SIZE(messageBytesBuffer); b++)
		{
			messageBytesBuffer[b] = dataCursor[0];
			dataCursor++;
			if(dataCursor >= dataEnd)
				dataCursor = rdb->data;
		}
		const u8* messageBytesBufferCursor = messageBytesBuffer;
		u16 messageByteCount;
		const u32 bytesUnpacked = 
			kutil::netUnpack(&messageByteCount, &messageBytesBufferCursor, 
			                 messageBytesBuffer + 
			                     CARRAY_SIZE(messageBytesBuffer));
		kassert(bytesUnpacked == sizeof(messageByteCount));
		/* move buffer cursor to next possible message location & add this 
			message size to the total used byte count */
		usedBytes += 
			sizeof(messageByteCount) + messageByteCount;
		dataCursor += messageByteCount;
		if(dataCursor >= dataEnd)
			dataCursor -= CARRAY_SIZE(rdb->data);
		kassert(dataCursor >= rdb->data);
		kassert(dataCursor < dataEnd);
	}
	kassert(usedBytes <= CARRAY_SIZE(rdb->data));
	if(o_reliableDataBufferCursor)
		*o_reliableDataBufferCursor = dataCursor;
	return usedBytes;
}
internal void kgtNetReliableDataBufferDequeue(
	KgtNetReliableDataBuffer* rdb, 
	u32 remoteReportedReliableMessageRollingIndex)
{
	if(rdb->messageCount == 0)
	/* ensure that the next reliable message sent to server will have a newer 
		rolling index than what the server is reporting */
	{
		rdb->frontMessageRollingIndex = 
			remoteReportedReliableMessageRollingIndex + 1;
		return;
	}
	if(remoteReportedReliableMessageRollingIndex < 
		rdb->frontMessageRollingIndex)
	/* the server has not confirmed receipt of our latest reliable messages, so 
		there is nothing to dequeue */
	{
		return;
	}
	/* remove reliable messages that the server has reported to have obtained so 
		that we stop sending copies to the server */
	kassert(remoteReportedReliableMessageRollingIndex < 
	            rdb->frontMessageRollingIndex + rdb->messageCount);
	const u8*      dataCursor = rdb->data + rdb->frontMessageByteOffset;
	const u8*const dataEnd    = rdb->data + CARRAY_SIZE(rdb->data);
	for(u32 rmi = rdb->frontMessageRollingIndex; 
		rmi <= remoteReportedReliableMessageRollingIndex; rmi++)
	{
		/* we can't unpack the message size using netUnpack API because the 
			reliableDataBuffer is circular, so the bytes can wrap around */
		u8 netPackedMessageSizeBuffer[2];
		for(size_t b = 0; b < CARRAY_SIZE(netPackedMessageSizeBuffer); b++)
		{
			netPackedMessageSizeBuffer[b] = dataCursor[0];
			dataCursor++;
			if(dataCursor >= dataEnd)
				dataCursor -= CARRAY_SIZE(rdb->data);
			kassert(dataCursor >= rdb->data);
			kassert(dataCursor < dataEnd);
		}
		const u8* netPackedMessageSizeBufferCursor = netPackedMessageSizeBuffer;
		u16 messageSize;
		const u32 bytesUnpacked = 
			kutil::netUnpack(&messageSize, &netPackedMessageSizeBufferCursor, 
			                 netPackedMessageSizeBuffer + 
			                     CARRAY_SIZE(netPackedMessageSizeBuffer));
		kassert(bytesUnpacked == sizeof(messageSize));
		/* now that we have the message size we can move the data cursor to the 
			next message location */
		dataCursor += messageSize;
		if(dataCursor >= dataEnd)
			dataCursor -= CARRAY_SIZE(rdb->data);
		kassert(dataCursor >= rdb->data);
		kassert(dataCursor < dataEnd);
		rdb->frontMessageByteOffset = 
			kmath::safeTruncateU16(dataCursor - rdb->data);
		rdb->messageCount--;
		rdb->frontMessageRollingIndex++;
//		KLOG(INFO, "confirmed reliable message[%i]", rmi);
	}
}
internal u32 kgtNetReliableDataBufferNetPack(
	KgtNetReliableDataBuffer* rdb, u8** dataCursor, const u8* dataEnd)
{
	u32 bytesPacked = 0;
	/* first, pack the datagram header & the # of reliable messages we're about 
		to send */
	bytesPacked += 
		kutil::netPack(rdb->frontMessageRollingIndex, dataCursor, dataEnd);
	bytesPacked += 
		kutil::netPack(rdb->messageCount, dataCursor, dataEnd);
	/* copy the reliable messages into the dataCursor */
	/* because reliable message buffer is circular, we need to handle two cases: 
		- reliableDataBufferFront <  reliableDataBufferCursor => one memcpy
		- reliableDataBufferFront >= reliableDataBufferCursor => two memcpys */
	u8* reliableDataBufferCursor = nullptr;
	const u16 reliableDataBufferUsedBytes = 
		kgtNetReliableDataBufferUsedBytes(rdb, &reliableDataBufferCursor);
	const u8* reliableDataBufferFront = rdb->data + rdb->frontMessageByteOffset;
	const u8*const reliableDataBufferEnd = rdb->data + CARRAY_SIZE(rdb->data);
	if(reliableDataBufferFront < reliableDataBufferCursor)
	{
		const u32 reliableBufferCopyBytes = kmath::safeTruncateU32(
			reliableDataBufferCursor - reliableDataBufferFront);
		memcpy(*dataCursor, reliableDataBufferFront, 
		       reliableBufferCopyBytes);
		*dataCursor  += reliableBufferCopyBytes;
		bytesPacked += reliableBufferCopyBytes;
	}
	else
	{
		const u32 reliableBufferCopyBytesA = kmath::safeTruncateU32(
			reliableDataBufferEnd - reliableDataBufferFront);
		const u32 reliableBufferCopyBytesB = kmath::safeTruncateU32(
			reliableDataBufferCursor - rdb->data);
		kassert(reliableBufferCopyBytesA + reliableBufferCopyBytesB == 
		        reliableDataBufferUsedBytes);
		memcpy(*dataCursor, reliableDataBufferFront, reliableBufferCopyBytesA);
		*dataCursor  += reliableBufferCopyBytesA;
		bytesPacked += reliableBufferCopyBytesA;
		memcpy(*dataCursor, rdb->data, reliableBufferCopyBytesB);
		*dataCursor  += reliableBufferCopyBytesB;
		bytesPacked += reliableBufferCopyBytesB;
	}
	return bytesPacked;
}
internal u32 kgtNetReliableDataBufferUnpackMeta(
	const u8** dataCursor, const u8* dataEnd, u32* o_frontMessageRollingIndex, 
	u16* o_reliableMessageCount)
{
	u32 bytesUnpacked = 0;
	/* extract range of reliable message rolling indices contained 
		in the packet */
	bytesUnpacked += 
		kutil::netUnpack(o_frontMessageRollingIndex, dataCursor, dataEnd);
	bytesUnpacked += 
		kutil::netUnpack(o_reliableMessageCount, dataCursor, dataEnd);
	return bytesUnpacked;
}
internal void kgtNetReliableDataBufferQueueMessage(
	KgtNetReliableDataBuffer* rdb, const u8* netPackedData, 
	u16 netPackedDataBytes)
{
	/* make sure that we have enough room in the circular buffer queue for 
		`dataBytes` + sizeof(dataBytes) worth of bytes */
	const u8*const reliableDataBufferEnd = rdb->data + CARRAY_SIZE(rdb->data);
	u8* reliableDataBufferCursor = nullptr;
	u16 reliableDataBufferUsedBytes = 
		kgtNetReliableDataBufferUsedBytes(rdb, &reliableDataBufferCursor);
	if(CARRAY_SIZE(rdb->data) - reliableDataBufferUsedBytes <
		(netPackedDataBytes + sizeof(netPackedDataBytes)))
	{
		KLOG(ERROR, "Reliable data buffer {%i/%i} overflow! "
		     "Cannot fit message size=%i!", reliableDataBufferUsedBytes, 
		     CARRAY_SIZE(rdb->data), 
		     netPackedDataBytes + sizeof(netPackedDataBytes));
	}
	/* each reliable message begins with a number representing the length of the 
		message */
	u8 netPackedDataBytesBuffer[2];
	u8* netPackedDataBytesBufferCursor = netPackedDataBytesBuffer;
	const u32 packedDataBytesBytes = 
		kutil::netPack(netPackedDataBytes, &netPackedDataBytesBufferCursor, 
		               netPackedDataBytesBuffer + 
		                   CARRAY_SIZE(netPackedDataBytesBuffer));
	kassert(packedDataBytesBytes == 2);
	/* now that we have `dataBytes` in network-byte-order, we can add them all 
		to the circular buffer queue */
	for(size_t c = 0; c < CARRAY_SIZE(netPackedDataBytesBuffer); c++)
	{
		reliableDataBufferCursor[0] = netPackedDataBytesBuffer[c];
		reliableDataBufferCursor++;
		if(reliableDataBufferCursor >= reliableDataBufferEnd)
			reliableDataBufferCursor -= CARRAY_SIZE(rdb->data);
		kassert(reliableDataBufferCursor >= rdb->data);
		kassert(reliableDataBufferCursor < reliableDataBufferEnd);
	}
	for(u16 b = 0; b < netPackedDataBytes; b++)
	{
		reliableDataBufferCursor[0] = netPackedData[b];
		reliableDataBufferCursor++;
		if(reliableDataBufferCursor >= reliableDataBufferEnd)
			reliableDataBufferCursor -= CARRAY_SIZE(rdb->data);
		kassert(reliableDataBufferCursor >= rdb->data);
		kassert(reliableDataBufferCursor < reliableDataBufferEnd);
	}
	/* finally, update the rest of the KgtNetClient's reliable message queue 
		state to reflect our newly added message.  For now, all we have to do is 
		increment the message count, since the other information is all 
		calculated on the fly.  */
	rdb->messageCount++;
}
