#pragma once
#include "global-defines.h"
#include "generalAllocator.h"
#include "kAssetManager.h"
#include "platform-game-interfaces.h"
struct KAudioMixer;
using KTapeHandle = u64;
internal KAudioMixer* kauConstruct(KgaHandle allocator, u16 audioTrackCount, 
                                   KAssetManager* assetManager);
internal void kauMix(KAudioMixer* audioMixer, GameAudioBuffer& audioBuffer);
internal KTapeHandle kauPlaySound(KAudioMixer* audioMixer, 
                                  KAssetHandle kahSound);
internal void kauSetRepeat(KAudioMixer* audioMixer, KTapeHandle* tapeHandle, 
                           bool value);