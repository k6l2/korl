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
	if(padTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
	{
		*o_gamePadNormalizedTrigger = 0.f;
	}
	else
	{
		*o_gamePadNormalizedTrigger = 
			(padTrigger - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / 
			(255.f      - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
	}
}
internal void w32XInputGetGamePadStates(u8* io_numGamePads,
                                        GamePad* gamePadArrayCurrentFrame,
                                        GamePad* gamePadArrayPreviousFrame)
{
	*io_numGamePads = 0;
	for(u32 ci = 0; ci < XUSER_MAX_COUNT; ci++)
	{
		XINPUT_STATE controllerState;
		if( XInputGetState(ci, &controllerState) != ERROR_SUCCESS )
		{
			///TODO: handle controller not available
			continue;
		}
		///TODO: investigate controllerState.dwPacketNumber for 
		///      input polling performance
		const XINPUT_GAMEPAD& pad = controllerState.Gamepad;
		w32ProcessXInputStick(
			pad.sThumbLX, pad.sThumbLY,
			XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 2,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickLeft.x,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickLeft.y);
		w32ProcessXInputStick(
			pad.sThumbRX, pad.sThumbRY,
			XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 2,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickRight.x,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickRight.y);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_UP,
			gamePadArrayPreviousFrame[*io_numGamePads].dPadUp,
			&gamePadArrayCurrentFrame[*io_numGamePads].dPadUp);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN,
			gamePadArrayPreviousFrame[*io_numGamePads].dPadDown,
			&gamePadArrayCurrentFrame[*io_numGamePads].dPadDown);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT,
			gamePadArrayPreviousFrame[*io_numGamePads].dPadLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].dPadLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT,
			gamePadArrayPreviousFrame[*io_numGamePads].dPadRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].dPadRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_START,
			gamePadArrayPreviousFrame[*io_numGamePads].start,
			&gamePadArrayCurrentFrame[*io_numGamePads].start);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_BACK,
			gamePadArrayPreviousFrame[*io_numGamePads].back,
			&gamePadArrayCurrentFrame[*io_numGamePads].back);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB,
			gamePadArrayPreviousFrame[*io_numGamePads].stickClickLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].stickClickLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB,
			gamePadArrayPreviousFrame[*io_numGamePads].stickClickRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].stickClickRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER,
			gamePadArrayPreviousFrame[*io_numGamePads].shoulderLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].shoulderLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER,
			gamePadArrayPreviousFrame[*io_numGamePads].shoulderRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].shoulderRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_A,
			gamePadArrayPreviousFrame[*io_numGamePads].faceDown,
			&gamePadArrayCurrentFrame[*io_numGamePads].faceDown);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_B,
			gamePadArrayPreviousFrame[*io_numGamePads].faceRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].faceRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_X,
			gamePadArrayPreviousFrame[*io_numGamePads].faceLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].faceLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_Y,
			gamePadArrayPreviousFrame[*io_numGamePads].faceUp,
			&gamePadArrayCurrentFrame[*io_numGamePads].faceUp);
		w32ProcessXInputTrigger(
			pad.bLeftTrigger,
			XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedTriggerLeft);
		w32ProcessXInputTrigger(
			pad.bRightTrigger,
			XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedTriggerRight);
		gamePadArrayCurrentFrame[*io_numGamePads].
			normalizedTriggerRight = pad.bRightTrigger/255.f;
#if INTERNAL_BUILD && 0
		///TODO: delete this test pls, future me.
		if(gamePadArrayCurrentFrame[*io_numGamePads].buttons.faceUp == 
			ButtonState::PRESSED)
		{
			RaiseException(0xc0000374, 0, 0, NULL);// RUH ROH...
		}
#endif
		(*io_numGamePads)++;
	}
}