#include "kAudioMixer.h"
struct AudioTrack
{
	KAssetIndex soundKAssetId = KAssetIndex::ENUM_SIZE;
	u32 currentSampleBlock;
	u16 idSalt;
	bool repeat;
	u8 repeat_PADDING;
	f32 volumeRatio = 1.f;
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
	return (static_cast<u64>(tracks[trackIndex].soundKAssetId) << 32) |
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
		if(tracks[t].soundKAssetId == KAssetIndex::ENUM_SIZE)
		{
			continue;
		}
		const RawSound tRawSound = 
			kamGetRawSound(audioMixer->assetManager, tracks[t].soundKAssetId);
		tracks[t].currentSampleBlock += sampleBlocksConsumed;
		if(tracks[t].currentSampleBlock >= tRawSound.sampleBlockCount)
		{
			if(tracks[t].repeat)
			{
				tracks[t].currentSampleBlock %= tRawSound.sampleBlockCount;
			}
			else
			{
				tracks[t].soundKAssetId = KAssetIndex::ENUM_SIZE;
			}
		}
		if(tracks[t].soundKAssetId != KAssetIndex::ENUM_SIZE)
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
			if(tracks[t].soundKAssetId == KAssetIndex::ENUM_SIZE)
			{
				continue;
			}
			if(kmath::isNearlyZero(tracks[t].volumeRatio))
			{
				continue;
			}
			kassert(tracks[t].volumeRatio >= 0.f && 
			        tracks[t].volumeRatio <= 1.f);
			const RawSound tRawSound = 
				kamGetRawSound(audioMixer->assetManager, 
				               tracks[t].soundKAssetId);
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
				const SoundSample sample = static_cast<SoundSample>(
					*(rawSoundSampleBlockStart + rawSoundChannel) * 
					tracks[t].volumeRatio);
				*(sampleBlockStart + c) += sample;
			}
		}
	}
}
internal KTapeHandle kauPlaySound(KAudioMixer* audioMixer, 
                                  KAssetIndex assetIndex)
{
	/* locate the first available audio track in the mixer (if one exists) */
	AudioTrack*const tracks = reinterpret_cast<AudioTrack*>(audioMixer + 1);
	u16 trackIndexFirstAvailable = 0;
	for(; trackIndexFirstAvailable < audioMixer->trackCount
		; trackIndexFirstAvailable++)
	{
		if(tracks[trackIndexFirstAvailable].soundKAssetId == 
			KAssetIndex::ENUM_SIZE)
		{
			break;
		}
	}
	if(trackIndexFirstAvailable >= audioMixer->trackCount)
	/* all of the audio mixer's tracks are currently playing sounds */
	{
		return kauHashTrack(audioMixer, audioMixer->trackCount);
	}
	/* there is an available tape track to put the sound in!  setup the track to 
		use the sound contained in `assetIndex` and pop it into the mixer.  We 
		then just return a handle to the tape */
	const u16 nextTapeSalt = static_cast<u16>( 
		tracks[trackIndexFirstAvailable].idSalt + u16(1) );
	tracks[trackIndexFirstAvailable] = {};
	tracks[trackIndexFirstAvailable].soundKAssetId = assetIndex;
	tracks[trackIndexFirstAvailable].idSalt        = nextTapeSalt;
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
internal void kauSetVolume(KAudioMixer* audioMixer, KTapeHandle* tapeHandle, 
                           f32 volumeRatio)
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
	tracks[trackIndex].volumeRatio = volumeRatio;
}