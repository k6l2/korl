#include "win32-directinput.h"
#include "win32-main.h"
global_variable const char* DIRECT_INPUT_TO_GAMEPAD_MAPS[] =
	{ "Logitech Dual Action{C216046D-0000-0000-0000-504944564944}b3:0,b1:1,"
		"b0:2,b2:3,b9:4,b8:5,b32:6,b34:7,b35:8,b33:9,b4:10,b5:11,b6:12,b7:13,"
		"b10:14,b11:15,+0:0,-1:1,+2:2,-5:3"
};
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
	/* locate the device in the DInput=>GamePad database by comparing the 
		product GUID of the lpddi to all of the GUIDs of the input map 
		strings */
	g_dInputDeviceGamePadMapIndices[firstEmptyInputDevice] = 
		CARRAY_COUNT(DIRECT_INPUT_TO_GAMEPAD_MAPS);
	for(size_t gpm = 0; gpm < CARRAY_COUNT(DIRECT_INPUT_TO_GAMEPAD_MAPS); gpm++)
	{
		/* parse the product GUID of the DInput=>GamePad input map */
		const char* cStrMapGuid = 
			strchr(DIRECT_INPUT_TO_GAMEPAD_MAPS[gpm], '{');
		const char* cStrMapGuidEnd = 
			strrchr(DIRECT_INPUT_TO_GAMEPAD_MAPS[gpm], '}');
		if(!cStrMapGuid || !cStrMapGuidEnd || cStrMapGuidEnd <= cStrMapGuid)
		{
			KLOG(ERROR, "Invalid DirectInput=>GamePad map string! '%s'",
			     DIRECT_INPUT_TO_GAMEPAD_MAPS[gpm]);
			continue;
		}
		cStrMapGuid++;// skip the '{' character to get straight into the GUID
		GUID guidInputMap;
		char parseGuidBuffer[9];
		for(u8 currentGuidDataParsing = 0; currentGuidDataParsing < 4; 
			currentGuidDataParsing++)
		{
			switch(currentGuidDataParsing)
			{
				case 0:// Guid::Data1 is a u32 (8 hex characters)
				{
					for(u8 c = 0; c < 8; c++)
					{
						parseGuidBuffer[c] = *cStrMapGuid++;
					}
					parseGuidBuffer[8] = '\0';
					if(*cStrMapGuid == '-')
						cStrMapGuid++;// skip over the '-' separator
					kassert(errno == 0);
					guidInputMap.Data1 = static_cast<u32>(
						strtoll(parseGuidBuffer, nullptr, 16));
					if (errno == ERANGE)
					{
						KLOG(ERROR, "strtoll range error!");
						errno = 0;
					}
				}break;
				case 1:// Guid::Data2/3 are u16 (4 hex characters)
				case 2:
				{
					for(u8 c = 0; c < 4; c++)
					{
						parseGuidBuffer[c] = *cStrMapGuid++;
					}
					parseGuidBuffer[4] = '\0';
					if(*cStrMapGuid == '-')
						cStrMapGuid++;// skip over the '-' separator
					kassert(errno == 0);
					guidInputMap.Data2 = static_cast<u16>(
						strtol(parseGuidBuffer, nullptr, 16));
					if (errno == ERANGE)
					{
						KLOG(ERROR, "strtoll range error!");
						errno = 0;
					}
					currentGuidDataParsing++;// we can just handle Data3 now
					for(u8 c = 0; c < 4; c++)
					{
						parseGuidBuffer[c] = *cStrMapGuid++;
					}
					parseGuidBuffer[4] = '\0';
					if(*cStrMapGuid == '-')
						cStrMapGuid++;// skip over the '-' separator
					kassert(errno == 0);
					guidInputMap.Data3 = static_cast<u16>(
						strtol(parseGuidBuffer, nullptr, 16));
					if (errno == ERANGE)
					{
						KLOG(ERROR, "strtoll range error!");
						errno = 0;
					}
				}break;
				case 3:// Guid::Data4 is an array[8] of u8 (2 hex chars each)
				{
					parseGuidBuffer[2] = '\0';
					for(u8 c = 0; c < 8; c++)
					{
						if(*cStrMapGuid == '-')
							cStrMapGuid++;// skip over any '-' separators
						parseGuidBuffer[0] = *cStrMapGuid++;
						parseGuidBuffer[1] = *cStrMapGuid++;
						kassert(errno == 0);
						guidInputMap.Data4[c] = static_cast<u8>(
							strtol(parseGuidBuffer, nullptr, 16));
						if (errno == ERANGE)
						{
							KLOG(ERROR, "strtoll range error!");
							errno = 0;
						}
					}
				}break;
			}
		}
		if(cStrMapGuid != cStrMapGuidEnd)
		{
			KLOG(ERROR, "Error parsing DirectInput=>GamePad map string! '%s'",
			     DIRECT_INPUT_TO_GAMEPAD_MAPS[gpm]);
			continue;
		}
		/* compare lpddi's product GUID to the gamepad map's product GUID */
		if(IsEqualGUID(guidInputMap, lpddi->guidProduct))
		{
			g_dInputDeviceGamePadMapIndices[firstEmptyInputDevice] = gpm;
			break;
		}
	}
	if(g_dInputDeviceGamePadMapIndices[firstEmptyInputDevice] == 
		CARRAY_COUNT(DIRECT_INPUT_TO_GAMEPAD_MAPS))
	{
		KLOG(WARNING, "Failed to locate an appropriate DirectInput=>GamePad "
		     "map!  GamePad for this device's slot will not be plugged in.");
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
/**
 * @return normalized dIAxis value
 */
internal f32 w32ProcessDInputAxis(LONG dIAxis, u16 deadzoneMagnitude)
{
	if(dIAxis < -0x7FFF) dIAxis = -0x7FFF;
	if(dIAxis < -0x7FFF) dIAxis = -0x7FFF;
	if(abs(dIAxis) <= deadzoneMagnitude)
		return 0.f;
	const f32 axisNorm = dIAxis / fabsf(static_cast<f32>(dIAxis));
	local_persist const f32 MAX_AXIS_MAG = static_cast<f32>(0x7FFF);
	const f32 axisMag = (fabsf(static_cast<f32>(dIAxis)) - deadzoneMagnitude) /
	                    (MAX_AXIS_MAG - deadzoneMagnitude);
	return axisNorm*axisMag;
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
		to test the direction of the button's vector and the POV's vector.  The 
		dot product is compared to a value in the range of (0, 0.5) to activate 
		buttons if an adjacent diagonal angle is being pressed, and prevent 
		buttons from being considered active if they are >= 90 degrees from the 
		pov switch position */
	const bool buttonPressed = v2dPov.dot(v2dButton) >= 0.1;
	w32ProcessDInputButton(buttonPressed, buttonStatePrevious, 
	                       o_buttonStateCurrent);
}
enum class DirectInputPadInputType
	{ NO_MAPPED_INPUT
	, BUTTON
	, AXIS
	, POV_HAT
};
struct DirectInputButton
{
	DirectInputPadInputType inputType;
	u16 buttonIndex;
};
struct DirectInputAxis
{
	DirectInputPadInputType inputType;
	u16 axisIndex;
	bool invertAxis;
};
struct DirectInputPovHat
{
	DirectInputPadInputType inputType;
	u16 povHatIndex;
	DWORD buttonCentiDegreesCwFromNorth;
};
union DirectInputPadInput
{
	DirectInputPadInputType inputType;
	DirectInputButton button;
	DirectInputAxis axis;
	DirectInputPovHat povHat;
};
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
		if(g_dInputDeviceGamePadMapIndices[d] == 
			CARRAY_COUNT(DIRECT_INPUT_TO_GAMEPAD_MAPS))
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
		/* parse the DIRECT_INPUT_TO_GAMEPAD_MAPS entry for this device */
		/* @optimization: store this mapping globally ONCE when the map for this 
			device is discovered instead of parsing every frame */
		const size_t gamePadMapIndex = g_dInputDeviceGamePadMapIndices[d];
		const char* nextGamePadMapChar = 
			strrchr(DIRECT_INPUT_TO_GAMEPAD_MAPS[gamePadMapIndex], '}');
		kassert(nextGamePadMapChar);
		nextGamePadMapChar++;// skip the '}' character 
		DirectInputPadInput 
			diJoyStateGamePadButtonInputs[CARRAY_COUNT(GamePad::buttons)];
		DirectInputPadInput 
			diJoyStateGamePadAxisInputs[CARRAY_COUNT(GamePad::axes)];
		for(size_t b = 0; b < CARRAY_COUNT(GamePad::buttons); b++)
		{
			diJoyStateGamePadButtonInputs[b].inputType = 
				DirectInputPadInputType::NO_MAPPED_INPUT;
		}
		for(size_t a = 0; a < CARRAY_COUNT(GamePad::axes); a++)
		{
			diJoyStateGamePadAxisInputs[a].inputType = 
				DirectInputPadInputType::NO_MAPPED_INPUT;
		}
		while(*nextGamePadMapChar)
		{
			/* determine where the end of the current entry is so we can update 
				the nextGamePadMapChar to this location for the next entry */
			const char* cStrEntryEnd = strchr(nextGamePadMapChar, ',');
			if(!cStrEntryEnd)
				cStrEntryEnd = nextGamePadMapChar + strlen(nextGamePadMapChar);
			switch(*nextGamePadMapChar)
			{
				case 'b':// DInputButton=>GamePadButton map entry
				/* entry format: 
					b<DInput button index>:<GamePad button index> */
				{
					nextGamePadMapChar++;// skip over the 'b' character
					char intBuffer[16];
					/* extract the DInput button index */
					intBuffer[CARRAY_COUNT(intBuffer) - 1] = '\0';
					for(u8 c = 0; c < CARRAY_COUNT(intBuffer) - 1; c++)
					{
						intBuffer[c] = *nextGamePadMapChar++;
						if(*nextGamePadMapChar == ':')
						{
							nextGamePadMapChar++;
							intBuffer[c + 1] = '\0';
							break;
						}
					}
					const u16 buttonIndexDInput = 
						kmath::safeTruncateU16(atoi(intBuffer));
					/* extract the GamePad button index */
					for(u8 c = 0; c < CARRAY_COUNT(intBuffer) - 1; c++)
					{
						intBuffer[c] = *nextGamePadMapChar++;
						if(*nextGamePadMapChar == ',' 
							|| *nextGamePadMapChar == '\0')
						{
							if(*nextGamePadMapChar == ',')
								nextGamePadMapChar++;
							intBuffer[c + 1] = '\0';
							break;
						}
					}
					const u16 buttonIndexGamePad = 
						kmath::safeTruncateU16(atoi(intBuffer));
					/* populate the button map list */
					if(buttonIndexDInput < CARRAY_COUNT(DIJOYSTATE::rgbButtons))
					/* the DInput button index refers to a rgbButtons index */
					{
						diJoyStateGamePadButtonInputs[buttonIndexGamePad]
							.inputType = DirectInputPadInputType::BUTTON;
						diJoyStateGamePadButtonInputs[buttonIndexGamePad]
							.button.buttonIndex = buttonIndexDInput;
					}
					else
					/* the DInput button index refers to a pov hat switch 
						direction */
					{
						diJoyStateGamePadButtonInputs[buttonIndexGamePad]
							.inputType = DirectInputPadInputType::POV_HAT;
						diJoyStateGamePadButtonInputs[buttonIndexGamePad]
							.povHat.povHatIndex = (buttonIndexDInput - 
								CARRAY_COUNT(DIJOYSTATE::rgbButtons)) / 4;
						const u16 povCwDirectionIndex = (buttonIndexDInput - 
							CARRAY_COUNT(DIJOYSTATE::rgbButtons)) % 4;
						diJoyStateGamePadButtonInputs[buttonIndexGamePad]
							.povHat.buttonCentiDegreesCwFromNorth = 
								povCwDirectionIndex*9000;
					}
				}break;
				case '+':// DInputAxis=>GamePadAxis map entry
				case '-':
				/* entry format:
					[+-]<DInput axis index>:<GamePad axis index> */
				{
					const bool positiveDInputAxis = *nextGamePadMapChar == '+';
					*nextGamePadMapChar++;// skip over the '+' or '-' character
					char intBuffer[16];
					/* extract the DInput axis index */
					intBuffer[CARRAY_COUNT(intBuffer) - 1] = '\0';
					for(u8 c = 0; c < CARRAY_COUNT(intBuffer) - 1; c++)
					{
						intBuffer[c] = *nextGamePadMapChar++;
						if(*nextGamePadMapChar == ':')
						{
							nextGamePadMapChar++;
							intBuffer[c + 1] = '\0';
							break;
						}
					}
					const u16 axisIndexDInput = 
						kmath::safeTruncateU16(atoi(intBuffer));
					/* extract the GamePad axis index */
					for(u8 c = 0; c < CARRAY_COUNT(intBuffer) - 1; c++)
					{
						intBuffer[c] = *nextGamePadMapChar++;
						if(*nextGamePadMapChar == ',' 
							|| *nextGamePadMapChar == '\0')
						{
							if(*nextGamePadMapChar == ',')
								nextGamePadMapChar++;
							intBuffer[c + 1] = '\0';
							break;
						}
					}
					const u16 axisIndexGamePad = 
						kmath::safeTruncateU16(atoi(intBuffer));
					/* populate the list of address offsets */
					diJoyStateGamePadAxisInputs[axisIndexGamePad]
						.inputType = DirectInputPadInputType::AXIS;
					diJoyStateGamePadAxisInputs[axisIndexGamePad]
						.axis.axisIndex = axisIndexDInput;
					diJoyStateGamePadAxisInputs[axisIndexGamePad]
						.axis.invertAxis = !positiveDInputAxis;
				}break;
				default:
				{
					KLOG(ERROR, "Invalid DInput=>GamePad map entry!");
				}break;
			}
			nextGamePadMapChar = cStrEntryEnd;
			// skip the next entry separator if it exists
			if(*nextGamePadMapChar == ',')
				nextGamePadMapChar++;
		}
		/* get the state of the controller */
		DIJOYSTATE joyState;
		ZeroMemory(&joyState, sizeof(joyState));
		const HRESULT hResultGetState = 
			g_dInputDevices[d]->GetDeviceState(sizeof(joyState), &joyState);
		if(hResultGetState != DI_OK)
		{
			KLOG(WARNING, "Failed to get device state!");
			gpArrCurrFrame[d].type = GamePadType::UNPLUGGED;
			continue;
		}
		gpArrCurrFrame[d].type = GamePadType::DINPUT_GENERIC;
		/* use the DInput=>GamePad map to populate the GamePad with appropriate 
			state  */
		for(u8 b = 0; b < CARRAY_COUNT(GamePad::buttons); b++)
		{
			switch(diJoyStateGamePadButtonInputs[b].inputType)
			{
				case DirectInputPadInputType::BUTTON:
				{
					const u16 dInputButtonIndex = 
						diJoyStateGamePadButtonInputs[b].button.buttonIndex;
					const bool dInputButtonPressed = DINPUT_BUTTON_PRESSED(
						joyState.rgbButtons[dInputButtonIndex]);
					w32ProcessDInputButton(dInputButtonPressed, 
					                       gpArrPrevFrame[d].buttons[b], 
					                       &gpArrCurrFrame[d].buttons[b]);
				}break;
				case DirectInputPadInputType::POV_HAT:
				{
					w32ProcessDInputPovButton(
						joyState.rgdwPOV[diJoyStateGamePadButtonInputs[b]
							.povHat.povHatIndex], 
						diJoyStateGamePadButtonInputs[b].povHat
							.buttonCentiDegreesCwFromNorth, 
						gpArrPrevFrame[d].buttons[b], 
						&gpArrCurrFrame[d].buttons[b]);
				}break;
				case DirectInputPadInputType::NO_MAPPED_INPUT:
				{
					// just skip this input if it isn't mapped
				}break;
				default:
				{
					KLOG(ERROR, "DirectInputPadInputType(%i) not implemented "
					     "for buttons!", 
					     diJoyStateGamePadButtonInputs[b].inputType);
				}break;
			}
		}
		for(u8 a = 0; a < CARRAY_COUNT(GamePad::axes); a++)
		{
			switch(diJoyStateGamePadAxisInputs[a].inputType)
			{
				case DirectInputPadInputType::AXIS:
				{
					local_persist const u16 DEADZONE = 
						XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 2;
					/* because all of the DIJOYSTATE axes are contiguous in 
						memory, we can just refer to them using a single index, 
						treating the DIJOYSTATE as an array of LONGs */
					LONG* pDiAxis = reinterpret_cast<LONG*>(&joyState) + 
						diJoyStateGamePadAxisInputs[a].axis.axisIndex;
					gpArrCurrFrame[d].axes[a] = 
						w32ProcessDInputAxis(*pDiAxis, DEADZONE);
					if(diJoyStateGamePadAxisInputs[a].axis.invertAxis)
						gpArrCurrFrame[d].axes[a] *= -1;
				}break;
				case DirectInputPadInputType::NO_MAPPED_INPUT:
				{
					// just skip this input if it isn't mapped
				}break;
				default:
				{
					KLOG(ERROR, "DirectInputPadInputType(%i) not implemented "
					     "for axes!", 
					     diJoyStateGamePadAxisInputs[a].inputType);
				}break;
			}
		}
		/* this simulation of the trigger states should only ever happen if the 
			controller has L2/R2 buttons and NOT analog buttons */
		const size_t axisIndexTriggerLeft = 
			static_cast<size_t>(&gpArrCurrFrame[d].triggerLeft - 
			                    &gpArrCurrFrame[d].axes[0]);
		const size_t axisIndexTriggerRight = 
			static_cast<size_t>(&gpArrCurrFrame[d].triggerRight - 
			                    &gpArrCurrFrame[d].axes[0]);
		if(diJoyStateGamePadAxisInputs[axisIndexTriggerLeft].inputType == 
			DirectInputPadInputType::NO_MAPPED_INPUT)
		{
			if(gpArrCurrFrame[d].shoulderLeft2 > ButtonState::NOT_PRESSED)
				gpArrCurrFrame[d].triggerLeft = 1.f;
			else
				gpArrCurrFrame[d].triggerLeft = 0.f;
		}
		if(diJoyStateGamePadAxisInputs[axisIndexTriggerRight].inputType == 
			DirectInputPadInputType::NO_MAPPED_INPUT)
		{
			if(gpArrCurrFrame[d].shoulderRight2 > ButtonState::NOT_PRESSED)
				gpArrCurrFrame[d].triggerRight = 1.f;
			else
				gpArrCurrFrame[d].triggerRight = 0.f;
		}
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
	u16 result = INVALID_PLATFORM_BUTTON_INDEX;
	for(u16 b = 0; b < CARRAY_COUNT(joyState.rgbButtons); b++)
	{
		if(DINPUT_BUTTON_PRESSED(joyState.rgbButtons[b]))
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
				if(result == INVALID_PLATFORM_BUTTON_INDEX)
				{
					result = kmath::safeTruncateU16(
						CARRAY_COUNT(joyState.rgbButtons) + pov*4 + 
						povDirection);
				}
				else
				{
					return INVALID_PLATFORM_BUTTON_INDEX;
				}
			}
		}
	}
	return result;
}
internal PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(w32DInputGetGamePadActiveAxis)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_dInputDevices));
	if(gamePadIndex >= CARRAY_COUNT(g_dInputDevices))
		return {INVALID_PLATFORM_AXIS_INDEX};
	LPDIRECTINPUTDEVICE8 dInput8Device = g_dInputDevices[gamePadIndex];
	if(!dInput8Device)
		return {INVALID_PLATFORM_AXIS_INDEX};
	/* poll the device for current input state */
	const HRESULT hResultPoll = dInput8Device->Poll();
	if(!(hResultPoll == DI_OK || hResultPoll == DI_NOEFFECT))
	/* poll failed means the device is no longer acquired, attempt to 
		re-acquire */
	{
		const HRESULT hResultAcquire = dInput8Device->Acquire();
		if(hResultAcquire == DIERR_OTHERAPPHASPRIO)
		{
			return {INVALID_PLATFORM_AXIS_INDEX};
		}
		if(hResultAcquire != DI_OK)
		{
			KLOG(WARNING, "Failed to re-acquire device! Unacquiring...");
			g_dInputDevices[gamePadIndex]->Unacquire();
			g_dInputDevices[gamePadIndex]->Release();
			g_dInputDevices[gamePadIndex] = nullptr;
			return {INVALID_PLATFORM_AXIS_INDEX};
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
		return {INVALID_PLATFORM_AXIS_INDEX};
	}
	/* at this point, we should have the state of the controller and we can 
		actually check to see what is being pressed/moved: */
	f32 axes[8];
	///TODO: use DirectInput-specific deadzone here???
	local_persist const u16 DEADZONE_MAG = 
		XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 2;
	axes[0] = w32ProcessDInputAxis(joyState.lX          , DEADZONE_MAG);
	axes[1] = w32ProcessDInputAxis(joyState.lY          , DEADZONE_MAG);
	axes[2] = w32ProcessDInputAxis(joyState.lZ          , DEADZONE_MAG);
	axes[3] = w32ProcessDInputAxis(joyState.lRx         , DEADZONE_MAG);
	axes[4] = w32ProcessDInputAxis(joyState.lRy         , DEADZONE_MAG);
	axes[5] = w32ProcessDInputAxis(joyState.lRz         , DEADZONE_MAG);
	axes[6] = w32ProcessDInputAxis(joyState.rglSlider[0], DEADZONE_MAG);
	axes[7] = w32ProcessDInputAxis(joyState.rglSlider[1], DEADZONE_MAG);
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
internal PLATFORM_GET_GAME_PAD_PRODUCT_NAME(w32DInputGetGamePadProductName)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_dInputDevices));
	if(gamePadIndex >= CARRAY_COUNT(g_dInputDevices))
	{
		StringCchPrintf(o_buffer, bufferSize, 
		                TEXT("DInput pad [%i] out of bounds!"), gamePadIndex);
		return;
	}
	LPDIRECTINPUTDEVICE8 dInput8Device = g_dInputDevices[gamePadIndex];
	kassert(dInput8Device);
	if(!dInput8Device)
	{
		StringCchPrintf(o_buffer, bufferSize, 
		                TEXT("DInput pad doesn't exist!"));
		return;
	}
	DIDEVICEINSTANCE deviceInstance;
	deviceInstance.dwSize = sizeof(deviceInstance);
	const HRESULT hResult = dInput8Device->GetDeviceInfo(&deviceInstance);
	if(hResult != DI_OK)
	{
		StringCchPrintf(o_buffer, bufferSize, TEXT("GetDeviceInfo FAILED!"));
		return;
	}
	StringCchPrintf(o_buffer, bufferSize, TEXT("%s"), 
	                deviceInstance.tszProductName);
}
internal PLATFORM_GET_GAME_PAD_PRODUCT_GUID(w32DInputGetGamePadProductGuid)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_dInputDevices));
	if(gamePadIndex >= CARRAY_COUNT(g_dInputDevices))
	{
		StringCchPrintf(o_buffer, bufferSize, 
		                TEXT("DInput pad [%i] out of bounds!"), gamePadIndex);
		return;
	}
	LPDIRECTINPUTDEVICE8 dInput8Device = g_dInputDevices[gamePadIndex];
	kassert(dInput8Device);
	if(!dInput8Device)
	{
		StringCchPrintf(o_buffer, bufferSize, 
		                TEXT("DInput pad doesn't exist!"));
		return;
	}
	DIDEVICEINSTANCE deviceInstance;
	deviceInstance.dwSize = sizeof(deviceInstance);
	const HRESULT hResult = dInput8Device->GetDeviceInfo(&deviceInstance);
	if(hResult != DI_OK)
	{
		StringCchPrintf(o_buffer, bufferSize, TEXT("GetDeviceInfo FAILED!"));
		return;
	}
	StringCchPrintf(o_buffer, bufferSize, TEXT(GUID_FORMAT), 
	                GUID_ARG(deviceInstance.guidProduct));
}