/*
 * User code must define the following global variables to use this module:
 * - KrbApi* g_krb
 * - KgtAssetManager* g_kam
 */
#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
#include "platform-game-interfaces.h"
#include "gen_kgtAssets.h"
struct KgtAudioMixer;
using KgtTapeHandle = u64;
internal KgtAudioMixer* 
	kgtAudioMixerConstruct(
		KgtAllocatorHandle allocator, u16 audioTrackCount);
internal void 
	kgtAudioMixerMix(
		KgtAudioMixer* audioMixer, GameAudioBuffer& audioBuffer, 
		u32 sampleBlocksConsumed);
internal KgtTapeHandle 
	kgtAudioMixerPlayWav(
		KgtAudioMixer* audioMixer, KgtAssetIndex assetIndex);
internal KgtTapeHandle 
	kgtAudioMixerPlayOggVorbis(
		KgtAudioMixer* audioMixer, KgtAssetIndex assetIndex);
internal bool 
	kgtAudioMixerIsTapeHandleValid(
		KgtAudioMixer* audioMixer, KgtTapeHandle* kth);
internal void 
	kgtAudioMixerStopSound(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle);
internal void 
	kgtAudioMixerSetRepeat(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, bool value);
internal void 
	kgtAudioMixerSetPause(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, bool value);
internal void 
	kgtAudioMixerSetVolume(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, f32 volumeRatio);
internal bool 
	kgtAudioMixerGetPause(KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle);
