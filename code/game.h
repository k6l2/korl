#pragma once
#include "global-defines.h"
internal void game_renderAudio(GameMemory& memory, 
                               GameAudioBuffer& audioBuffer);
/** 
 * @return false if the platform should close the game application
 */
internal bool game_updateAndDraw(GameMemory& memory, 
                                 GameGraphicsBuffer& graphicsBuffer, 
                                 GameKeyboard& gameKeyboard,
                                 GamePad* gamePadArray, u8 numGamePads);