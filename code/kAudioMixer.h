#pragma once
#include "global-defines.h"
#include "generalAllocator.h"
#include "kAssetManager.h"
#include "platform-game-interfaces.h"
struct KAudioMixer;
internal KAudioMixer* kauConstruct(KgaHandle allocator, u16 audioTrackCount, 
                                   KAssetManager* assetManager);
internal void kauMix(KAudioMixer* audioMixer, GameAudioBuffer& audioBuffer);
internal void kauPlaySound(KAudioMixer* audioMixer, KAssetHandle kahSound);