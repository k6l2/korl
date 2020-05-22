#include "kAudioMixer.h"
struct AudioTrack
{
	KAssetHandle soundAssetHandle;
	u32 currentSampleBlock;
};
struct KAudioMixer
{
	KAssetManager* assetManager;
	u16 trackCount;
	u8 trackCount_PADDING[6];
	//AudioTrack tracks[];
};
internal KAudioMixer* kauConstruct(KgaHandle allocator, u16 audioTrackCount, 
                                   KAssetManager* assetManager)
{
	KAudioMixer*const result = reinterpret_cast<KAudioMixer*>(
		kgaAlloc(allocator, sizeof(KAudioMixer) + 
		                        sizeof(AudioTrack)*audioTrackCount));
	if(result)
	{
		*result = 
			{ .assetManager = assetManager
			, .trackCount   = audioTrackCount };
		AudioTrack*const tracks = reinterpret_cast<AudioTrack*>(result + 1);
		for(u16 t = 0; t < audioTrackCount; t++)
		{
			tracks[t].soundAssetHandle = INVALID_KASSET_HANDLE;
		}
	}
	return result;
}
internal void kauMix(KAudioMixer* audioMixer, GameAudioBuffer& audioBuffer)
{
	AudioTrack*const tracks = reinterpret_cast<AudioTrack*>(audioMixer + 1);
	u16 tracksActive = 0;
	for(u16 t = 0; t < audioMixer->trackCount; t++)
	{
		if(tracks[t].soundAssetHandle != INVALID_KASSET_HANDLE)
		{
			tracksActive++;
		}
	}
	for(u32 s = 0; s < audioBuffer.lockedSampleCount; s++)
	{
		SoundSample*const sampleBlockStart = 
			audioBuffer.memory + (s*audioBuffer.numSoundChannels);
		for(u8 c = 0; c < audioBuffer.numSoundChannels; c++)
		{
			*(sampleBlockStart + c) = 0;
		}
		if(tracksActive <= 0)
		{
			continue;
		}
		for(u16 t = 0; t < audioMixer->trackCount; t++)
		{
			if(tracks[t].soundAssetHandle == INVALID_KASSET_HANDLE)
			{
				continue;
			}
			const RawSound tRawSound = 
				kamGetRawSound(audioMixer->assetManager, 
				               tracks[t].soundAssetHandle);
			SoundSample*const rawSoundSampleBlockStart = tRawSound.sampleData + 
				tRawSound.channelCount*tracks[t].currentSampleBlock;
			for(u8 c = 0; c < audioBuffer.numSoundChannels; c++)
			{
				const u8 rawSoundChannel = static_cast<u8>(
					c % tRawSound.channelCount);
				*(sampleBlockStart + c) += 
					*(rawSoundSampleBlockStart + rawSoundChannel);
			}
			tracks[t].currentSampleBlock++;
			if(tracks[t].currentSampleBlock >= tRawSound.sampleBlockCount)
			{
				tracks[t].soundAssetHandle = INVALID_KASSET_HANDLE;
			}
		}
	}
}
internal void kauPlaySound(KAudioMixer* audioMixer, KAssetHandle kahSound)
{
	kassert(kamIsRawSound(audioMixer->assetManager, kahSound));
	AudioTrack*const tracks = reinterpret_cast<AudioTrack*>(audioMixer + 1);
	u16 trackIndexFirstAvailable = 0;
	for(; trackIndexFirstAvailable < audioMixer->trackCount
		; trackIndexFirstAvailable++)
	{
		if(tracks[trackIndexFirstAvailable].soundAssetHandle == 
			INVALID_KASSET_HANDLE)
		{
			break;
		}
	}
	if(trackIndexFirstAvailable >= audioMixer->trackCount)
	{
		// all of the audio mixer's tracks are currently playing sounds //
		return;
	}
	tracks[trackIndexFirstAvailable].soundAssetHandle = kahSound;
	tracks[trackIndexFirstAvailable].currentSampleBlock = 0;
}