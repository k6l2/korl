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
		kassert(!"Failed to choose pixel format!");
		///TODO: handle GetLastError
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
			kassert(!"Failed to describe pixel format!");
			///TODO: handle GetLastError
		}
		if((providedPixelFormat.iPixelType != desiredPixelFormat.iPixelType) ||
			(providedPixelFormat.cColorBits < desiredPixelFormat.cColorBits))
		{
			kassert(!"Invalid provided pixel format!");
			///TODO: log error
			///TODO: do a more robust job of this for shipping
		}
	}
	if(!SetPixelFormat(windowDc, pixelFormatIndex, &desiredPixelFormat))
	{
		kassert(!"Failed to set pixel format!");
		///TODO: log error
	}
	HGLRC openGlRc = wglCreateContext(windowDc);
	if(!wglMakeCurrent(windowDc, openGlRc))
	{
		kassert(!"Failed to set the current OpenGL render context!");
		///TODO: handle error
	}
	wglGetExtensionsStringARB = 
		reinterpret_cast<fnSig_wglGetExtensionsStringARB*>(
			wglGetProcAddress("wglGetExtensionsStringARB"));
	if(!wglGetExtensionsStringARB)
	{
		kassert(!"Failed to get wglGetExtensionsStringARB!");
		///TODO: handle GetLastError
	}
	const char*const wglExtensionsString = wglGetExtensionsStringARB(windowDc);
	ReleaseDC(hwnd, windowDc);
	if(!wglExtensionsString)
	{
		kassert(!"Failed to get wgl extensions string!");
		///TODO: handle GetLastError
	}
#if INTERNAL_BUILD
	platformPrintDebugString(wglExtensionsString);
#endif
	if(strstr(wglExtensionsString, "WGL_EXT_swap_control"))
	{
		wglSwapIntervalEXT =
			reinterpret_cast<fnSig_wglSwapIntervalEXT*>(
				wglGetProcAddress("wglSwapIntervalEXT"));
		if(!wglSwapIntervalEXT)
		{
			kassert(!"Failed to get wglSwapIntervalEXT!");
			///TODO: handle GetLastError
		}
		wglGetSwapIntervalEXT =
			reinterpret_cast<fnSig_wglGetSwapIntervalEXT*>(
				wglGetProcAddress("wglGetSwapIntervalEXT"));
		if(!wglGetSwapIntervalEXT)
		{
			kassert(!"Failed to get wglGetSwapIntervalEXT!");
			///TODO: handle GetLastError
		}
	}
	else
	{
		///TODO: log warning.  Application should still be able to function w/o
		///      the ability to use vertical-sync
	}
}
internal void w32KrbOglSetVSyncPreference(bool value)
{
	if(!wglSwapIntervalEXT)
	{
		return;
	}
	if(!wglSwapIntervalEXT(value ? 1 : 0))
	{
		///TODO: handle GetLastError
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
internal KRB_BEGIN_FRAME(krbBeginFrame)
{
	glClearColor(clamped0_1_red, clamped0_1_green, clamped0_1_blue, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
}
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho)
{
	glOrtho(-windowSizeX/2, windowSizeX/2, -windowSizeY/2, windowSizeY/2, 
	        halfDepth, -halfDepth);
}
internal KRB_DRAW_LINE(krbDrawLine)
{
	glBegin(GL_LINES);
	glColor4f(color.r, color.g, color.b, color.a);
	glVertex2f(p0.x, p0.y);
	glVertex2f(p1.x, p1.y);
	glEnd();
}