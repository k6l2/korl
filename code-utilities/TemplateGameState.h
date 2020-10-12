#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "kAllocator.h"
#include "kAssetManager.h"
#include "kAudioMixer.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize, 
                             void* context);
internal void kStbDsFree(void* allocatedAddress, void* context);
struct KmlTemplateGameState
{
	/* memory allocators */
	KAllocatorHandle hKalPermanent;
	KAllocatorHandle hKalTransient;
	KAllocatorHandle hKalFrame;
	/* required state for KRB */
	krb::Context krbContext;
	/* useful utilities which almost every game will use */
	KAssetManager* assetManager;
	KAudioMixer* kAudioMixer;
};
/* useful global variables */
global_variable KmlPlatformApi* g_kpl;
global_variable KrbApi* g_krb;
/* template game state API */
internal void templateGameState_onReloadCode(GameMemory& memory);
/** 
 * Make sure to clear (zero-out) ALL game state memory, including the memory 
 * occupied by `tgs` BEFORE calling this function!!!
 */
internal void templateGameState_initialize(
	KmlTemplateGameState* tgs, GameMemory& memory, size_t totalGameStateSize);
internal void templateGameState_renderAudio(
	KmlTemplateGameState* tgs, GameAudioBuffer& audioBuffer, 
	u32 sampleBlocksConsumed);
/** @return true to request the platform continues to run, false to request the 
 *          platform closes the application */
internal bool templateGameState_updateAndDraw(
	KmlTemplateGameState* tgs, const GameKeyboard& gameKeyboard, 
	bool windowIsFocused);
/* convenience macros for our application */
#define ALLOC_FRAME_ARRAY(gs,type,elements) \
	reinterpret_cast<type*>(\
		kAllocAlloc((gs)->templateGameState.hKalFrame,sizeof(type)*(elements)))
