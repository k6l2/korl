#include "kgtGameState.h"
#include "gen_kgtAssets.h"
#include "z85-png-default.h"
#include "z85.h"
internal void kgtGameStateOnReloadCode(KgtGameState* kgs, GameMemory& memory)
{
	g_kpl                     = &memory.kpl;
	g_krb                     = &memory.krb;
	g_kgs                     = kgs;
	g_kam                     = kgs->assetManager;
	platformLog               = memory.kpl.log;
	korlPlatformAssertFailure = memory.kpl.assertFailure;
	/* ImGui support */
	ImGui::SetCurrentContext(
		reinterpret_cast<ImGuiContext*>(memory.imguiContext));
	ImGui::SetAllocatorFunctions(
		memory.platformImguiAlloc, memory.platformImguiFree, 
		memory.imguiAllocUserData);
}
internal void 
	kgtGameStateInitialize(
		GameMemory& memory, size_t totalGameStateSize, 
		size_t frameAllocatorBytes)
{
	korlAssert(totalGameStateSize <= memory.permanentMemoryBytes);
	/* Tell KRB where it can safely store its CPU-side internal state.  We only 
		ever need to do this one time because *tgs should be in an immutable 
		spot in memory forever */
	g_krb->setCurrentContext(&g_kgs->krbContext);
	// initialize dynamic allocators //
	g_kgs->hKalPermanent = kgtAllocInit(
		KgtAllocatorType::GENERAL, 
		reinterpret_cast<u8*>(memory.permanentMemory) + totalGameStateSize, 
		memory.permanentMemoryBytes - totalGameStateSize);
	g_kgs->hKalTransient = kgtAllocInit(
		KgtAllocatorType::GENERAL, 
		memory.transientMemory, memory.transientMemoryBytes);
	// construct a linear frame allocator //
	{
		void*const kalFrameStartAddress = 
			kgtAllocAlloc(g_kgs->hKalPermanent, frameAllocatorBytes);
		g_kgs->hKalFrame = 
			kgtAllocInit(
				KgtAllocatorType::LINEAR, kalFrameStartAddress, 
				frameAllocatorBytes);
	}
	/* construct the dynamic application's asset manager */
	g_kgs->assetManager = 
		kgt_assetManager_construct(
			g_kgs->hKalPermanent, KGT_ASSET_COUNT, g_kgs->hKalTransient, 
			g_kpl, g_krb);
	/* add asset descriptors immediately after construction */
	// add PNG asset descriptor //
	{
		defer(kgtAllocReset(g_kgs->hKalFrame));
		const u32 rawFileBytes = kmath::safeTruncateU32(
			z85::decodedFileSizeBytes(CARRAY_SIZE(z85_png_default) - 1));
		korlAssert(rawFileBytes);
		u8* rawFileData = reinterpret_cast<u8*>(
			kgtAllocAlloc(g_kgs->hKalFrame, rawFileBytes));
		korlAssert(rawFileData);
		const bool successDecodeZ85 = 
			z85::decode(
				reinterpret_cast<const i8*>(z85_png_default), 
				reinterpret_cast<i8*>(rawFileData));
		korlAssert(successDecodeZ85);
		kgt_assetManager_addAssetDescriptor(
			g_kgs->assetManager, KgtAsset::Type::KGTASSETPNG, ".png", 
			rawFileData, rawFileBytes);
	}
	// add Texture asset descriptor //
	{
		char defaultTexture[] = 
			"image-asset-file-name : USE_DEFAULT_IMAGE_ASSET\n"
			"wrap-x                : clamp\n"
			"wrap-y                : clamp\n"
			"filter-minify         : nearest\n"
			"filter-magnify        : nearest";
		kgt_assetManager_addAssetDescriptor(
			g_kgs->assetManager, KgtAsset::Type::KGTASSETTEXTURE, ".tex", 
			reinterpret_cast<u8*>(defaultTexture), 
			/* subtract 1 from size to only account for file size, NOT the null
				character terminator */
			sizeof(defaultTexture) - 1);
	}
	/* set the global asset manager pointer again here because the VERY FIRST 
		time it is set in `kgtGameStateOnReloadCode` when the program first 
		starts the value is zero, but on subsequent calls to the same function 
		the value determined here will be the one which is copied instead! */
	g_kam = g_kgs->assetManager;
#if SEPARATE_ASSET_MODULES_COMPLETE
	// Initialize the game's audio mixer //
	g_kgs->audioMixer = kgtAudioMixerConstruct(g_kgs->hKalPermanent, 16);
#endif// SEPARATE_ASSET_MODULES_COMPLETE
	kgt_assetManager_loadAllStaticAssets(g_kam);
}
internal void 
	kgtGameStateRenderAudio(
		GameAudioBuffer& audioBuffer, u32 sampleBlocksConsumed)
{
#if SEPARATE_ASSET_MODULES_COMPLETE
	kgtAudioMixerMix(g_kgs->audioMixer, audioBuffer, sampleBlocksConsumed);
#endif// SEPARATE_ASSET_MODULES_COMPLETE
}
internal bool 
	kgtGameStateUpdateAndDraw(
		const GameKeyboard& gameKeyboard, bool windowIsFocused)
{
	kgtAllocReset(g_kgs->hKalFrame);
	/* shortcuts to quickly exit the program */
	if(   (   gameKeyboard.f4 == ButtonState::PRESSED 
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
	kgt_assetManager_reloadChangedAssets(g_kgs->assetManager);
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
	/* warning C5219: implicit conversion from 'int' to 'float', possible loss 
		of data */
	#pragma warning( disable : 5219 )
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
	korlAssert(context);
	KgtAllocatorHandle hKal = reinterpret_cast<KgtAllocatorHandle>(context);
	void*const result = 
		kgtAllocRealloc(hKal, allocatedAddress, newAllocationSize);
	korlAssert(result);
	return result;
}
internal void kStbDsFree(void* allocatedAddress, void* context)
{
	korlAssert(context);
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
#include "platform-game-interfaces.cpp"
#include "kgtNetClient.cpp"
#include "kgtNetServer.cpp"
#include "KgtNetReliableDataBuffer.cpp"
#if SEPARATE_ASSET_MODULES_COMPLETE
#include "kgtFlipBook.cpp"
#include "kgtAudioMixer.cpp"
#endif// SEPARATE_ASSET_MODULES_COMPLETE
#include "kgtAsset.cpp"
#include "kgtAssetManager.cpp"
#include "z85.cpp"
#include "kgtAssetPng.cpp"
#include "kgtAssetTexture.cpp"
#include "kgtAllocator.cpp"
#include "korl-texture.cpp"
#if SEPARATE_ASSET_MODULES_COMPLETE
#include "kgtDraw.cpp"
#include "kgtSpriteFont.cpp"
#endif// SEPARATE_ASSET_MODULES_COMPLETE