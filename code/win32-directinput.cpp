#include "win32-directinput.h"
#include "win32-main.h"
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
		{
			firstEmptyInputDevice = CARRAY_COUNT(g_dInputDevices);
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
		const HWND hwnd = GetActiveWindow();
		if(!hwnd)
		{
			KLOG(ERROR, "Failed to get active window; "
			            "cannot set cooperative level of directinput device!");
			return;
		}
		const HRESULT hResult = g_dInputDevices[firstEmptyInputDevice]->
			SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_EXCLUSIVE);
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to set direct input device cooperation level! "
			     "'%s':'%s'", lpddi->tszProductName, lpddi->tszInstanceName);
			return;
		}
	}
}
/**
 * @return DIENUM_CONTINUE to continue the enumeration.  DIENUM_STOP to stop the 
 *         enumeration.
 */
internal BOOL DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	if(IsXInputDevice(&lpddi->guidProduct))
	{
		KLOG(INFO, "XInput device detected! Skipping: '%s'", 
		     lpddi->tszProductName);
		return DIENUM_CONTINUE;
	}
	KLOG(INFO, "DInput device detected! '%s'", lpddi->tszProductName);
	w32DInputAddDevice(lpddi);
//	if(IsEqualGUID(lpddi->guidInstance, ))
	return DIENUM_CONTINUE;
}
internal void w32LoadDInput(HINSTANCE hInst)
{
	HMODULE LibDInput = LoadLibraryA("dinput8.dll");
	if(!LibDInput)
	{
		KLOG(WARNING, "Failed to load dinput8.dll! GetLastError=i", 
		     GetLastError());
	}
//	fnSig_dInputCreate* DirectInput8Create = nullptr;
	if(LibDInput)
	{
//		DirectInput8Create = reinterpret_cast<fnSig_dInputCreate*>( 
//			GetProcAddress(LibDInput, "DirectInput8Create") );
//		if(!DirectInput8Create)
//		{
//			KLOG(ERROR, "Failed to get DirectInput8Create! GetLastError=i", 
//			     GetLastError());
//			return;
//		}
#if 0
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
#endif// 0
	}
	else
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
	/* enumerate DirectInput compatible devices */
	{
		const HRESULT hResult = 
			g_pIDInput->EnumDevices(DI8DEVCLASS_GAMECTRL, DIEnumDevicesCallback, 
			                        nullptr, DIEDFL_ATTACHEDONLY);
		if(hResult != DI_OK)
		{
			KLOG(ERROR, "Failed to enumerate DirectInput devices! "
			     "hResult=%li", hResult);
		}
	}
}