#include "win32-krb-opengl.h"
#include <GL/GL.h>
#define WGL_GET_EXTENSIONS_STRING_ARB(name) const char* WINAPI name(HDC hdc)
#define WGL_SWAP_INTERVAL(name) BOOL WINAPI name(int interval)
#define WGL_GET_SWAP_INTERVAL(name) int WINAPI name(void)
typedef WGL_GET_EXTENSIONS_STRING_ARB(fnSig_wglGetExtensionsStringARB);
typedef WGL_SWAP_INTERVAL(fnSig_wglSwapIntervalEXT);
typedef WGL_GET_SWAP_INTERVAL(fnSig_wglGetSwapIntervalEXT);
global_variable fnSig_wglGetExtensionsStringARB* wglGetExtensionsStringARB;
global_variable fnSig_wglSwapIntervalEXT* wglSwapIntervalEXT;
global_variable fnSig_wglGetSwapIntervalEXT* wglGetSwapIntervalEXT;
internal void w32KrbOglInitialize(HWND hwnd)
{
	PIXELFORMATDESCRIPTOR desiredPixelFormat = {};
	desiredPixelFormat.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
	desiredPixelFormat.nVersion   = 1;
	desiredPixelFormat.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | 
	                                PFD_DOUBLEBUFFER;
	desiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
	desiredPixelFormat.cColorBits = 24;
	desiredPixelFormat.iLayerType = PFD_MAIN_PLANE;
	HDC windowDc = GetDC(hwnd);
	const int pixelFormatIndex = 
		ChoosePixelFormat(windowDc, &desiredPixelFormat);
	if(pixelFormatIndex == 0)
	{
		KLOG_ERROR("Failed to choose pixel format! GetLastError=%i",
		           GetLastError());
	}
	// verify that the OS-provided pixel format is good enough for our needs //
	{
		PIXELFORMATDESCRIPTOR providedPixelFormat;
		const int maxPixelFormatIndex = 
			DescribePixelFormat(windowDc, pixelFormatIndex, 
			                    sizeof(providedPixelFormat), 
			                    &providedPixelFormat);
		if(maxPixelFormatIndex == 0)
		{
			KLOG_ERROR("Failed to describe pixel format! GetLastError=%i",
			           GetLastError());
		}
		if((providedPixelFormat.iPixelType != desiredPixelFormat.iPixelType) ||
			(providedPixelFormat.cColorBits < desiredPixelFormat.cColorBits))
		{
			KLOG_ERROR("Invalid provided pixel format!");
			///TODO: do a more robust job of this for shipping
		}
	}
	if(!SetPixelFormat(windowDc, pixelFormatIndex, &desiredPixelFormat))
	{
		KLOG_ERROR("Failed to set pixel format! GetLastError=%i", 
		           GetLastError());
	}
	HGLRC openGlRc = wglCreateContext(windowDc);
	if(!wglMakeCurrent(windowDc, openGlRc))
	{
		KLOG_ERROR("Failed to set the current OpenGL render context! "
		           "GetLastError=%i", GetLastError());
	}
	wglGetExtensionsStringARB = 
		reinterpret_cast<fnSig_wglGetExtensionsStringARB*>(
			wglGetProcAddress("wglGetExtensionsStringARB"));
	if(!wglGetExtensionsStringARB)
	{
		KLOG_ERROR("Failed to get wglGetExtensionsStringARB! "
		           "GetLastError=%i", GetLastError());
	}
	const char*const wglExtensionsString = wglGetExtensionsStringARB(windowDc);
	ReleaseDC(hwnd, windowDc);
	if(!wglExtensionsString)
	{
		KLOG_ERROR("Failed to get wgl extensions string! "
		           "GetLastError=%i", GetLastError());
	}
	KLOG_INFO("wglExtensionsString='%s'", wglExtensionsString);
	if(strstr(wglExtensionsString, "WGL_EXT_swap_control"))
	{
		wglSwapIntervalEXT =
			reinterpret_cast<fnSig_wglSwapIntervalEXT*>(
				wglGetProcAddress("wglSwapIntervalEXT"));
		if(!wglSwapIntervalEXT)
		{
			KLOG_ERROR("Failed to get 'wglSwapIntervalEXT'! "
			           "GetLastError=%i", GetLastError());
		}
		wglGetSwapIntervalEXT =
			reinterpret_cast<fnSig_wglGetSwapIntervalEXT*>(
				wglGetProcAddress("wglGetSwapIntervalEXT"));
		if(!wglGetSwapIntervalEXT)
		{
			KLOG_ERROR("Failed to get 'wglGetSwapIntervalEXT'! "
			           "GetLastError=%i", GetLastError());
		}
	}
	else
	{
		KLOG_WARNING("System does not contain 'WGL_EXT_swap_control'; the "
		             "application will not have vertical-sync capabilities!");
	}
}
internal void w32KrbOglSetVSyncPreference(bool value)
{
	if(!wglSwapIntervalEXT)
	{
		KLOG_WARNING("System failed to initialize vertical-sync capabilities! "
		             "Ignoring request...");
		return;
	}
	const int desiredInterval = value ? 1 : 0;
	if(!wglSwapIntervalEXT(desiredInterval))
	{
		KLOG_ERROR("Failed call wglSwapIntervalEXT with a value of %i! "
		           "GetLastError=%i", desiredInterval, GetLastError());
	}
}
internal bool w32KrbOglGetVSync()
{
	if(!wglGetSwapIntervalEXT)
	{
		return false;
	}
	return wglGetSwapIntervalEXT() == 1;
}