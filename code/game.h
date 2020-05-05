#pragma once
#include "global-defines.h"
struct GameState
{
	i32 offsetX = 0;
	i32 offsetY = 0;
	f32 theraminHz = 294.f;
	f32 theraminSine = 0;
	f32 theraminVolume = 5000;
};
#define GAME_RENDER_AUDIO(name) void name(GameMemory& memory, \
                                          GameAudioBuffer& audioBuffer)
typedef GAME_RENDER_AUDIO(fnSig_GameRenderAudio);
GAME_RENDER_AUDIO(gameRenderAudioStub)
{
}
extern "C" GAME_RENDER_AUDIO(gameRenderAudio);
#define GAME_UPDATE_AND_DRAW(name) bool name(GameMemory& memory, \
                                          GameGraphicsBuffer& graphicsBuffer, \
                                          GameKeyboard& gameKeyboard, \
                                          GamePad* gamePadArray, u8 numGamePads)
typedef GAME_UPDATE_AND_DRAW(fnSig_GameUpdateAndDraw);
GAME_UPDATE_AND_DRAW(gameUpdateAndDrawStub)
{
	return false;
}
/** 
 * @return false if the platform should close the game application
 */
extern "C" GAME_UPDATE_AND_DRAW(gameUpdateAndDraw);