#include "kgtGameState.h"
internal void kgtGameStateOnReloadCode(GameMemory& memory)
{
	g_kpl          = &memory.kpl;
	g_krb          = &memory.krb;
	platformLog    = memory.kpl.log;
	platformAssert = memory.kpl.assert;
	/* ImGui support */
	ImGui::SetCurrentContext(
		reinterpret_cast<ImGuiContext*>(memory.imguiContext));
	ImGui::SetAllocatorFunctions(memory.platformImguiAlloc, 
	                             memory.platformImguiFree, 
	                             memory.imguiAllocUserData);
}
internal void kgtGameStateInitialize(
	KgtGameState* kgs, GameMemory& memory, size_t totalGameStateSize)
{
	kassert(totalGameStateSize <= memory.permanentMemoryBytes);
	g_kgs = kgs;
	/* Tell KRB where it can safely store its CPU-side internal state.  We only 
		ever need to do this one time because *tgs should be in an immutable 
		spot in memory forever */
	g_krb->setCurrentContext(&kgs->krbContext);
	// initialize dynamic allocators //
	kgs->hKalPermanent = kgtAllocInit(
		KgtAllocatorType::GENERAL, 
		reinterpret_cast<u8*>(memory.permanentMemory) + totalGameStateSize, 
		memory.permanentMemoryBytes - totalGameStateSize);
	kgs->hKalTransient = kgtAllocInit(
		KgtAllocatorType::GENERAL, 
		memory.transientMemory, memory.transientMemoryBytes);
	// construct a linear frame allocator //
	{
		local_persist const size_t FRAME_ALLOC_BYTES = kmath::megabytes(5);
		void*const kalFrameStartAddress = 
			kgtAllocAlloc(kgs->hKalPermanent, FRAME_ALLOC_BYTES);
		kgs->hKalFrame = kgtAllocInit(
			KgtAllocatorType::LINEAR, kalFrameStartAddress, FRAME_ALLOC_BYTES);
	}
	// Contruct/Initialize the game's AssetManager //
	kgs->assetManager = kgtAssetManagerConstruct(
		kgs->hKalPermanent, KGT_ASSET_COUNT, kgs->hKalTransient, g_krb);
	g_kam = kgs->assetManager;
	// Initialize the game's audio mixer //
	kgs->audioMixer = kgtAudioMixerConstruct(kgs->hKalPermanent, 16);
	// Tell the asset manager to load assets asynchronously! //
	kgtAssetManagerPushAllKgtAssets(kgs->assetManager);
}
internal void kgtGameStateRenderAudio(
	KgtGameState* kgs, GameAudioBuffer& audioBuffer, u32 sampleBlocksConsumed)
{
	kgtAudioMixerMix(kgs->audioMixer, audioBuffer, sampleBlocksConsumed);
}
internal bool kgtGameStateUpdateAndDraw(
	KgtGameState* kgs, const GameKeyboard& gameKeyboard, bool windowIsFocused)
{
	kgtAllocReset(kgs->hKalFrame);
	/* Esc, Ctrl+W & Alt+F4 shortcuts to quickly exit the program */
	if(gameKeyboard.escape == ButtonState::PRESSED 
	   || (   gameKeyboard.f4 == ButtonState::PRESSED 
	       && gameKeyboard.modifiers.alt)
	   || (   gameKeyboard.w == ButtonState::PRESSED 
	       && gameKeyboard.modifiers.control))
	{
		if(windowIsFocused)
			return false;
	}
	/* easy fullscreen shortcuts */
	if(gameKeyboard.f11 == ButtonState::PRESSED 
	   || (   gameKeyboard.enter == ButtonState::PRESSED 
	       && gameKeyboard.modifiers.alt))
	{
		g_kpl->setFullscreen(!g_kpl->isFullscreen());
	}
	/* hot-reload all assets which have been reported to be changed by the 
		platform layer (newer file write timestamp) */
	if(kgtAssetManagerUnloadChangedAssets(kgs->assetManager))
		kgtAssetManagerPushAllKgtAssets(kgs->assetManager);
	return true;
}
#pragma warning( push )
	// warning C4127: conditional expression is constant
	#pragma warning( disable : 4127 )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4577: 'noexcept' used with no exception handling mode 
	//                specified...
	#pragma warning( disable : 4577 )
	// warning C4774: 'sscanf' : format string expected in argument 2 is not a 
	//                string literal
	#pragma warning( disable : 4774 )
	#define NOGDI
	#include "imgui/imgui_demo.cpp"
	#include "imgui/imgui_draw.cpp"
	#include "imgui/imgui_widgets.cpp"
	#include "imgui/imgui.cpp"
	#undef NOGDI
#pragma warning( pop )
#define STB_DS_IMPLEMENTATION
internal void* kStbDsRealloc(
	void* allocatedAddress, size_t newAllocationSize, void* context)
{
	kassert(context);
	KgtAllocatorHandle hKal = reinterpret_cast<KgtAllocatorHandle>(context);
	void*const result = 
		kgtAllocRealloc(hKal, allocatedAddress, newAllocationSize);
	kassert(result);
	return result;
}
internal void kStbDsFree(void* allocatedAddress, void* context)
{
	kassert(context);
	KgtAllocatorHandle hKal = reinterpret_cast<KgtAllocatorHandle>(context);
	kgtAllocFree(hKal, allocatedAddress);
}
#pragma warning( push )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4456: declaration of 'i' hides previous local declaration
	#pragma warning( disable : 4456 )
	#include "stb/stb_ds.h"
#pragma warning( pop )
#include "kmath.cpp"
#include "kutil.cpp"
#include "kgtNetClient.cpp"
#include "kgtNetServer.cpp"
#include "KgtNetReliableDataBuffer.cpp"
#include "kgtFlipBook.cpp"
#include "kgtAudioMixer.cpp"
#pragma warning( push )
	/* warning C4296: '<': expression is always false.  This happens if there 
		are no KAssets in the assets directory */
	#pragma warning( disable : 4296 )
	#include "kgtAssetManager.cpp"
#pragma warning( pop )
#include "kgtAllocator.cpp"
#include "korl-texture.cpp"
#include "kgtDraw.cpp"