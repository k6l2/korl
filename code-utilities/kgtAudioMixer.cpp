#include "kgtAudioMixer.h"
struct KgtAudioTrack
{
	KgtAssetIndex kaiRawSound = KgtAssetIndex::ENUM_SIZE;
	u32 currentSampleBlock;
	u16 idSalt;
	bool repeat;
	bool pause;
	f32 volumeRatio = 1.f;
};
struct KgtAudioMixer
{
	u16 trackCount;
	u8 trackCount_PADDING[6];
	//AudioTrack tracks[];
};
internal KgtTapeHandle 
	kgtAudioMixerHashTrack(KgtAudioMixer* audioMixer, u16 trackIndex)
{
	if(trackIndex == audioMixer->trackCount)
	{
		return trackIndex;
	}
	korlAssert(trackIndex < audioMixer->trackCount);
	KgtAudioTrack*const tracks = 
		reinterpret_cast<KgtAudioTrack*>(audioMixer + 1);
	return (static_cast<u64>(tracks[trackIndex].kaiRawSound) << 32) |
		   (static_cast<u32>(tracks[trackIndex].idSalt) << 16) |
		   trackIndex;
}
internal u16 
	kgtAudioMixerSoundHandleTrackIndex(KgtTapeHandle soundHandle)
{
	return soundHandle & 0xFFFF;
}
internal KgtAudioMixer* 
	kgtAudioMixerConstruct(
		KgtAllocatorHandle allocator, u16 audioTrackCount)
{
	const size_t totalBytesRequired = 
		sizeof(KgtAudioMixer) + sizeof(KgtAudioTrack)*audioTrackCount;
	KgtAudioMixer*const result = reinterpret_cast<KgtAudioMixer*>(
		kgtAllocAlloc(allocator, totalBytesRequired));
	if(result)
	{
		*result = 
			{ .trackCount = audioTrackCount };
		KgtAudioTrack*const tracks = 
			reinterpret_cast<KgtAudioTrack*>(result + 1);
		for(u16 t = 0; t < audioTrackCount; t++)
		{
			tracks[t] = {};
		}
	}
	return result;
}
internal void 
	kgtAudioMixerMix(
		KgtAudioMixer* audioMixer, GameAudioBuffer& audioBuffer, 
		u32 sampleBlocksConsumed)
{
	KgtAudioTrack*const tracks = 
		reinterpret_cast<KgtAudioTrack*>(audioMixer + 1);
	u16 tracksActive = 0;
	// Examine all the tape tracks of the audio mixer.  
	//	Determine if there are any tapes currently playing.
	//	Determine how much of the tape has been consumed by the platform's audio 
	//	layer, and consequently if the track has run out of tape.
	for(u16 t = 0; t < audioMixer->trackCount; t++)
	{
		if(tracks[t].kaiRawSound == KgtAssetIndex::ENUM_SIZE)
		{
			continue;
		}
		const RawSound tRawSound = kgtAssetManagerGetRawSound(
			g_kam, tracks[t].kaiRawSound);
		if(!tracks[t].pause)
			tracks[t].currentSampleBlock += sampleBlocksConsumed;
		if(tracks[t].currentSampleBlock >= tRawSound.sampleBlockCount)
		{
			if(tracks[t].repeat)
			{
				tracks[t].currentSampleBlock %= tRawSound.sampleBlockCount;
			}
			else
			{
				tracks[t].kaiRawSound = KgtAssetIndex::ENUM_SIZE;
			}
		}
		if(tracks[t].kaiRawSound != KgtAssetIndex::ENUM_SIZE)
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
			if(   tracks[t].kaiRawSound == KgtAssetIndex::ENUM_SIZE
			   || tracks[t].pause)
			{
				continue;
			}
			if(kmath::isNearlyZero(tracks[t].volumeRatio))
			{
				continue;
			}
			korlAssert(tracks[t].volumeRatio >= 0.f && 
			           tracks[t].volumeRatio <= 1.f);
			const RawSound tRawSound = kgtAssetManagerGetRawSound(
				g_kam, tracks[t].kaiRawSound);
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
					static_cast<f32>(
						*(rawSoundSampleBlockStart + rawSoundChannel)) * 
					tracks[t].volumeRatio);
				*(sampleBlockStart + c) += sample;
			}
		}
	}
}
internal KgtTapeHandle 
	kgtAudioMixerPlaySound(
		KgtAudioMixer* audioMixer, KgtAssetIndex assetIndex)
{
	/* locate the first available audio track in the mixer (if one exists) */
	KgtAudioTrack*const tracks = 
		reinterpret_cast<KgtAudioTrack*>(audioMixer + 1);
	u16 trackIndexFirstAvailable = 0;
	for(; trackIndexFirstAvailable < audioMixer->trackCount
		; trackIndexFirstAvailable++)
	{
		if(tracks[trackIndexFirstAvailable].kaiRawSound == 
			KgtAssetIndex::ENUM_SIZE)
		{
			break;
		}
	}
	if(trackIndexFirstAvailable >= audioMixer->trackCount)
	/* all of the audio mixer's tracks are currently playing sounds */
	{
		return kgtAudioMixerHashTrack(audioMixer, audioMixer->trackCount);
	}
	/* there is an available tape track to put the sound in!  setup the track to 
		use the sound contained in `assetIndex` and pop it into the mixer.  We 
		then just return a handle to the tape */
	const u16 nextTapeSalt = static_cast<u16>( 
		tracks[trackIndexFirstAvailable].idSalt + u16(1) );
	tracks[trackIndexFirstAvailable] = {};
	tracks[trackIndexFirstAvailable].kaiRawSound = assetIndex;
	tracks[trackIndexFirstAvailable].idSalt      = nextTapeSalt;
	return kgtAudioMixerHashTrack(audioMixer, trackIndexFirstAvailable);
}
internal bool 
	kgtAudioMixerIsTapeHandleValid(
		KgtAudioMixer* audioMixer, KgtTapeHandle* kth)
{
	const u16 trackIndex = kgtAudioMixerSoundHandleTrackIndex(*kth);
	if(trackIndex >= audioMixer->trackCount)
	/* If the tape handle's trackIndex is out of bounds, it should always be 
		invalid no matter what.
		@assumption: audioMixer->trackCount is immutable once constructed */
	{
		return false;
	}
	const KgtTapeHandle currHandle = 
		kgtAudioMixerHashTrack(audioMixer, trackIndex);
	if(*kth != currHandle)
	/* the salt of `*kth` doesn't match the current tape handle of this track 
		index, which indicates `*kth` was once valid, but is no longer.  We 
		should invalidate the caller's sound handle since the tape no longer 
		exists in the mixer to prevent it from accidentally becoming valid in 
		the future if the salt rolls around to the same value! */
	{
		*kth = kgtAudioMixerHashTrack(audioMixer, audioMixer->trackCount);
	}
	return trackIndex < audioMixer->trackCount;
}
#define K_AUDIO_MIXER_DECODE_TAPE_HANDLE(...) \
	if(!kgtAudioMixerIsTapeHandleValid(audioMixer, tapeHandle))\
	/* Silently ignore the request to modify the track if the handle is \
		invalid */\
	{\
		return __VA_ARGS__;\
	}\
	const u16 trackIndex = kgtAudioMixerSoundHandleTrackIndex(*tapeHandle);\
	KgtAudioTrack*const tracks = \
		reinterpret_cast<KgtAudioTrack*>(audioMixer + 1);
internal void 
	kgtAudioMixerStopSound(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	tracks[trackIndex].kaiRawSound = KgtAssetIndex::ENUM_SIZE;
}
internal void 
	kgtAudioMixerSetRepeat(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, bool value)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	tracks[trackIndex].repeat = value;
}
internal void 
	kgtAudioMixerSetPause(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, bool value)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	tracks[trackIndex].pause = value;
}
internal void 
	kgtAudioMixerSetVolume(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, f32 volumeRatio)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	tracks[trackIndex].volumeRatio = volumeRatio;
}
internal bool 
	kgtAudioMixerGetPause(
		KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE(false);
	return tracks[trackIndex].pause;
}
