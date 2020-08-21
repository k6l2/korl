#include "exampleGameNet.h"
#include "serverExample.h"
#include "KmlGameExample.h"
#include "kNetCommon.h"
internal u16 reliableMessageAnsiTextPack(
	const char* nullTerminatedAnsiText, u8* dataBuffer, u32 dataBufferSize)
{
	u8* reliableMessageCursor = dataBuffer;
	u32 reliableMessageBytes = 
		kutil::netPack(static_cast<u8>(
			ReliableMessageType::ANSI_TEXT_MESSAGE), 
			&reliableMessageCursor, dataBuffer, dataBufferSize);
	const size_t ansiStringLength = strlen(nullTerminatedAnsiText);
	reliableMessageBytes +=
		kutil::netPack(ansiStringLength, &reliableMessageCursor, 
		               dataBuffer, dataBufferSize);
	kassert(reliableMessageCursor + ansiStringLength < 
	        dataBuffer + dataBufferSize);
	for(size_t c = 0; c < ansiStringLength; c++)
	{
		reliableMessageCursor[0] = 
			*reinterpret_cast<const u8*>(&nullTerminatedAnsiText[c]);
		reliableMessageCursor++;
	}
	reliableMessageBytes += kmath::safeTruncateU32(ansiStringLength);
	return kmath::safeTruncateU16(reliableMessageBytes);
}
internal void reliableMessageAnsiTextUnpack(
	char* o_nullTerminatedAnsiText, size_t nullTerminatedAnsiTextSize, 
	const u8* dataBuffer, u32 dataBufferSize)
{
	const u8* dataBufferCursor = dataBuffer;
	const ReliableMessageType reliableMessageType = ReliableMessageType(
		kutil::netUnpackU8(&dataBufferCursor, dataBuffer, dataBufferSize));
	kassert(reliableMessageType == ReliableMessageType::ANSI_TEXT_MESSAGE);
	const size_t ansiStringLength = 
		kutil::netUnpackU64(&dataBufferCursor, dataBuffer, dataBufferSize);
	kassert(nullTerminatedAnsiTextSize >= ansiStringLength + 1);
	for(size_t c = 0; c < ansiStringLength; c++)
	{
		o_nullTerminatedAnsiText[c] = 
			*reinterpret_cast<const char*>(&dataBufferCursor[0]);
		dataBufferCursor++;
	}
	o_nullTerminatedAnsiText[ansiStringLength] = '\0';
}
internal u32 actorNetPack(const Actor& a, u8 **ppPacketBuffer, 
                          u8 *dataBuffer, u32 dataBufferSize)
{
	u32 bytesPacked = kutil::netPack(a.clientId, ppPacketBuffer, 
	                                 dataBuffer, dataBufferSize);
	for(size_t e = 0; e < CARRAY_SIZE(a.shipWorldPosition.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(a.shipWorldPosition.elements[e], ppPacketBuffer, 
			               dataBuffer, dataBufferSize);
	}
	for(size_t e = 0; e < CARRAY_SIZE(a.shipWorldOrientation.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(a.shipWorldOrientation.elements[e], ppPacketBuffer, 
			               dataBuffer, dataBufferSize);
	}
	return bytesPacked;
}
internal void actorNetUnpack(Actor* a, const u8 **ppPacketBuffer, 
                             const u8 *dataBuffer, u32 dataBufferSize)
{
	a->clientId = 
		kutil::netUnpackU16(ppPacketBuffer, dataBuffer, dataBufferSize);
	for(size_t e = 0; e < CARRAY_SIZE(a->shipWorldPosition.elements); e++)
	{
		a->shipWorldPosition.elements[e] = 
			kutil::netUnpackF32(ppPacketBuffer, dataBuffer, dataBufferSize);
	}
	for(size_t e = 0; e < CARRAY_SIZE(a->shipWorldOrientation.elements); e++)
	{
		a->shipWorldOrientation.elements[e] = 
			kutil::netUnpackF32(ppPacketBuffer, dataBuffer, dataBufferSize);
	}
}
internal u32 controlInputNetPack(const ClientControlInput& c, 
                                 u8 **ppPacketBuffer, 
                                 u8 *dataBuffer, u32 dataBufferSize)
{
	u32 bytesPacked = 0;
	bytesPacked += kutil::netPack(c.controlMoveVector.x, ppPacketBuffer, 
	                              dataBuffer, dataBufferSize);
	bytesPacked += kutil::netPack(c.controlMoveVector.y, ppPacketBuffer, 
	                              dataBuffer, dataBufferSize);
	bytesPacked += kutil::netPack(static_cast<u8>(c.controlResetPosition), 
	                              ppPacketBuffer, dataBuffer, dataBufferSize);
	return bytesPacked;
}
internal void controlInputNetUnpack(ClientControlInput* c, 
                                    const u8 **ppPacketBuffer, 
                                    const u8 *dataBuffer, u32 dataBufferSize)
{
	c->controlMoveVector.x = 
		kutil::netUnpackF32(ppPacketBuffer, dataBuffer, dataBufferSize);
	c->controlMoveVector.y = 
		kutil::netUnpackF32(ppPacketBuffer, dataBuffer, dataBufferSize);
	c->controlResetPosition = static_cast<bool>(
		kutil::netUnpackU8(ppPacketBuffer, dataBuffer, dataBufferSize));
}
internal K_NET_CLIENT_WRITE_STATE(gameWriteClientState)
{
	u32 bytesPacked = 0;
	u8* packet = packetBuffer;
	bytesPacked += controlInputNetPack(g_gs->controlInput, &packet, 
	                                   packetBuffer, packetBufferSize);
	return bytesPacked;
}
internal K_NET_CLIENT_READ_SERVER_STATE(gameClientReadServerState)
{
	const u8* packet = packetBuffer;
	const size_t netActorCount = 
		kutil::netUnpackU64(&packet, packetBuffer, packetBufferSize);
	arrsetlen(g_gs->actors, netActorCount);
	for(size_t a = 0; a < netActorCount; a++)
	{
		Actor& actor = g_gs->actors[a];
		actorNetUnpack(&actor, &packet, packetBuffer, packetBufferSize);
	}
}
internal K_NET_SERVER_READ_CLIENT_STATE(serverReadClient)
{
	ServerState*const ss = reinterpret_cast<ServerState*>(userPointer);
	/* search for the actor possessed by clientId in the ServerState */
	size_t clientActorIndex = 0;
	for(; clientActorIndex < CARRAY_SIZE(ss->actors); clientActorIndex++)
	{
		if(ss->actors[clientActorIndex].clientId == clientId)
		{
			break;
		}
	}
	kassert(clientActorIndex < CARRAY_SIZE(ss->actors));
	Actor& clientActor = ss->actors[clientActorIndex];
	/* at this point, we know that clientActorIndex is the correct index of the 
		actor possessed by clientId, so we can now populate this data with the 
		packet data received by the client~ */
	const u8* packet = packetBuffer;
	ClientControlInput clientControlInput;
	controlInputNetUnpack(&clientControlInput, &packet, 
	                      packetBuffer, packetBufferSize);
	/* apply input to client's possessed actor on the server */
	clientActor.shipWorldPosition.x += 
		10*clientControlInput.controlMoveVector.x;
	clientActor.shipWorldPosition.y += 
		10*clientControlInput.controlMoveVector.y;
	if(!kmath::isNearlyZero(clientControlInput.controlMoveVector.x) 
		|| !kmath::isNearlyZero(clientControlInput.controlMoveVector.y))
	{
		const f32 stickRadians =
			kmath::v2Radians(clientControlInput.controlMoveVector);
		clientActor.shipWorldOrientation =
			kQuaternion({0,0,1}, stickRadians - PI32/2);
	}
	if(clientControlInput.controlResetPosition)
	{
		clientActor.shipWorldPosition = {};
	}
}
internal K_NET_SERVER_WRITE_STATE(serverWriteState)
{
	ServerState*const ss = reinterpret_cast<ServerState*>(userPointer);
	size_t* arrayActorIndices = reinterpret_cast<size_t*>(
		arrinit(sizeof(size_t), ss->hKgaPermanent, CARRAY_SIZE(ss->actors)));
	defer(arrfree(arrayActorIndices));
	for(size_t a = 0; a < CARRAY_SIZE(ss->actors); a++)
	/* accumulate a list of active actors */
	{
		if(ss->actors[a].clientId != network::SERVER_INVALID_CLIENT_ID)
			arrpush(arrayActorIndices, a);
	}
	/* pack all the active actors into the server state 
		@optimization: only pack actors the player is within range of */
	u8* packet = packetBuffer;
	u32 bytesPacked = 0;
	bytesPacked += kutil::netPack(arrlenu(arrayActorIndices), &packet, 
	                              packetBuffer, packetBufferSize);
	for(size_t aa = 0; aa < arrlenu(arrayActorIndices); aa++)
	{
		bytesPacked += 
			actorNetPack(ss->actors[arrayActorIndices[aa]], &packet, 
			             packetBuffer, packetBufferSize);
	}
	return bytesPacked;
}
internal K_NET_SERVER_ON_CLIENT_CONNECT(serverOnClientConnect)
{
	/* spawn the client's possessed actor if it hasn't already been spawned */
	ServerState*const ss = reinterpret_cast<ServerState*>(userPointer);
	size_t clientActorIndex = 0;
	for(clientActorIndex = 0; clientActorIndex < CARRAY_SIZE(ss->actors); 
		clientActorIndex++)
	{
		if(ss->actors[clientActorIndex].clientId == 
			network::SERVER_INVALID_CLIENT_ID)
		{
			break;
		}
	}
	kassert(clientActorIndex < CARRAY_SIZE(ss->actors));
	ss->actors[clientActorIndex] = {};
	ss->actors[clientActorIndex].clientId = clientId;
}
internal K_NET_SERVER_ON_CLIENT_DISCONNECT(serverOnClientDisconnect)
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
	ss->actors[clientActorIndex].clientId = network::SERVER_INVALID_CLIENT_ID;
}
internal K_NET_SERVER_READ_RELIABLE_MESSAGE(serverReadReliableMessage)
{
	/* peek at the `ReliableMessageType`, then read the data from it 
		appropriately */
	const ReliableMessageType reliableMessageType = 
		ReliableMessageType(netDataBuffer[0]);
	switch(reliableMessageType)
	{
		case ReliableMessageType::ANSI_TEXT_MESSAGE:{
			char ansiBuffer[256];
			reliableMessageAnsiTextUnpack(ansiBuffer, CARRAY_SIZE(ansiBuffer), 
				                          netDataBuffer, netDataBufferSize);
			KLOG(INFO, "SERVER: ANSI_TEXT_MESSAGE=\"%s\"", ansiBuffer);
		}break;
		default:{
			KLOG(ERROR, "Invalid reliable message type! (%i)", 
			     reliableMessageType);
		}break;
	}
}