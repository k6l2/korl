#include "exampleGameNet.h"
#include "serverExample.h"
#include "KmlGameExample.h"
internal u32 actorNetPack(Actor* a, u8 **ppPacketBuffer, 
                          u8 *dataBuffer, u32 dataBufferSize)
{
	u32 bytesPacked = kutil::netPack(a->clientId, ppPacketBuffer, 
	                                 dataBuffer, dataBufferSize);
	for(size_t e = 0; e < CARRAY_SIZE(a->shipWorldPosition.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(a->shipWorldPosition.elements[e], ppPacketBuffer, 
			               dataBuffer, dataBufferSize);
	}
	for(size_t e = 0; e < CARRAY_SIZE(a->shipWorldOrientation.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(a->shipWorldOrientation.elements[e], ppPacketBuffer, 
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
internal K_NET_CLIENT_WRITE_STATE(gameWriteClientState)
{
	u32 bytesPacked = 0;
	u8* packet = packetBuffer;
	///@TODO: replace this crap with actorNetPack, or just send input instead of 
	///       client actor state
	bytesPacked += 
		kutil::netPack(u16(0), &packet, packetBuffer, packetBufferSize);
	for(size_t e = 0; e < CARRAY_SIZE(g_gs->shipWorldPosition.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(g_gs->shipWorldPosition.elements[e], &packet, 
			               packetBuffer, packetBufferSize);
	}
	for(size_t e = 0; e < CARRAY_SIZE(g_gs->shipWorldOrientation.elements); e++)
	{
		bytesPacked += 
			kutil::netPack(g_gs->shipWorldOrientation.elements[e], &packet, 
			               packetBuffer, packetBufferSize);
	}
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
	/* at this point, we know that clientActorIndex is the correct index of the 
		actor possessed by clientId, so we can now populate this data with the 
		packet data received by the client~ */
	const u8* packet = packetBuffer;
	const u16 preservedClientId = ss->actors[clientActorIndex].clientId;
	actorNetUnpack(&ss->actors[clientActorIndex], &packet, 
	               packetBuffer, packetBufferSize);
	ss->actors[clientActorIndex].clientId = preservedClientId;
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
		if(ss->actors[a].clientId != K_NET_SERVER_INVALID_CLIENT_ID)
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
			actorNetPack(&ss->actors[arrayActorIndices[aa]], &packet, 
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
			K_NET_SERVER_INVALID_CLIENT_ID)
		{
			break;
		}
	}
	kassert(clientActorIndex < CARRAY_SIZE(ss->actors));
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
	ss->actors[clientActorIndex].clientId = K_NET_SERVER_INVALID_CLIENT_ID;
}