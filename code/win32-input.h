#pragma once
#include "win32-main.h"
internal ButtonState* w32DecodeVirtualKey(GameMouse* gm, WPARAM vKeyCode);
internal ButtonState* w32DecodeVirtualKey(GameKeyboard* gk, WPARAM vKeyCode);
internal void 
	w32GetKeyboardKeyStates(
		GameKeyboard* gkCurrFrame, GameKeyboard* gkPrevFrame);
internal void w32GetMouseStates(GameMouse* gmCurrent, GameMouse* gmPrevious);