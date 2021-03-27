#include "kgtAudioMixer.h"
struct KgtAudioTrack
{
	KgtAssetIndex kaiRawSound = KgtAssetIndex::ENUM_SIZE;
	enum class AssetType : u8
		{ WAV, OGG_VORBIS }
			assetType;
	u32 currentSampleBlock;
	u16 idSalt;
	bool repeat;
	bool pause;
	f32 volumeRatio = 1.f;
};
struct KgtAudioMixer
{
	u16 trackCount;
	KgtAudioTrack tracks[1];
};
internal KgtTapeHandle kgtAudioMixerHashTrack(
	KgtAudioMixer* kam, u16 trackIndex)
{
	if(trackIndex == kam->trackCount)
		return trackIndex;
	korlAssert(trackIndex < kam->trackCount);
	return (static_cast<u64>(kam->tracks[trackIndex].kaiRawSound) << 32) 
		|  (static_cast<u32>(kam->tracks[trackIndex].idSalt)      << 16) 
		|  trackIndex;
}
internal u16 kgtAudioMixerSoundHandleTrackIndex(KgtTapeHandle soundHandle)
{
	return soundHandle & 0xFFFF;
}
internal KgtAudioMixer* kgtAudioMixerConstruct(
	KgtAllocatorHandle allocator, u16 audioTrackCount)
{
	korlAssert(audioTrackCount > 0);
	const size_t totalBytesRequired = 
		sizeof(KgtAudioMixer) + sizeof(KgtAudioTrack)*(audioTrackCount - 1);
	KgtAudioMixer*const result = reinterpret_cast<KgtAudioMixer*>(
		kgtAllocAlloc(allocator, totalBytesRequired));
	if(!result)
	{
		KLOG(ERROR, "Failed to allocate %zu bytes for audio mixer!", 
			totalBytesRequired);
		return nullptr;
	}
	*result = 
		{ .trackCount = audioTrackCount };
	for(u16 t = 0; t < audioTrackCount; t++)
		result->tracks[t] = {};
	return result;
}
internal RawSound _kgt_audioMixer_getRawSound(
	KgtAudioMixer* audioMixer, u16 trackIndex)
{
	korlAssert(trackIndex < audioMixer->trackCount);
	korlAssert(
		audioMixer->tracks[trackIndex].kaiRawSound != KgtAssetIndex::ENUM_SIZE);
	switch(audioMixer->tracks[trackIndex].assetType)
	{
	case KgtAudioTrack::AssetType::WAV: {
		return 
			kgt_assetWav_get(g_kam, audioMixer->tracks[trackIndex].kaiRawSound);
		} break;
	case KgtAudioTrack::AssetType::OGG_VORBIS: {
		korlAssert(!"@todo: use custom KgtAsset PTU API!");
		} break;
	}
	KLOG(ERROR, "Invalid audio track asset type! (%u)", 
		static_cast<u32>(audioMixer->tracks[trackIndex].assetType));
	return {};
}
internal void kgtAudioMixerMix(
	KgtAudioMixer* audioMixer, GameAudioBuffer& audioBuffer, 
	u32 sampleBlocksConsumed)
{
	u16 tracksActive = 0;
	/* Examine all the tape tracks of the audio mixer.  
		Determine if there are any tapes currently playing.
		Determine how much of the tape has been consumed by the platform's audio 
		layer, and consequently if the track has run out of tape. */
	for(u16 t = 0; t < audioMixer->trackCount; t++)
	{
		if(audioMixer->tracks[t].kaiRawSound == KgtAssetIndex::ENUM_SIZE)
			continue;
		const RawSound tRawSound = _kgt_audioMixer_getRawSound(audioMixer, t);
		if(!audioMixer->tracks[t].pause)
			audioMixer->tracks[t].currentSampleBlock += sampleBlocksConsumed;
		if(audioMixer->tracks[t].currentSampleBlock >= 
			tRawSound.sampleBlockCount)
		{
			if(audioMixer->tracks[t].repeat)
				audioMixer->tracks[t].currentSampleBlock %= 
					tRawSound.sampleBlockCount;
			else
				audioMixer->tracks[t].kaiRawSound = KgtAssetIndex::ENUM_SIZE;
		}
		if(audioMixer->tracks[t].kaiRawSound != KgtAssetIndex::ENUM_SIZE)
			tracksActive++;
	}
	// Mix the audio from all the tape tracks into the platform's provided audio 
	//	buffer //
	for(u32 s = 0; s < audioBuffer.lockedSampleBlockCount; s++)
	{
		SoundSample*const sampleBlockStart = 
			audioBuffer.sampleBlocks + (s*audioBuffer.numSoundChannels);
		for(u8 c = 0; c < audioBuffer.numSoundChannels; c++)
			*(sampleBlockStart + c) = 0;
		if(tracksActive <= 0)
			continue;
		for(u16 t = 0; t < audioMixer->trackCount; t++)
		{
			if(   audioMixer->tracks[t].kaiRawSound == KgtAssetIndex::ENUM_SIZE
			   || audioMixer->tracks[t].pause)
				continue;
			if(kmath::isNearlyZero(audioMixer->tracks[t].volumeRatio))
				continue;
			korlAssert(audioMixer->tracks[t].volumeRatio >= 0.f 
				&& audioMixer->tracks[t].volumeRatio <= 1.f);
			const RawSound tRawSound = 
				_kgt_audioMixer_getRawSound(audioMixer, t);
			u32 currTapeSampleBlock = 
				s + audioMixer->tracks[t].currentSampleBlock;
			if(currTapeSampleBlock >= tRawSound.sampleBlockCount)
			{
				if(audioMixer->tracks[t].repeat)
					currTapeSampleBlock %= tRawSound.sampleBlockCount;
				else
					continue;
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
					audioMixer->tracks[t].volumeRatio);
				*(sampleBlockStart + c) += sample;
			}
		}
	}
}
internal KgtTapeHandle _kgtAudioMixerPlaySound(
	KgtAudioMixer* audioMixer, KgtAssetIndex assetIndex, 
	KgtAudioTrack::AssetType assetType)
{
	/* locate the first available audio track in the mixer (if one exists) */
	u16 trackIndexFirstAvailable = 0;
	for(; trackIndexFirstAvailable < audioMixer->trackCount
			; trackIndexFirstAvailable++)
		if(audioMixer->tracks[trackIndexFirstAvailable].kaiRawSound == 
				KgtAssetIndex::ENUM_SIZE)
			break;
	if(trackIndexFirstAvailable >= audioMixer->trackCount)
		/* all of the audio mixer's tracks are currently playing sounds */
		return kgtAudioMixerHashTrack(audioMixer, audioMixer->trackCount);
	/* there is an available tape track to put the sound in!  setup the track to 
		use the sound contained in `assetIndex` and pop it into the mixer.  We 
		then just return a handle to the tape */
	const u16 nextTapeSalt = static_cast<u16>( 
		audioMixer->tracks[trackIndexFirstAvailable].idSalt + u16(1) );
	audioMixer->tracks[trackIndexFirstAvailable] = {};
	audioMixer->tracks[trackIndexFirstAvailable].assetType   = assetType;
	audioMixer->tracks[trackIndexFirstAvailable].kaiRawSound = assetIndex;
	audioMixer->tracks[trackIndexFirstAvailable].idSalt      = nextTapeSalt;
	return kgtAudioMixerHashTrack(audioMixer, trackIndexFirstAvailable);
}
internal KgtTapeHandle kgtAudioMixerPlayWav(
	KgtAudioMixer* audioMixer, KgtAssetIndex assetIndex)
{
	return _kgtAudioMixerPlaySound(
		audioMixer, assetIndex, KgtAudioTrack::AssetType::WAV);
}
internal KgtTapeHandle kgtAudioMixerPlayOggVorbis(
	KgtAudioMixer* audioMixer, KgtAssetIndex assetIndex)
{
	return _kgtAudioMixerPlaySound(
		audioMixer, assetIndex, KgtAudioTrack::AssetType::OGG_VORBIS);
}
internal bool kgtAudioMixerIsTapeHandleValid(
	KgtAudioMixer* audioMixer, KgtTapeHandle* kth)
{
	const u16 trackIndex = kgtAudioMixerSoundHandleTrackIndex(*kth);
	if(trackIndex >= audioMixer->trackCount)
		/* If the tape handle's trackIndex is out of bounds, it should always be 
			invalid no matter what.
			@assumption: audioMixer->trackCount is immutable once constructed */
		return false;
	const KgtTapeHandle currHandle = 
		kgtAudioMixerHashTrack(audioMixer, trackIndex);
	if(*kth != currHandle)
		/* the salt of `*kth` doesn't match the current tape handle of this 
			track index, which indicates `*kth` was once valid, but is no 
			longer.  We should invalidate the caller's sound handle since the 
			tape no longer exists in the mixer to prevent it from accidentally 
			becoming valid in the future if the salt rolls around to the same 
			value! */
		*kth = kgtAudioMixerHashTrack(audioMixer, audioMixer->trackCount);
	return trackIndex < audioMixer->trackCount;
}
#define K_AUDIO_MIXER_DECODE_TAPE_HANDLE(...) \
	if(!kgtAudioMixerIsTapeHandleValid(audioMixer, tapeHandle))\
		/* Silently ignore the request to modify the track if the handle is \
			invalid */\
		return __VA_ARGS__;\
	const u16 trackIndex = kgtAudioMixerSoundHandleTrackIndex(*tapeHandle);
internal void kgtAudioMixerStopSound(
	KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	audioMixer->tracks[trackIndex].kaiRawSound = KgtAssetIndex::ENUM_SIZE;
}
internal void kgtAudioMixerSetRepeat(
	KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, bool value)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	audioMixer->tracks[trackIndex].repeat = value;
}
internal void kgtAudioMixerSetPause(
	KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, bool value)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	audioMixer->tracks[trackIndex].pause = value;
}
internal void kgtAudioMixerSetVolume(
	KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle, f32 volumeRatio)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE();
	audioMixer->tracks[trackIndex].volumeRatio = volumeRatio;
}
internal bool kgtAudioMixerGetPause(
	KgtAudioMixer* audioMixer, KgtTapeHandle* tapeHandle)
{
	K_AUDIO_MIXER_DECODE_TAPE_HANDLE(false);
	return audioMixer->tracks[trackIndex].pause;
}
