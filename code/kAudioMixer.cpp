#include "kAudioMixer.h"
struct AudioTrack
{
	KAssetHandle soundAssetHandle = INVALID_KASSET_HANDLE;
	u32 currentSampleBlock;
	u16 idSalt;
	bool repeat;
	u8 repeat_PADDING;
};
struct KAudioMixer
{
	KAssetManager* assetManager;
	u16 trackCount;
	u8 trackCount_PADDING[6];
	//AudioTrack tracks[];
};
internal KTapeHandle kauHashTrack(KAudioMixer* audioMixer, u16 trackIndex)
{
	if(trackIndex == audioMixer->trackCount)
	{
		return trackIndex;
	}
	kassert(trackIndex < audioMixer->trackCount);
	AudioTrack*const tracks = reinterpret_cast<AudioTrack*>(audioMixer + 1);
	return (static_cast<u64>(tracks[trackIndex].soundAssetHandle) << 32) |
		   (static_cast<u32>(tracks[trackIndex].idSalt) << 16) |
		   trackIndex;
}
internal u16 kauSoundHandleTrackIndex(KTapeHandle soundHandle)
{
	return soundHandle & 0xFFFF;
}
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
			tracks[t] = {};
		}
	}
	return result;
}
internal void kauMix(KAudioMixer* audioMixer, GameAudioBuffer& audioBuffer,
                     u32 sampleBlocksConsumed)
{
	AudioTrack*const tracks = reinterpret_cast<AudioTrack*>(audioMixer + 1);
	u16 tracksActive = 0;
	// Examine all the tape tracks of the audio mixer.  
	//	Determine if there are any tapes currently playing.
	//	Determine how much of the tape has been consumed by the platform's audio 
	//	layer, and consequently if the track has run out of tape.
	for(u16 t = 0; t < audioMixer->trackCount; t++)
	{
		if(tracks[t].soundAssetHandle == INVALID_KASSET_HANDLE)
		{
			continue;
		}
		const RawSound tRawSound = 
			kamGetRawSound(audioMixer->assetManager, 
			               tracks[t].soundAssetHandle);
		tracks[t].currentSampleBlock += sampleBlocksConsumed;
		if(tracks[t].currentSampleBlock >= tRawSound.sampleBlockCount)
		{
			if(tracks[t].repeat)
			{
				tracks[t].currentSampleBlock %= tRawSound.sampleBlockCount;
			}
			else
			{
				tracks[t].soundAssetHandle = INVALID_KASSET_HANDLE;
			}
		}
		if(tracks[t].soundAssetHandle != INVALID_KASSET_HANDLE)
		{
			tracksActive++;
		}
	}
	// Mix the audio from all the tape tracks into the platform's provided audio 
	//	buffer //
	for(u32 s = 0; s < audioBuffer.lockedSampleBlockCount; s++)
	{
		SoundSample*const sampleBlockStart = 
			audioBuffer.sampleBlocks + (s*audioBuffer.numSoundChannels);
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
			u32 currTapeSampleBlock = s + tracks[t].currentSampleBlock;
			if(currTapeSampleBlock >= tRawSound.sampleBlockCount)
			{
				if(tracks[t].repeat)
				{
					currTapeSampleBlock %= tRawSound.sampleBlockCount;
				}
				else
				{
					continue;
				}
			}
			SoundSample*const rawSoundSampleBlockStart = tRawSound.sampleData + 
				tRawSound.channelCount*currTapeSampleBlock;
			for(u8 c = 0; c < audioBuffer.numSoundChannels; c++)
			{
				const u8 rawSoundChannel = static_cast<u8>(
					c % tRawSound.channelCount);
				*(sampleBlockStart + c) += 
					*(rawSoundSampleBlockStart + rawSoundChannel);
			}
		}
	}
}
internal KTapeHandle kauPlaySound(KAudioMixer* audioMixer, 
                                  KAssetCStr kAssetCStr)
{
	const KAssetHandle kahSound = 
		kamPushAsset(audioMixer->assetManager, kAssetCStr);
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
		return kauHashTrack(audioMixer, audioMixer->trackCount);
	}
	const u16 nextTapeSalt = static_cast<u16>( 
		tracks[trackIndexFirstAvailable].idSalt + u16(1) );
	tracks[trackIndexFirstAvailable] = {};
	tracks[trackIndexFirstAvailable].soundAssetHandle = kahSound;
	tracks[trackIndexFirstAvailable].idSalt           = nextTapeSalt;
	return kauHashTrack(audioMixer, trackIndexFirstAvailable);
}
internal void kauSetRepeat(KAudioMixer* audioMixer, KTapeHandle* tapeHandle, 
                           bool value)
{
	const u16 trackIndex = kauSoundHandleTrackIndex(*tapeHandle);
	if(trackIndex >= audioMixer->trackCount)
	// Silently ignore the request to modify the track if the handle is invalid
	{
		return;
	}
	AudioTrack*const tracks = reinterpret_cast<AudioTrack*>(audioMixer + 1);
	const KTapeHandle currHandle = kauHashTrack(audioMixer, trackIndex);
	if(*tapeHandle != currHandle)
	// if the requested sound handle's identification doesn't match the current
	//	sound handle at the same track index, then we should invalidate the 
	//	caller's sound handle since the tape no longer exists in the mixer!
	{
		*tapeHandle = kauHashTrack(audioMixer, audioMixer->trackCount);
		return;
	}
	tracks[trackIndex].repeat = value;
}