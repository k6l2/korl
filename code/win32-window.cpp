#include "win32-window.h"
internal v2u32 w32GetWindowDimensions(HWND hwnd)
{
	RECT clientRect;
	if(GetClientRect(hwnd, &clientRect))
	{
		const u32 w = clientRect.right  - clientRect.left;
		const u32 h = clientRect.bottom - clientRect.top;
		return {w, h};
	}
	else
	{
		KLOG(ERROR, "Failed to get window dimensions! GetLastError=%i", 
			GetLastError());
		return {0, 0};
	}
}
internal DWORD w32QueryNearestMonitorRefreshRate(HWND hwnd)
{
	local_persist const DWORD DEFAULT_RESULT = 60;
	const HMONITOR nearestMonitor = 
		MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEXA monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFOEXA);
	if(!GetMonitorInfoA(nearestMonitor, &monitorInfo))
	{
		KLOG(ERROR, "Failed to get monitor info!");
		return DEFAULT_RESULT;
	}
	DEVMODEA monitorDevMode;
	monitorDevMode.dmSize = sizeof(DEVMODEA);
	monitorDevMode.dmDriverExtra = 0;
	if(!EnumDisplaySettingsA(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, 
	                         &monitorDevMode))
	{
		KLOG(ERROR, "Failed to enum display settings!");
		return DEFAULT_RESULT;
	}
	if(monitorDevMode.dmDisplayFrequency < 2)
	{
		KLOG(WARNING, "Unknown hardware-defined refresh rate! "
		     "Defaulting to %ihz...", DEFAULT_RESULT);
		return DEFAULT_RESULT;
	}
	return monitorDevMode.dmDisplayFrequency;
}