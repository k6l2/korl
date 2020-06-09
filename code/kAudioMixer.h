#pragma once
#include "global-defines.h"
#include "generalAllocator.h"
#include "kAssetManager.h"
#include "platform-game-interfaces.h"
struct KAudioMixer;
using KTapeHandle = u64;
internal KAudioMixer* kauConstruct(KgaHandle allocator, u16 audioTrackCount, 
                                   KAssetManager* assetManager);
internal void kauMix(KAudioMixer* audioMixer, GameAudioBuffer& audioBuffer, 
                     u32 sampleBlocksConsumed);
internal KTapeHandle kauPlaySound(KAudioMixer* audioMixer, 
                                  KAssetCStr kAssetCStr);
internal void kauSetRepeat(KAudioMixer* audioMixer, KTapeHandle* tapeHandle, 
                           bool value);
internal void kauSetVolume(KAudioMixer* audioMixer, KTapeHandle* tapeHandle, 
                           f32 volumeRatio);