#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "kgtAllocator.h"
#include "kgtAssetManager.h"
#if SEPARATE_ASSET_MODULES_COMPLETE
#include "kgtAudioMixer.h"
#include "kgtDraw.h"
#endif// SEPARATE_ASSET_MODULES_COMPLETE
#include "kgtVertex.h"
#include "kgtFlipBook.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
internal void* kStbDsRealloc(
	void* allocatedAddress, size_t newAllocationSize, void* context);
internal void kStbDsFree(void* allocatedAddress, void* context);
struct KgtGameState
{
	/* memory allocators */
	KgtAllocatorHandle hKalPermanent;
	KgtAllocatorHandle hKalTransient;
	KgtAllocatorHandle hKalFrame;
	/* required state for KRB */
	krb::Context krbContext;
	/* useful utilities which almost every game will use */
	KgtAssetManager* assetManager;
#if SEPARATE_ASSET_MODULES_COMPLETE
	KgtAudioMixer* audioMixer;
#endif// SEPARATE_ASSET_MODULES_COMPLETE
};
/* useful global variables */
global_variable KorlPlatformApi* g_kpl;
global_variable KrbApi* g_krb;
global_variable KgtAssetManager* g_kam;
global_variable KgtGameState* g_kgs;
/* KGT game state API */
internal void 
	kgtGameStateOnReloadCode(KgtGameState* kgs, GameMemory& memory);
/** 
 * Make sure to clear (zero-out) ALL game state memory, including the memory 
 * occupied by `tgs` BEFORE calling this function!!!
 */
internal void 
	kgtGameStateInitialize(
		GameMemory& memory, size_t totalGameStateSize, 
		size_t frameAllocatorBytes = kmath::megabytes(5));
internal void 
	kgtGameStateRenderAudio(
		GameAudioBuffer& audioBuffer, u32 sampleBlocksConsumed);
/** @return 
 * true to request the platform continues to run, false to request the platform 
 * closes the application */
internal bool 
	kgtGameStateUpdateAndDraw(
		const GameKeyboard& gameKeyboard, bool windowIsFocused);
/* convenience macros for our application */
#define KGT_ALLOC(bytes) \
	kgtAllocAlloc(g_kgs->hKalPermanent, bytes)
#define KGT_FREE(address) \
	kgtAllocFree(g_kgs->hKalPermanent, address)
#define KGT_ALLOC_FRAME_ARRAY(type,elements) \
	reinterpret_cast<type*>(\
		kgtAllocAlloc(g_kgs->hKalFrame, sizeof(type)*(elements)))
#define KGT_FREE_FRAME(address) \
	kgtAllocFree(g_kgs->hKalFrame, address)