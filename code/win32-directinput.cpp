#include "win32-directinput.h"
#include "win32-main.h"
/* easy printing of GUIDs 
	Source: https://stackoverflow.com/a/26644772 */
#define GUID_FORMAT "%08lX-%04hX-%04hX-%02hhX%02hhX-"\
                    "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX"
#define GUID_ARG(guid) guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], \
                       guid.Data4[1], guid.Data4[2], guid.Data4[3], \
                       guid.Data4[4], guid.Data4[5], guid.Data4[6], \
                       guid.Data4[7]
/* The following block of code is near-copypasta from microsoft documentation in 
	order to detect whether or not a DirectInput device is actually Xinput 
	compatible.  It looks like garbage because it is, and shouldn't exist in the 
	first place had Xinput been constructed with half a hemisphere. 
	This is a modified version from the `pcsx2` project on github found here:
	https://github.com/PCSX2/pcsx2/blob/master/plugins/LilyPad/DirectInput.cpp
	which does not require the inclusion of the entire windows media SDK. */
#define SHITTY_MICROSOFT_CHECK_IF_DINPUT_DEVICE_IS_XINPUT_COMPATIBLE_CODE 1
#if SHITTY_MICROSOFT_CHECK_IF_DINPUT_DEVICE_IS_XINPUT_COMPATIBLE_CODE
#include <wbemidl.h>
#include <oleauto.h>
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) \
   if(x != NULL)        \
   {                    \
      x->Release();     \
      x = NULL;         \
   }
#endif
//-----------------------------------------------------------------------------
// Enum each PNP device using WMI and check each device ID to see if it contains 
// "IG_" (ex. "VID_045E&PID_028E&IG_00").  If it does, then it's an XInput device
// Unfortunately this information can not be found by just using DirectInput 
//-----------------------------------------------------------------------------
BOOL IsXInputDevice(const GUID* pGuidProductFromDirectInput)
{
	IWbemLocator *pIWbemLocator = NULL;
	IEnumWbemClassObject *pEnumDevices = NULL;
	IWbemClassObject *pDevices[20] = {0};
	IWbemServices *pIWbemServices = NULL;
	BSTR bstrNamespace = NULL;
	BSTR bstrDeviceID = NULL;
	BSTR bstrClassName = NULL;
	DWORD uReturned = 0;
	bool bIsXinputDevice = false;
	UINT iDevice = 0;
	VARIANT var;
	HRESULT hr;
	// CoInit if needed
	hr = CoInitialize(NULL);
	bool bCleanupCOM = SUCCEEDED(hr);
	// Create WMI
	hr = CoCreateInstance(__uuidof(WbemLocator),
	                      NULL,
	                      CLSCTX_INPROC_SERVER,
	                      __uuidof(IWbemLocator),
	                      (LPVOID *)&pIWbemLocator);
	if(FAILED(hr) || pIWbemLocator == NULL)
		goto LCleanup;
	bstrNamespace = SysAllocString(L"\\\\.\\root\\cimv2");
	if(bstrNamespace == NULL)
		goto LCleanup;
	bstrClassName = SysAllocString(L"Win32_PNPEntity");
	if(bstrClassName == NULL)
		goto LCleanup;
	bstrDeviceID = SysAllocString(L"DeviceID");
	if(bstrDeviceID == NULL)
		goto LCleanup;
	// Connect to WMI
	hr = pIWbemLocator->ConnectServer(bstrNamespace, NULL, NULL, 0L,
	                                  0L, NULL, NULL, &pIWbemServices);
	if(FAILED(hr) || pIWbemServices == NULL)
		goto LCleanup;
	// Switch security level to IMPERSONATE.
	CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
	                  RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 
	                  EOAC_NONE);
	hr = pIWbemServices->CreateInstanceEnum(bstrClassName, 0, NULL, 
	                                        &pEnumDevices);
	if(FAILED(hr) || pEnumDevices == NULL)
	    goto LCleanup;
	// Loop over all devices
	for(;;) 
	{
		// Get 20 at a time
		hr = pEnumDevices->Next(10000, 20, pDevices, &uReturned);
		if(FAILED(hr))
			goto LCleanup;
		if(uReturned == 0)
			break;
		for(iDevice = 0; iDevice < uReturned; iDevice++) 
		{
			// For each device, get its device ID
			hr = pDevices[iDevice]->Get(bstrDeviceID, 0L, &var, NULL, NULL);
			if(SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != NULL) 
			{
				/* Check if the device ID contains "IG_".  If it does, then it's 
					an XInput device.  This information can not be found from 
					DirectInput */
				if (wcsstr(var.bstrVal, L"IG_")) 
				{
					// If it does, then get the VID/PID from var.bstrVal
					DWORD dwPid = 0, dwVid = 0;
					WCHAR *strVid = wcsstr(var.bstrVal, L"VID_");
					if(strVid) 
					{
						dwVid = wcstoul(strVid + 4, 0, 16);
					}
					WCHAR *strPid = wcsstr(var.bstrVal, L"PID_");
					if(strPid) 
					{
						dwPid = wcstoul(strPid + 4, 0, 16);
					}
					// Compare the VID/PID to the DInput device
					DWORD dwVidPid = MAKELONG(dwVid, dwPid);
					if(dwVidPid == pGuidProductFromDirectInput->Data1) 
					{
						bIsXinputDevice = true;
						goto LCleanup;
					}
				}
			}
			SAFE_RELEASE(pDevices[iDevice]);
		}
	}
LCleanup:
	if(bstrNamespace)
		SysFreeString(bstrNamespace);
	if(bstrDeviceID)
		SysFreeString(bstrDeviceID);
	if(bstrClassName)
		SysFreeString(bstrClassName);
	for(iDevice = 0; iDevice < 20; iDevice++)
		SAFE_RELEASE(pDevices[iDevice]);
	SAFE_RELEASE(pEnumDevices);
	SAFE_RELEASE(pIWbemLocator);
	SAFE_RELEASE(pIWbemServices);
	if(bCleanupCOM)
		CoUninitialize();
	return bIsXinputDevice;
}
#endif // SHITTY_MICROSOFT_CHECK_IF_DINPUT_DEVICE_IS_XINPUT_COMPATIBLE_CODE
#define DINPUT_BUTTON_PRESSED(button) ((button & 0b10000000) != 0)
BOOL DIEnumDeviceAbsoluteAxes(LPCDIDEVICEOBJECTINSTANCE lpddoi, 
                              LPVOID pvDI8Device)
{
	LPDIRECTINPUTDEVICE8 di8Device = 
		reinterpret_cast<LPDIRECTINPUTDEVICE8>(pvDI8Device);
	/* set the axis range */
	{
		DIPROPRANGE gameControllerRange;
		gameControllerRange.diph.dwSize       = sizeof(DIPROPRANGE);
		gameControllerRange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		gameControllerRange.diph.dwHow        = DIPH_BYID;
		gameControllerRange.diph.dwObj        = lpddoi->dwType;
		gameControllerRange.lMin = -0x7FFF;
		gameControllerRange.lMax =  0x7FFF;
		const HRESULT hResult = 
			di8Device->SetProperty(DIPROP_RANGE, &gameControllerRange.diph);
		if(hResult == DI_PROPNOEFFECT)
		{
			KLOG(WARNING, "Axis range property is read-only.");
		}
		else if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to set axis range!");
		}
	}
	/* set the deadzone range */
	{
		DIPROPDWORD gameControllerDeadZone;
		gameControllerDeadZone.diph.dwSize       = sizeof(DIPROPDWORD);
		gameControllerDeadZone.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		gameControllerDeadZone.diph.dwHow        = DIPH_BYID;
		gameControllerDeadZone.diph.dwObj        = lpddoi->dwType;
		/* the deadzone data is measured in centi-percents [0,10000] */
		gameControllerDeadZone.dwData = 100;
		const HRESULT hResult = 
			di8Device->SetProperty(DIPROP_DEADZONE, 
			                       &gameControllerDeadZone.diph);
		if(hResult == DI_PROPNOEFFECT)
		{
			KLOG(WARNING, "Deadzone property is read-only.");
		}
		else if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to set deadzone!");
		}
	}
	return DIENUM_CONTINUE;
}
/**
 * This function will attempt to use the given device instance to create a 
 * direct input device and store it in `g_dInputDevices`.  If the device has 
 * already been created, nothing will happen.  If the input devices array is 
 * already full of devices, nothing will happen.
 */
internal void w32DInputAddDevice(LPCDIDEVICEINSTANCE lpddi)
{
	size_t firstEmptyInputDevice = CARRAY_COUNT(g_dInputDevices);
	for(size_t d = 0; d < CARRAY_COUNT(g_dInputDevices); d++)
	/* iterate over all existing input devices and check if this device instance 
		has already been created, and determine if there is an empty slot we can 
		add a new device to simultaneously */
	{
		if(!g_dInputDevices[d])
		{
			if(firstEmptyInputDevice == CARRAY_COUNT(g_dInputDevices))
				firstEmptyInputDevice = d;
			continue;
		}
		DIDEVICEINSTANCE deviceInstance;
		deviceInstance.dwSize = sizeof(deviceInstance);
		const HRESULT hResult = 
			g_dInputDevices[d]->GetDeviceInfo(&deviceInstance);
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to get device info for device %i!", d);
			continue;
		}
		if(IsEqualGUID(lpddi->guidInstance, deviceInstance.guidInstance))
		/* the device already existed in the global array of DirectInput 
			devices */
		{
			firstEmptyInputDevice = CARRAY_COUNT(g_dInputDevices);
			g_dInputDeviceJustAcquiredFlags[d] = true;
			break;
		}
	}
	if(firstEmptyInputDevice == CARRAY_COUNT(g_dInputDevices))
	/* do nothing if we have too many devices, or if the device is already added
		to the g_dInputDevices */
	{
		return;
	}
	/* create a new direct input device and add it to the global array */
	{
		const HRESULT hResult = 
			g_pIDInput->CreateDevice(lpddi->guidInstance, 
			                         &g_dInputDevices[firstEmptyInputDevice], 
			                         nullptr);
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to create direct input device! '%s':'%s'", 
			     lpddi->tszProductName, lpddi->tszInstanceName);
			return;
		}
	}
	/* set the cooperative level of the new device */
	{
		const HRESULT hResult = g_dInputDevices[firstEmptyInputDevice]->
			SetCooperativeLevel(g_mainWindow, DISCL_BACKGROUND | DISCL_EXCLUSIVE);
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to set direct input device cooperation level! "
			     "'%s':'%s'", lpddi->tszProductName, lpddi->tszInstanceName);
			return;
		}
	}
	/* set the data format of the new device.  Setting the data format to 
		c_dfDIJoystick2 will allow us to fetch data into the pre-defined 
		directinput struct DIJOYSTATE */
	{
		const HRESULT hResult = g_dInputDevices[firstEmptyInputDevice]->
			SetDataFormat(&c_dfDIJoystick);
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to set direct input device data format! "
			     "'%s':'%s'", lpddi->tszProductName, lpddi->tszInstanceName);
			return;
		}
	}
	/* query the device for capabilities and set device properties */
	{
#if 0
		DIDEVCAPS capabilities;
		capabilities.dwSize = sizeof(DIDEVCAPS);
		const HRESULT hResultGetCaps = 
			g_dInputDevices[firstEmptyInputDevice]->
				GetCapabilities(&capabilities);
		if(hResultGetCaps != DI_OK)
		{
			KLOG(ERROR, "Failed to get direct input device capabilities! "
			     "'%s':'%s'", lpddi->tszProductName, lpddi->tszInstanceName);
			return;
		}
#endif // 0
		const HRESULT hResultEnumAbsAxes = 
			g_dInputDevices[firstEmptyInputDevice]->
				EnumObjects(DIEnumDeviceAbsoluteAxes, 
				            g_dInputDevices[firstEmptyInputDevice], 
				            DIDFT_ABSAXIS);
		if(hResultEnumAbsAxes != DI_OK)
		{
			KLOG(ERROR, "Failed to enum direct input device absolute axes! "
			     "'%s':'%s'", lpddi->tszProductName, lpddi->tszInstanceName);
			return;
		}
	}
	/* actually acquire the device so that we can start getting input! */
	{
		const HRESULT hResult = 
			g_dInputDevices[firstEmptyInputDevice]->Acquire();
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to acquire direct input device! '%s':'%s'",
			     lpddi->tszProductName, lpddi->tszInstanceName);
			return;
		}
		g_dInputDeviceJustAcquiredFlags[firstEmptyInputDevice] = true;
	}
}
/**
 * @return DIENUM_CONTINUE to continue the enumeration.  DIENUM_STOP to stop the 
 *         enumeration.
 */
internal BOOL DIEnumAttachedGameControllers(LPCDIDEVICEINSTANCE lpddi, 
                                            LPVOID /*pvRef*/)
{
	if(IsXInputDevice(&lpddi->guidProduct))
	{
		KLOG(INFO, "XInput device detected! Skipping: '%s'", 
		     lpddi->tszProductName);
		return DIENUM_CONTINUE;
	}
	/* Log the product name & GUID */
	{
		KLOG(INFO, "DInput device detected! '%s' guidProduct{" GUID_FORMAT "} "
		     "guidInstance{" GUID_FORMAT "}", lpddi->tszProductName, 
		     GUID_ARG(lpddi->guidProduct), GUID_ARG(lpddi->guidInstance));
	}
	w32DInputAddDevice(lpddi);
	return DIENUM_CONTINUE;
}
internal void w32DInputEnumerateDevices()
{
	static_assert(CARRAY_COUNT(g_dInputDevices) == 
	              CARRAY_COUNT(g_dInputDeviceJustAcquiredFlags));
	/* clear the flags which determine if a device was found during 
		EnumDevices */
	for(size_t a = 0; a < CARRAY_COUNT(g_dInputDevices); a++)
	{
		g_dInputDeviceJustAcquiredFlags[a] = false;
	}
	const HRESULT hResult = 
		g_pIDInput->EnumDevices(DI8DEVCLASS_GAMECTRL, 
		                        DIEnumAttachedGameControllers, 
		                        nullptr, DIEDFL_ATTACHEDONLY);
	if(hResult != DI_OK)
	{
		KLOG(ERROR, "Failed to enumerate DirectInput devices! "
		     "hResult=%li", hResult);
	}
	/* because we have flags for which devices were in fact found during the 
		call to EnumDevices, we can unacquire & release devices that were 
		not! */
	for(size_t a = 0; a < CARRAY_COUNT(g_dInputDevices); a++)
	{
		if(g_dInputDevices[a] && !g_dInputDeviceJustAcquiredFlags)
		{
			g_dInputDevices[a]->Unacquire();
			g_dInputDevices[a]->Release();
			g_dInputDevices[a] = nullptr;
		}
	}
}
internal void w32LoadDInput(HINSTANCE hInst)
{
	HMODULE LibDInput = LoadLibraryA("dinput8.dll");
	if(!LibDInput)
	{
		KLOG(WARNING, "Failed to load dinput8.dll! GetLastError=i", 
		     GetLastError());
	}
	if(!LibDInput)
	{
		KLOG(ERROR, "Failed to load DirectInput!");
	}
	/* create a directinput8 COM interface instance (please god, kill me now) */
	{
		const HRESULT hResult = 
			DirectInput8Create(hInst, DIRECTINPUT_VERSION, IID_IDirectInput8, 
			                   reinterpret_cast<LPVOID*>(&g_pIDInput), nullptr);
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to create DirectInput COM interface! "
			     "hResult=%li", hResult);
		}
	}
	w32DInputEnumerateDevices();
}
internal void w32ProcessDInputStick(LONG xiThumbX, LONG xiThumbY, 
                                    u16 circularDeadzoneMagnitude, 
                                    f32 *o_normalizedStickX, 
                                    f32 *o_normalizedStickY)
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
internal void w32ProcessDInputButton(bool buttonPressed, 
                                     ButtonState buttonStatePrevious,
                                     ButtonState* o_buttonStateCurrent)
{
	if(buttonPressed)
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
internal void w32ProcessDInputTrigger(bool buttonPressed, 
                                      f32 *o_normalizedTrigger)
{
	*o_normalizedTrigger = buttonPressed ? 1.f : 0.f;
}
internal void w32ProcessDInputPovButton(DWORD povCentiDegreesCwFromNorth, 
                                        DWORD buttonCentiDegreesCwFromNorth, 
                                        ButtonState buttonStatePrevious, 
                                        ButtonState* o_buttonStateCurrent)
{
	/* taken directly from microsoft documentation */
	const BOOL povIsCentered = (LOWORD(povCentiDegreesCwFromNorth) == 0xFFFF);
	if(povIsCentered)
	{
		*o_buttonStateCurrent = ButtonState::NOT_PRESSED;
		return;
	}
	/* transform the stupid CentiDegrees measurements into vectors so we can 
		compare directions */
	const f32 radiansPov    = (povCentiDegreesCwFromNorth   /100.f)*PI32/180;
	const f32 radiansButton = (buttonCentiDegreesCwFromNorth/100.f)*PI32/180;
	const v2f32 v2dPov    = kmath::rotate({0,1}, radiansPov);
	const v2f32 v2dButton = kmath::rotate({0,1}, radiansButton);
	/* we can now simply treat this as a standard button press using dot product 
		to test the direction of the button's vector and the POV's vector */
	const bool buttonPressed = v2dPov.dot(v2dButton) >= 0;
	w32ProcessDInputButton(buttonPressed, buttonStatePrevious, 
	                       o_buttonStateCurrent);
}
internal void w32DInputGetGamePadStates(GamePad* gpArrCurrFrame,
                                        GamePad* gpArrPrevFrame)
{
	for(size_t d = 0; d < CARRAY_COUNT(g_dInputDevices); d++)
	{
		if(!g_dInputDevices[d])
		{
			gpArrCurrFrame[d].type = GamePadType::UNPLUGGED;
			continue;
		}
		const HRESULT hResultPoll = g_dInputDevices[d]->Poll();
		if(!(hResultPoll == DI_OK || hResultPoll == DI_NOEFFECT))
		/* poll failed means the device is no longer acquired, attempt to 
			re-acquire */
		{
			const HRESULT hResultAcquire = g_dInputDevices[d]->Acquire();
			if(hResultAcquire == DIERR_OTHERAPPHASPRIO)
			{
				gpArrCurrFrame[d].type = GamePadType::UNPLUGGED;
				continue;
			}
			if(hResultAcquire != DI_OK)
			{
				KLOG(WARNING, "Failed to re-acquire device! Unacquiring...");
				g_dInputDevices[d]->Unacquire();
				g_dInputDevices[d]->Release();
				g_dInputDevices[d] = nullptr;
				gpArrCurrFrame[d].type = GamePadType::UNPLUGGED;
				continue;
			}
		}
		DIJOYSTATE joyState;
		ZeroMemory(&joyState, sizeof(joyState));
		/* get the state of the controller */
		const HRESULT hResultGetState = 
			g_dInputDevices[d]->GetDeviceState(sizeof(joyState), &joyState);
		if(hResultGetState != DI_OK)
		{
			KLOG(WARNING, "Failed to get device state!");
			gpArrCurrFrame[d].type = GamePadType::UNPLUGGED;
			continue;
		}
		gpArrCurrFrame[d].type = GamePadType::DINPUT_GENERIC;
		w32ProcessDInputStick(
			joyState.lX, -joyState.lY,
			///TODO: use the DirectInput device deadzone here probably??
			XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 2,
			&gpArrCurrFrame[d].normalizedStickLeft.x,
			&gpArrCurrFrame[d].normalizedStickLeft.y);
		w32ProcessDInputStick(
			joyState.lZ, -joyState.lRz,
			///TODO: use the DirectInput device deadzone here probably??
			XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 2,
			&gpArrCurrFrame[d].normalizedStickRight.x,
			&gpArrCurrFrame[d].normalizedStickRight.y);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[0]),
		                       gpArrPrevFrame[d].faceLeft,
		                       &gpArrCurrFrame[d].faceLeft);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[1]),
		                       gpArrPrevFrame[d].faceDown,
		                       &gpArrCurrFrame[d].faceDown);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[2]),
		                       gpArrPrevFrame[d].faceRight,
		                       &gpArrCurrFrame[d].faceRight);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[3]),
		                       gpArrPrevFrame[d].faceUp,
		                       &gpArrCurrFrame[d].faceUp);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[4]),
		                       gpArrPrevFrame[d].shoulderLeft,
		                       &gpArrCurrFrame[d].shoulderLeft);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[5]),
		                       gpArrPrevFrame[d].shoulderRight,
		                       &gpArrCurrFrame[d].shoulderRight);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[8]),
		                       gpArrPrevFrame[d].back,
		                       &gpArrCurrFrame[d].back);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[9]),
		                       gpArrPrevFrame[d].start,
		                       &gpArrCurrFrame[d].start);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[10]),
		                       gpArrPrevFrame[d].stickClickLeft,
		                       &gpArrCurrFrame[d].stickClickLeft);
		w32ProcessDInputButton(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[11]),
		                       gpArrPrevFrame[d].stickClickRight,
		                       &gpArrCurrFrame[d].stickClickRight);
		w32ProcessDInputTrigger(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[6]),
		                        &gpArrCurrFrame[d].normalizedTriggerLeft);
		w32ProcessDInputTrigger(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[7]),
		                        &gpArrCurrFrame[d].normalizedTriggerRight);
		w32ProcessDInputPovButton(joyState.rgdwPOV[0], 0, 
		                          gpArrPrevFrame[d].dPadUp, 
		                          &gpArrCurrFrame[d].dPadUp);
		w32ProcessDInputPovButton(joyState.rgdwPOV[0], 9000, 
		                          gpArrPrevFrame[d].dPadRight, 
		                          &gpArrCurrFrame[d].dPadRight);
		w32ProcessDInputPovButton(joyState.rgdwPOV[0], 18000, 
		                          gpArrPrevFrame[d].dPadDown, 
		                          &gpArrCurrFrame[d].dPadDown);
		w32ProcessDInputPovButton(joyState.rgdwPOV[0], 27000, 
		                          gpArrPrevFrame[d].dPadLeft, 
		                          &gpArrCurrFrame[d].dPadLeft);
	}
}
internal PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(w32DInputGetGamePadActiveButton)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_dInputDevices));
	if(gamePadIndex >= CARRAY_COUNT(g_dInputDevices))
		return INVALID_PLATFORM_BUTTON_INDEX;
	LPDIRECTINPUTDEVICE8 dInput8Device = g_dInputDevices[gamePadIndex];
	if(!dInput8Device)
		return INVALID_PLATFORM_BUTTON_INDEX;
	/* poll the device for current input state */
	const HRESULT hResultPoll = dInput8Device->Poll();
	if(!(hResultPoll == DI_OK || hResultPoll == DI_NOEFFECT))
	/* poll failed means the device is no longer acquired, attempt to 
		re-acquire */
	{
		const HRESULT hResultAcquire = dInput8Device->Acquire();
		if(hResultAcquire == DIERR_OTHERAPPHASPRIO)
		{
			return INVALID_PLATFORM_BUTTON_INDEX;
		}
		if(hResultAcquire != DI_OK)
		{
			KLOG(WARNING, "Failed to re-acquire device! Unacquiring...");
			g_dInputDevices[gamePadIndex]->Unacquire();
			g_dInputDevices[gamePadIndex]->Release();
			g_dInputDevices[gamePadIndex] = nullptr;
			return INVALID_PLATFORM_BUTTON_INDEX;
		}
	}
	DIJOYSTATE joyState;
	ZeroMemory(&joyState, sizeof(joyState));
	/* get the state of the controller */
	const HRESULT hResultGetState = 
		dInput8Device->GetDeviceState(sizeof(joyState), &joyState);
	if(hResultGetState != DI_OK)
	{
		KLOG(WARNING, "Failed to get device state!");
		return INVALID_PLATFORM_BUTTON_INDEX;
	}
	/* at this point, we should have the state of the controller and we can 
		actually check to see what is being pressed/moved: */
	for(u16 b = 0; b < CARRAY_COUNT(joyState.rgbButtons); b++)
	{
		if(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[b]))
			return b;
	}
	/* interpret the POV switch directions as buttons for all POV switches */
	for(size_t pov = 0; pov < CARRAY_COUNT(joyState.rgdwPOV); pov++)
	{
		for(u8 povDirection = 0; povDirection < 4; povDirection++)
		{
			DWORD povCentiDegreesCwFromNorth = povDirection*9000;
			ButtonState buttonState;
			w32ProcessDInputPovButton(joyState.rgdwPOV[pov], 
			                          povCentiDegreesCwFromNorth, 
			                          ButtonState::NOT_PRESSED, 
			                          &buttonState);
			if(buttonState > ButtonState::NOT_PRESSED)
			{
				return kmath::safeTruncateU16(
					CARRAY_COUNT(joyState.rgbButtons) + pov*4 + povDirection);
			}
		}
	}
	return INVALID_PLATFORM_BUTTON_INDEX;
}