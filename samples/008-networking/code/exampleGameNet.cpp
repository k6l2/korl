#include "exampleGameNet.h"
#include "serverExample.h"
#include "korlDynApp.h"
#include "kgtNetCommon.h"
internal u16 reliableMessageAnsiTextPack(
	const char* nullTerminatedAnsiText, u8* dataBuffer, const u8* dataBufferEnd)
{
	u32 reliableMessageBytes = 
		kutil::netPack(static_cast<u8>(ReliableMessageType::ANSI_TEXT_MESSAGE), 
			           &dataBuffer, dataBufferEnd);
	const size_t ansiStringLength = strlen(nullTerminatedAnsiText);
	reliableMessageBytes +=
		kutil::netPack(ansiStringLength, &dataBuffer, dataBufferEnd);
	kassert(dataBuffer + ansiStringLength < dataBufferEnd);
	for(size_t c = 0; c < ansiStringLength; c++)
	{
		dataBuffer[0] = *reinterpret_cast<const u8*>(
			&nullTerminatedAnsiText[c]);
		dataBuffer++;
	}
	reliableMessageBytes += kmath::safeTruncateU32(ansiStringLength);
	return kmath::safeTruncateU16(reliableMessageBytes);
}
internal u32 reliableMessageAnsiTextUnpack(
	char* o_nullTerminatedAnsiText, size_t nullTerminatedAnsiTextSize, 
	const u8* dataBuffer, const u8* dataBufferEnd)
{
	u32 unpackedBytes = 0;
	ReliableMessageType reliableMessageType;
	unpackedBytes += 
		kutil::netUnpack(reinterpret_cast<u8*>(&reliableMessageType), 
		                 &dataBuffer, dataBufferEnd);
	kassert(reliableMessageType == ReliableMessageType::ANSI_TEXT_MESSAGE);
	size_t ansiStringLength;
	unpackedBytes += 
		kutil::netUnpack(&ansiStringLength, &dataBuffer, dataBufferEnd);
	kassert(nullTerminatedAnsiTextSize >= ansiStringLength + 1);
	for(size_t c = 0; c < ansiStringLength; c++)
	{
		o_nullTerminatedAnsiText[c] = 
			*reinterpret_cast<const char*>(&dataBuffer[0]);
		dataBuffer++;
	}
	unpackedBytes += kmath::safeTruncateU32(ansiStringLength);
	o_nullTerminatedAnsiText[ansiStringLength] = '\0';
	return unpackedBytes;
}
internal u32 actorNetPack(const Actor& a, u8** dataCursor, 
                          const u8* dataCursorEnd)
{
	u32 bytesPacked = 
		kutil::netPack(a.clientId, dataCursor, dataCursorEnd);
	for(size_t e = 0; e < CARRAY_SIZE(a.shipWorldPosition.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(a.shipWorldPosition.elements[e], dataCursor, 
			               dataCursorEnd);
	}
	for(size_t e = 0; e < CARRAY_SIZE(a.shipWorldOrientation.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(a.shipWorldOrientation.elements[e], dataCursor, 
			               dataCursorEnd);
	}
	return bytesPacked;
}
internal u32 actorNetUnpack(Actor* a, const u8** dataCursor, 
                            const u8 *dataCursorEnd)
{
	u32 bytesUnpacked = 0;
	bytesUnpacked += 
		kutil::netUnpack(&a->clientId, dataCursor, dataCursorEnd);
	for(size_t e = 0; e < CARRAY_SIZE(a->shipWorldPosition.elements); e++)
	{
		bytesUnpacked += 
			kutil::netUnpack(&a->shipWorldPosition.elements[e], 
			                 dataCursor, dataCursorEnd);
	}
	for(size_t e = 0; e < CARRAY_SIZE(a->shipWorldOrientation.elements); e++)
	{
		bytesUnpacked += 
			kutil::netUnpack(&a->shipWorldOrientation.elements[e], dataCursor, 
			                 dataCursorEnd);
	}
	return bytesUnpacked;
}
internal u32 controlInputNetPack(const ClientControlInput& c, 
                                 u8** dataCursor, const u8* dataCursorEnd)
{
	u32 bytesPacked = 0;
	bytesPacked += kutil::netPack(c.controlMoveVector.x, dataCursor, 
	                              dataCursorEnd);
	bytesPacked += kutil::netPack(c.controlMoveVector.y, dataCursor, 
	                              dataCursorEnd);
	bytesPacked += kutil::netPack(static_cast<u8>(c.controlResetPosition), 
	                              dataCursor, dataCursorEnd);
	return bytesPacked;
}
internal u32 controlInputNetUnpack(ClientControlInput* c, const u8** dataCursor, 
                                   const u8* dataCursorEnd)
{
	u32 bytesUnpacked = 0;
	bytesUnpacked += 
		kutil::netUnpack(&c->controlMoveVector.x, dataCursor, dataCursorEnd);
	bytesUnpacked += 
		kutil::netUnpack(&c->controlMoveVector.y, dataCursor, dataCursorEnd);
	bytesUnpacked += 
		kutil::netUnpack(reinterpret_cast<u8*>(&c->controlResetPosition), 
		                 dataCursor, dataCursorEnd);
	return bytesUnpacked;
}
internal u16 reliableMessageClientControlInputPack(
	const ClientControlInput& cci, u8* o_data, const u8* dataEnd)
{
	u32 reliableMessageBytes = 
		kutil::netPack(static_cast<u8>(ReliableMessageType::CLIENT_INPUT_STATE), 
			           &o_data, dataEnd);
	reliableMessageBytes += controlInputNetPack(cci, &o_data, dataEnd);
	return kmath::safeTruncateU16(reliableMessageBytes);
}
internal u32 reliableMessageClientControlInputUnpack(
	ClientControlInput* o_cci, const u8* data, const u8* dataEnd)
{
	u32 unpackedBytes = 0;
	/* ensure that this is the correct type of reliable message! */
	{
		ReliableMessageType reliableMessageType;
		unpackedBytes += 
			kutil::netUnpack(reinterpret_cast<u8*>(&reliableMessageType), 
			                 &data, dataEnd);
		kassert(reliableMessageType == ReliableMessageType::CLIENT_INPUT_STATE);
	}
	unpackedBytes += controlInputNetUnpack(o_cci, &data, dataEnd);
	return unpackedBytes;
}
internal KGT_NET_CLIENT_WRITE_STATE(gameWriteClientState)
{
	/* if client ever needs to send unreliable data, pack it up here! */
	return 0;
}
internal KGT_NET_CLIENT_READ_SERVER_STATE(gameClientReadServerState)
{
	u32 bytesUnpacked = 0;
	size_t netActorCount;
	bytesUnpacked += kutil::netUnpack(&netActorCount, &data, dataEnd);
	arrsetlen(g_gs->actors, netActorCount);
	for(size_t a = 0; a < netActorCount; a++)
	{
		Actor& actor = g_gs->actors[a];
		bytesUnpacked += actorNetUnpack(&actor, &data, dataEnd);
	}
	kassert(data == dataEnd);
}
internal KGT_NET_SERVER_READ_CLIENT_STATE(serverReadClient)
{
	/* if client ever sends any unreliable data, unpack and interpret it 
		here! */
}
internal KGT_NET_SERVER_WRITE_STATE(serverWriteState)
{
	ServerState*const ss = reinterpret_cast<ServerState*>(userPointer);
	size_t* arrayActorIndices = arrinit(size_t, ss->hKalPermanent);
	defer(arrfree(arrayActorIndices));
	arrsetcap(arrayActorIndices, CARRAY_SIZE(ss->actors));
	for(size_t a = 0; a < CARRAY_SIZE(ss->actors); a++)
	/* accumulate a list of active actors */
	{
		if(ss->actors[a].clientId != kgtNet::SERVER_INVALID_CLIENT_ID)
			arrpush(arrayActorIndices, a);
	}
	/* pack all the active actors into the server state 
		@optimization: only pack actors the player is within range of */
	u32 bytesPacked = 0;
	bytesPacked += kutil::netPack(arrlenu(arrayActorIndices), &data, dataEnd);
	for(size_t aa = 0; aa < arrlenu(arrayActorIndices); aa++)
	{
		bytesPacked += 
			actorNetPack(ss->actors[arrayActorIndices[aa]], &data, dataEnd);
	}
	return bytesPacked;
}
internal KGT_NET_SERVER_ON_CLIENT_CONNECT(serverOnClientConnect)
{
	/* spawn the client's possessed actor if it hasn't already been spawned */
	ServerState*const ss = reinterpret_cast<ServerState*>(userPointer);
	size_t clientActorIndex = 0;
	for(clientActorIndex = 0; clientActorIndex < CARRAY_SIZE(ss->actors); 
		clientActorIndex++)
	{
		if(ss->actors[clientActorIndex].clientId == 
			kgtNet::SERVER_INVALID_CLIENT_ID)
		{
			break;
		}
	}
	kassert(clientActorIndex < CARRAY_SIZE(ss->actors));
	ss->actors[clientActorIndex] = {};
	ss->actors[clientActorIndex].clientId = clientId;
}
internal KGT_NET_SERVER_ON_CLIENT_DISCONNECT(serverOnClientDisconnect)
{
	/* "erase" the client's possessed actor */
	ServerState*const ss = reinterpret_cast<ServerState*>(userPointer);
	size_t clientActorIndex = 0;
	for(clientActorIndex = 0; clientActorIndex < CARRAY_SIZE(ss->actors); 
		clientActorIndex++)
	{
		if(ss->actors[clientActorIndex].clientId == clientId)
		{
			break;
		}
	}
	kassert(clientActorIndex < CARRAY_SIZE(ss->actors));
	ss->actors[clientActorIndex].clientId = kgtNet::SERVER_INVALID_CLIENT_ID;
}
internal KGT_NET_SERVER_READ_RELIABLE_MESSAGE(serverReadReliableMessage)
{
	ServerState*const ss = reinterpret_cast<ServerState*>(userPointer);
	/* peek at the `ReliableMessageType`, then read the data from it 
		appropriately */
	const ReliableMessageType reliableMessageType = 
		ReliableMessageType(netData[0]);
	u32 bytesUnpacked = 0;
	switch(reliableMessageType)
	{
		case ReliableMessageType::CLIENT_INPUT_STATE:{
			/* unpack the client input from the reliable message payload */
			ClientControlInput cci;
			bytesUnpacked += reliableMessageClientControlInputUnpack(
				&cci, netData, netDataEnd);
			/* search for the actor possessed by clientId in the ServerState */
			size_t clientActorIndex = 0;
			for(; clientActorIndex < CARRAY_SIZE(ss->actors); 
				clientActorIndex++)
			{
				if(ss->actors[clientActorIndex].clientId == clientId)
				{
					break;
				}
			}
			if(clientActorIndex >= CARRAY_SIZE(ss->actors))
				break;
			/* at this point, we know that clientActorIndex is the correct index 
				of the actor possessed by clientId, so we can now populate this 
				data with the packet data received by the client~ */
			Actor& clientActor = ss->actors[clientActorIndex];
			/* apply input to client's possessed actor on the server */
			clientActor.shipWorldPosition.x += 10*cci.controlMoveVector.x;
			clientActor.shipWorldPosition.y += 10*cci.controlMoveVector.y;
			if(    !kmath::isNearlyZero(cci.controlMoveVector.x) 
			    || !kmath::isNearlyZero(cci.controlMoveVector.y))
			{
				const f32 stickRadians =
					kmath::v2Radians(cci.controlMoveVector);
				clientActor.shipWorldOrientation =
					{{0,0,1}, stickRadians - PI32/2};
			}
			if(cci.controlResetPosition)
			{
				clientActor.shipWorldPosition = {};
			}
		}break;
		case ReliableMessageType::ANSI_TEXT_MESSAGE:{
#if 0
			char ansiBuffer[256];
			bytesUnpacked += 
				reliableMessageAnsiTextUnpack(
					ansiBuffer, CARRAY_SIZE(ansiBuffer), netData, netDataEnd);
			KLOG(INFO, "SERVER: ANSI_TEXT_MESSAGE=\"%s\"", ansiBuffer);
#endif// 0
			/* simply broadcast the text message back to all other clients~ */
			const u16 netDataBytes = 
				kmath::safeTruncateU16(netDataEnd - netData);
			kgtNetServerQueueReliableMessage(
				&ss->kNetServer, netData, netDataBytes, clientId);
			bytesUnpacked += netDataBytes;
		}break;
		default:{
			KLOG(ERROR, "Invalid reliable message type! (%i)", 
			     reliableMessageType);
		}break;
	}
	kassert(bytesUnpacked == (netDataEnd - netData));
	return bytesUnpacked;
}
internal KGT_NET_CLIENT_READ_RELIABLE_MESSAGE(gameClientReadReliableMessage)
{
	/* peek at the `ReliableMessageType`, then read the data from it 
		appropriately */
	const ReliableMessageType reliableMessageType = 
		ReliableMessageType(data[0]);
	u32 bytesUnpacked = 0;
	switch(reliableMessageType)
	{
		case ReliableMessageType::ANSI_TEXT_MESSAGE:
		{
			char ansiBuffer[256];
			bytesUnpacked += 
				reliableMessageAnsiTextUnpack(
					ansiBuffer, CARRAY_SIZE(ansiBuffer), data, dataEnd);
			KLOG(INFO, "CLIENT: ANSI_TEXT_MESSAGE=\"%s\"", ansiBuffer);
		}break;
		case ReliableMessageType::CLIENT_INPUT_STATE:
		default:
		{
			KLOG(ERROR, "Invalid SERVER=>CLIENT reliable message!");
		}break;
	}
	return bytesUnpacked;
}
