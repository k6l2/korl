#include "win32-xinput.h"
#include "win32-main.h"
XINPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_NOT_SUPPORTED;
}
XINPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_NOT_SUPPORTED;
}
internal void w32LoadXInput()
{
	HMODULE LibXInput = LoadLibraryA("xinput1_4.dll");
	if(!LibXInput)
	{
		KLOG(WARNING, "Failed to load xinput1_4.dll! GetLastError=i", 
		     GetLastError());
		LibXInput = LoadLibraryA("xinput9_1_0.dll");
	}
	if(!LibXInput)
	{
		KLOG(WARNING, "Failed to load xinput9_1_0.dll! GetLastError=i", 
		     GetLastError());
		LibXInput = LoadLibraryA("xinput1_3.dll");
	}
	if(!LibXInput)
	{
		KLOG(WARNING, "Failed to load xinput1_3.dll! GetLastError=i", 
		     GetLastError());
	}
	if(LibXInput)
	{
		XInputGetState = 
			(fnSig_XInputGetState*)GetProcAddress(LibXInput, "XInputGetState");
		if(!XInputGetState)
		{
			XInputGetState_ = XInputGetStateStub;
			KLOG(WARNING, "Failed to get XInputGetState! GetLastError=i", 
			     GetLastError());
		}
		XInputSetState = 
			(fnSig_XInputSetState*)GetProcAddress(LibXInput, "XInputSetState");
		if(!XInputSetState)
		{
			XInputSetState_ = XInputSetStateStub;
			KLOG(WARNING, "Failed to get XInputSetState! GetLastError=i", 
			     GetLastError());
		}
	}
	else
	{
		KLOG(ERROR, "Failed to load XInput!");
	}
}
internal void w32ProcessXInputStick(SHORT xiThumbX, SHORT xiThumbY,
                                    u16 circularDeadzoneMagnitude,
                                    f32* o_normalizedStickX, 
                                    f32* o_normalizedStickY)
{
	if(xiThumbX < -0x7FFF) xiThumbX = -0x7FFF;
	if(xiThumbY < -0x7FFF) xiThumbY = -0x7FFF;
	local_persist const f32 MAX_THUMB_MAG = static_cast<f32>(0x7FFF);
	f32 thumbMag = 
		sqrtf(static_cast<f32>(xiThumbX)*xiThumbX + xiThumbY*xiThumbY);
	const f32 thumbNormX = 
		!kmath::isNearlyZero(thumbMag) ? xiThumbX / thumbMag : 0.f;
	const f32 thumbNormY = 
		!kmath::isNearlyZero(thumbMag) ? xiThumbY / thumbMag : 0.f;
	if(thumbMag <= circularDeadzoneMagnitude)
	{
		*o_normalizedStickX = 0.f;
		*o_normalizedStickY = 0.f;
		return;
	}
	if(thumbMag > MAX_THUMB_MAG) 
	{
		thumbMag = MAX_THUMB_MAG;
	}
	const f32 thumbMagNorm = (thumbMag - circularDeadzoneMagnitude) / 
	                    (MAX_THUMB_MAG - circularDeadzoneMagnitude);
	*o_normalizedStickX = thumbNormX * thumbMagNorm;
	*o_normalizedStickY = thumbNormY * thumbMagNorm;
}
internal void w32ProcessXInputButton(bool xInputButtonPressed, 
                                     ButtonState buttonStatePrevious,
                                     ButtonState* o_buttonStateCurrent)
{
	if(xInputButtonPressed)
	{
		if(buttonStatePrevious == ButtonState::NOT_PRESSED)
		{
			*o_buttonStateCurrent = ButtonState::PRESSED;
		}
		else
		{
			*o_buttonStateCurrent = ButtonState::HELD;
		}
	}
	else
	{
		*o_buttonStateCurrent = ButtonState::NOT_PRESSED;
	}
}
internal void w32ProcessXInputTrigger(BYTE padTrigger,
                                      BYTE padTriggerDeadzone,
                                      f32* o_gamePadNormalizedTrigger)
{
	if(padTrigger <= padTriggerDeadzone)
	{
		*o_gamePadNormalizedTrigger = 0.f;
	}
	else
	{
		*o_gamePadNormalizedTrigger = 
			(padTrigger - padTriggerDeadzone) / 
			(255.f      - padTriggerDeadzone);
	}
}
internal void w32XInputGetGamePadStates(GamePad* gamePadArrayCurrentFrame,
                                        GamePad* gamePadArrayPreviousFrame)
{
	for(u32 ci = 0; ci < XUSER_MAX_COUNT; ci++)
	{
		XINPUT_STATE controllerState;
		if( XInputGetState(ci, &controllerState) != ERROR_SUCCESS )
		{
			gamePadArrayCurrentFrame[ci].type = GamePadType::UNPLUGGED;
			continue;
		}
		///TODO: investigate controllerState.dwPacketNumber for 
		///      input polling performance
		gamePadArrayCurrentFrame[ci].type = GamePadType::XINPUT;
		const XINPUT_GAMEPAD& pad = controllerState.Gamepad;
		w32ProcessXInputStick(
			pad.sThumbLX, pad.sThumbLY,
			XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 2,
			&gamePadArrayCurrentFrame[ci].normalizedStickLeft.x,
			&gamePadArrayCurrentFrame[ci].normalizedStickLeft.y);
		w32ProcessXInputStick(
			pad.sThumbRX, pad.sThumbRY,
			XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 2,
			&gamePadArrayCurrentFrame[ci].normalizedStickRight.x,
			&gamePadArrayCurrentFrame[ci].normalizedStickRight.y);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_UP,
			gamePadArrayPreviousFrame[ci].dPadUp,
			&gamePadArrayCurrentFrame[ci].dPadUp);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN,
			gamePadArrayPreviousFrame[ci].dPadDown,
			&gamePadArrayCurrentFrame[ci].dPadDown);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT,
			gamePadArrayPreviousFrame[ci].dPadLeft,
			&gamePadArrayCurrentFrame[ci].dPadLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT,
			gamePadArrayPreviousFrame[ci].dPadRight,
			&gamePadArrayCurrentFrame[ci].dPadRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_START,
			gamePadArrayPreviousFrame[ci].start,
			&gamePadArrayCurrentFrame[ci].start);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_BACK,
			gamePadArrayPreviousFrame[ci].back,
			&gamePadArrayCurrentFrame[ci].back);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB,
			gamePadArrayPreviousFrame[ci].stickClickLeft,
			&gamePadArrayCurrentFrame[ci].stickClickLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB,
			gamePadArrayPreviousFrame[ci].stickClickRight,
			&gamePadArrayCurrentFrame[ci].stickClickRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER,
			gamePadArrayPreviousFrame[ci].shoulderLeft,
			&gamePadArrayCurrentFrame[ci].shoulderLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER,
			gamePadArrayPreviousFrame[ci].shoulderRight,
			&gamePadArrayCurrentFrame[ci].shoulderRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_A,
			gamePadArrayPreviousFrame[ci].faceDown,
			&gamePadArrayCurrentFrame[ci].faceDown);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_B,
			gamePadArrayPreviousFrame[ci].faceRight,
			&gamePadArrayCurrentFrame[ci].faceRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_X,
			gamePadArrayPreviousFrame[ci].faceLeft,
			&gamePadArrayCurrentFrame[ci].faceLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_Y,
			gamePadArrayPreviousFrame[ci].faceUp,
			&gamePadArrayCurrentFrame[ci].faceUp);
		w32ProcessXInputTrigger(
			pad.bLeftTrigger,
			XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
			&gamePadArrayCurrentFrame[ci].normalizedTriggerLeft);
		w32ProcessXInputTrigger(
			pad.bRightTrigger,
			XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
			&gamePadArrayCurrentFrame[ci].normalizedTriggerRight);
#if INTERNAL_BUILD && 0
		///TODO: delete this test pls, future me.
		if(gamePadArrayCurrentFrame[ci].buttons.faceUp == 
			ButtonState::PRESSED)
		{
			RaiseException(0xc0000374, 0, 0, NULL);// RUH ROH...
		}
#endif
	}
}