#include "win32-input.h"
#include "win32-platform.h"
internal ButtonState* w32DecodeVirtualKey(GameMouse* gm, WPARAM vKeyCode)
{
	ButtonState* buttonState = nullptr;
	// Virtual-Key Codes: 
	//	https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	switch(vKeyCode)
	{
		case VK_LBUTTON:  buttonState = &gm->left; break;
		case VK_RBUTTON:  buttonState = &gm->right; break;
		case VK_MBUTTON:  buttonState = &gm->middle; break;
		case VK_XBUTTON1: buttonState = &gm->back; break;
		case VK_XBUTTON2: buttonState = &gm->forward; break;
	}
	return buttonState;
}
internal ButtonState* w32DecodeVirtualKey(GameKeyboard* gk, WPARAM vKeyCode)
{
	ButtonState* buttonState = nullptr;
	// Virtual-Key Codes: 
	//	https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	switch(vKeyCode)
	{
		case VK_OEM_COMMA:  buttonState = &gk->comma; break;
		case VK_OEM_PERIOD: buttonState = &gk->period; break;
		case VK_OEM_2:      buttonState = &gk->slashForward; break;
		case VK_OEM_5:      buttonState = &gk->slashBack; break;
		case VK_OEM_4:      buttonState = &gk->curlyBraceLeft; break;
		case VK_OEM_6:      buttonState = &gk->curlyBraceRight; break;
		case VK_OEM_1:      buttonState = &gk->semicolon; break;
		case VK_OEM_7:      buttonState = &gk->quote; break;
		case VK_OEM_3:      buttonState = &gk->grave; break;
		case VK_OEM_MINUS:  buttonState = &gk->tenkeyless_minus; break;
		case VK_OEM_PLUS:   buttonState = &gk->equals; break;
		case VK_BACK:       buttonState = &gk->backspace; break;
		case VK_ESCAPE:     buttonState = &gk->escape; break;
		case VK_RETURN:     buttonState = &gk->enter; break;
		case VK_SPACE:      buttonState = &gk->space; break;
		case VK_TAB:        buttonState = &gk->tab; break;
		case VK_LSHIFT:     buttonState = &gk->shiftLeft; break;
		case VK_RSHIFT:     buttonState = &gk->shiftRight; break;
		case VK_LCONTROL:   buttonState = &gk->controlLeft; break;
		case VK_RCONTROL:   buttonState = &gk->controlRight; break;
		case VK_LMENU:      buttonState = &gk->altLeft; break;
		case VK_RMENU:      buttonState = &gk->altRight; break;
		case VK_UP:         buttonState = &gk->arrowUp; break;
		case VK_DOWN:       buttonState = &gk->arrowDown; break;
		case VK_LEFT:       buttonState = &gk->arrowLeft; break;
		case VK_RIGHT:      buttonState = &gk->arrowRight; break;
		case VK_INSERT:     buttonState = &gk->insert; break;
		case VK_DELETE:     buttonState = &gk->deleteKey; break;
		case VK_HOME:       buttonState = &gk->home; break;
		case VK_END:        buttonState = &gk->end; break;
		case VK_PRIOR:      buttonState = &gk->pageUp; break;
		case VK_NEXT:       buttonState = &gk->pageDown; break;
		case VK_DECIMAL:    buttonState = &gk->numpad_period; break;
		case VK_DIVIDE:     buttonState = &gk->numpad_divide; break;
		case VK_MULTIPLY:   buttonState = &gk->numpad_multiply; break;
		case VK_SEPARATOR:  buttonState = &gk->numpad_minus; break;
		case VK_ADD:        buttonState = &gk->numpad_add; break;
		default:
		{
			if(vKeyCode >= 0x41 && vKeyCode <= 0x5A)
			{
				buttonState = &gk->a + (vKeyCode - 0x41);
			}
			else if(vKeyCode >= 0x30 && vKeyCode <= 0x39)
			{
				buttonState = &gk->tenkeyless_0 + (vKeyCode - 0x30);
			}
			else if(vKeyCode >= 0x70 && vKeyCode <= 0x7B)
			{
				buttonState = &gk->f1 + (vKeyCode - 0x70);
			}
			else if(vKeyCode >= 0x60 && vKeyCode <= 0x69)
			{
				buttonState = &gk->numpad_0 + (vKeyCode - 0x60);
			}
		} break;
	}
	return buttonState;
}
internal void 
	w32GetKeyboardKeyStates(
		GameKeyboard* gkCurrFrame, GameKeyboard* gkPrevFrame)
{
	for(WPARAM vKeyCode = 0; vKeyCode <= 0xFF; vKeyCode++)
	{
		ButtonState* buttonState = w32DecodeVirtualKey(gkCurrFrame, vKeyCode);
		if(!buttonState)
		continue;
		const SHORT asyncKeyState = 
			GetAsyncKeyState(static_cast<int>(vKeyCode));
		// do NOT use the least-significant bit to determine key state!
		//	Why? See documentation on the GetAsyncKeyState function:
		// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate
		const bool keyDown = (asyncKeyState & ~1) != 0;
		const size_t buttonIndex = buttonState - gkCurrFrame->vKeys;
		*buttonState = keyDown
			? (gkPrevFrame->vKeys[buttonIndex] >= ButtonState::PRESSED
				? ButtonState::HELD
				: ButtonState::PRESSED)
			: ButtonState::NOT_PRESSED;
	}
	// update the game keyboard modifier states //
	gkCurrFrame->modifiers.shift =
		(gkCurrFrame->shiftLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->shiftRight > ButtonState::NOT_PRESSED);
	gkCurrFrame->modifiers.control =
		(gkCurrFrame->controlLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->controlRight > ButtonState::NOT_PRESSED);
	gkCurrFrame->modifiers.alt =
		(gkCurrFrame->altLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->altRight > ButtonState::NOT_PRESSED);
}
internal void w32GetMouseStates(GameMouse* gmCurrent, GameMouse* gmPrevious)
{
	if(g_mouseRelativeMode)
	{
		/* in mouse relative mode, we the mouse should remain in the same 
			position in window-space forever */
		gmCurrent->windowPosition = gmPrevious->windowPosition;
	}
	else
	/* get the mouse cursor position in window-space */
	{
		POINT pointMouseCursor;
		/* first, get the mouse cursor position in screen-space */
		if(!GetCursorPos(&pointMouseCursor))
		{
			const DWORD error = GetLastError();
			if(error == ERROR_ACCESS_DENIED)
				KLOG(WARNING, "GetCursorPos: access denied!");
			else
				KLOG(ERROR, "GetCursorPos failure! GetLastError=%i", 
					GetLastError());
		}
		else
		{
			/* convert mouse position screen-space => window-space */
			if(!ScreenToClient(g_mainWindow, &pointMouseCursor))
			{
				/* for whatever reason, MSDN doesn't say there is any kind of 
					error code obtainable for this failure condition... 
					¯\_(ツ)_/¯ */
				KLOG(ERROR, "ScreenToClient failure!");
			}
			gmCurrent->windowPosition.x = pointMouseCursor.x;
			gmCurrent->windowPosition.y = pointMouseCursor.y;
		}
	}
	/* get async mouse button states */
	for(WPARAM vKeyCode = 0; vKeyCode <= 0xFF; vKeyCode++)
	{
		ButtonState* buttonState = w32DecodeVirtualKey(gmCurrent, vKeyCode);
		if(!buttonState)
			continue;
		const SHORT asyncKeyState = 
			GetAsyncKeyState(static_cast<int>(vKeyCode));
		/* do NOT use the least-significant bit to determine key state!
			Why? See documentation on the GetAsyncKeyState function:
			https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate */
		const bool keyDown = (asyncKeyState & ~1) != 0;
		const size_t buttonIndex = buttonState - gmCurrent->vKeys;
		*buttonState = keyDown
			? (gmPrevious->vKeys[buttonIndex] >= ButtonState::PRESSED
				? ButtonState::HELD
				: ButtonState::PRESSED)
			: ButtonState::NOT_PRESSED;
	}
}