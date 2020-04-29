#include "game.h"
internal void game_renderAudio(GameAudioBuffer& audioBuffer, f32 theraminHz)
{
	// render theramin audio data to temporary sound buffer //
	local_persist f32 theraminSine = 0;
	local_persist const f32 THERAMIN_VOLUME = 5000;
	const u32 samplesPerWaveTheramin = audioBuffer.soundSampleHz / theraminHz;
	for(u32 s = 0; s < audioBuffer.lockedSampleCount; s++)
	{
		SoundSample* sample = 
			audioBuffer.memory + (s*audioBuffer.numSoundChannels);
		// Determine what the debug waveforms should look like at this point of
		//	the running sound sample //
		const f32 waveform = sinf(theraminSine);
		theraminSine += 2*PI32*(1.f / samplesPerWaveTheramin);
		while(theraminSine > 2*PI32) theraminSine -= 2*PI32;
		for(u8 c = 0; c < audioBuffer.numSoundChannels; c++)
		{
			*sample++ = static_cast<SoundSample>(THERAMIN_VOLUME * waveform);
		}
	}
}
internal void game_updateAndDraw(GameGraphicsBuffer& graphicsBuffer,
                                 int offsetX, int offsetY)
{
	// render a weird gradient pattern to the offscreen buffer //
	u8* row = reinterpret_cast<u8*>(graphicsBuffer.bitmapMemory);
	for (int y = 0; y < graphicsBuffer.height; y++)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = 0; x < graphicsBuffer.width; x++)
		{
			// pixel format: 0xXxRrGgBb
			*pixel++ = ((u8)(x + offsetX) << 16) | ((u8)(y + offsetY) << 8);
		}
		row += graphicsBuffer.pitch;
	}
}