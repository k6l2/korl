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
global_variable const WORD XINPUT_BUTTONS[] =
	{ XINPUT_GAMEPAD_Y
	, XINPUT_GAMEPAD_A
	, XINPUT_GAMEPAD_X
	, XINPUT_GAMEPAD_B
	, XINPUT_GAMEPAD_START
	, XINPUT_GAMEPAD_BACK
	, XINPUT_GAMEPAD_DPAD_UP
	, XINPUT_GAMEPAD_DPAD_DOWN
	, XINPUT_GAMEPAD_DPAD_LEFT
	, XINPUT_GAMEPAD_DPAD_RIGHT
	, XINPUT_GAMEPAD_LEFT_SHOULDER
	, XINPUT_GAMEPAD_RIGHT_SHOULDER
	, XINPUT_GAMEPAD_LEFT_THUMB
	, XINPUT_GAMEPAD_RIGHT_THUMB
};
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
		for(u16 b = 0; b < CARRAY_COUNT(XINPUT_BUTTONS); b++)
		{
			w32ProcessXInputButton(
				pad.wButtons & XINPUT_BUTTONS[b],
				gamePadArrayPreviousFrame[ci].buttons[b],
				&gamePadArrayCurrentFrame[ci].buttons[b]);
		}
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
internal PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(w32XInputGetGamePadActiveButton)
{
	kassert(gamePadIndex < XUSER_MAX_COUNT);
	if(gamePadIndex >= XUSER_MAX_COUNT)
		return INVALID_PLATFORM_BUTTON_INDEX;
	XINPUT_STATE controllerState;
	if( XInputGetState(gamePadIndex, &controllerState) != ERROR_SUCCESS )
		return INVALID_PLATFORM_BUTTON_INDEX;
	const XINPUT_GAMEPAD& pad = controllerState.Gamepad;
	u16 result = INVALID_PLATFORM_BUTTON_INDEX;
	for(u16 b = 0; b < CARRAY_COUNT(XINPUT_BUTTONS); b++)
	{
		if(pad.wButtons & XINPUT_BUTTONS[b])
		{
			if(result == INVALID_PLATFORM_BUTTON_INDEX)
			{
				result = b;
			}
			else
			{
				return INVALID_PLATFORM_BUTTON_INDEX;
			}
		}
	}
	return result;
}
internal PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(w32XInputGetGamePadActiveAxis)
{
	kassert(gamePadIndex < XUSER_MAX_COUNT);
	if(gamePadIndex >= XUSER_MAX_COUNT)
		return {INVALID_PLATFORM_AXIS_INDEX};
	XINPUT_STATE controllerState;
	if( XInputGetState(gamePadIndex, &controllerState) != ERROR_SUCCESS )
		return {INVALID_PLATFORM_AXIS_INDEX};
	const XINPUT_GAMEPAD& pad = controllerState.Gamepad;
	f32 axes[6];
	w32ProcessXInputStick(pad.sThumbLX, pad.sThumbLY,
	                      XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 2,
	                      &axes[0], &axes[1]);
	w32ProcessXInputStick(pad.sThumbRX, pad.sThumbRY,
	                      XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 2,
	                      &axes[2], &axes[3]);
	w32ProcessXInputTrigger(pad.bLeftTrigger,
	                        XINPUT_GAMEPAD_TRIGGER_THRESHOLD, &axes[4]);
	w32ProcessXInputTrigger(pad.bRightTrigger,
	                        XINPUT_GAMEPAD_TRIGGER_THRESHOLD, &axes[5]);
	PlatformGamePadActiveAxis result = {INVALID_PLATFORM_AXIS_INDEX};
	for(u16 a = 0; a < CARRAY_COUNT(axes); a++)
	{
		if(fabsf(axes[a]) > 0.9)
		{
			if(result.index == INVALID_PLATFORM_AXIS_INDEX)
			{
				result.index    = a;
				result.positive = axes[a] >= 0;
			}
			else
			{
				return {INVALID_PLATFORM_AXIS_INDEX};
			}
		}
	}
	return result;
}