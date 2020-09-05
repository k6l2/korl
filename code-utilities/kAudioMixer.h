#pragma once
#include "kutil.h"
#include "kGeneralAllocator.h"
#include "kAssetManager.h"
#include "platform-game-interfaces.h"
#include "gen_kassets.h"
struct KAudioMixer;
using KTapeHandle = u64;
internal KAudioMixer* kauConstruct(
	KgaHandle allocator, u16 audioTrackCount, KAssetManager* assetManager);
internal void kauMix(KAudioMixer* audioMixer, GameAudioBuffer& audioBuffer, 
                     u32 sampleBlocksConsumed);
internal KTapeHandle kauPlaySound(
	KAudioMixer* audioMixer, KAssetIndex assetIndex);
internal bool kauIsTapeHandleValid(KAudioMixer* audioMixer, KTapeHandle* kth);
internal void kauStopSound(KAudioMixer* audioMixer, KTapeHandle* tapeHandle);
internal void kauSetRepeat(
	KAudioMixer* audioMixer, KTapeHandle* tapeHandle, bool value);
internal void kauSetPause(
	KAudioMixer* audioMixer, KTapeHandle* tapeHandle, bool value);
internal void kauSetVolume(
	KAudioMixer* audioMixer, KTapeHandle* tapeHandle, f32 volumeRatio);
internal bool kauGetPause(KAudioMixer* audioMixer, KTapeHandle* tapeHandle);