#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "kGeneralAllocator.h"
#include "kAssetManager.h"
#include "kAudioMixer.h"
#include "kAllocatorLinear.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
#define STBDS_REALLOC(context,ptr,size) kStbDsRealloc(ptr,size,context)
#define STBDS_FREE(context,ptr)         kStbDsFree(ptr,context)
#define STBDS_ASSERT(x)                 g_kpl->assert(x)
#include "stb/stb_ds.h"
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize, 
                             void* context);
internal void kStbDsFree(void* allocatedAddress, void* context);
struct KmlTemplateGameState
{
	/* memory allocators */
	KgaHandle hKgaPermanent;
	KgaHandle hKgaTransient;
	KalHandle hKalFrame;
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
internal void templateGameState_onReloadCode(
	KmlTemplateGameState* tgs, GameMemory& memory);
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