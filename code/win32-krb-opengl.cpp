#include "win32-krb-opengl.h"
#include <GL/GL.h>
#include "krb-opengl-extensions.h"
/* On machines w/ multiple graphics cards, such as laptops, the default behavior 
	of nvidia drivers is to select the integrated graphics card.  In order to 
	force the computer to use the dedicated graphics card, we need to export 
	the `NvOptimusEnablement` symbol with the appropriate value.  See: 
	https://stackoverflow.com/a/39047129 */
extern "C" 
{
//	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}
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
		KLOG(ERROR, "Failed to choose pixel format! GetLastError=%i",
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
			KLOG(ERROR, "Failed to describe pixel format! GetLastError=%i",
			     GetLastError());
		}
		if((providedPixelFormat.iPixelType != desiredPixelFormat.iPixelType) ||
			(providedPixelFormat.cColorBits < desiredPixelFormat.cColorBits))
		{
			KLOG(ERROR, "Invalid provided pixel format!");
			///TODO: do a more robust job of this for shipping
		}
	}
	if(!SetPixelFormat(windowDc, pixelFormatIndex, &desiredPixelFormat))
	{
		KLOG(ERROR, "Failed to set pixel format! GetLastError=%i", 
		     GetLastError());
	}
	HGLRC openGlRc = wglCreateContext(windowDc);
	if(!wglMakeCurrent(windowDc, openGlRc))
	{
		KLOG(ERROR, "Failed to set the current OpenGL render context! "
		     "GetLastError=%i", GetLastError());
	}
	/* Our OpenGL context should now be set up, so let's print out some info 
		about it to the log. */
	{
		KLOG(INFO, "----- OpenGL Context -----");
		KLOG(INFO, "vendor: %s", glGetString(GL_VENDOR));
		KLOG(INFO, "renderer: %s", glGetString(GL_RENDERER));
		KLOG(INFO, "version: %s", glGetString(GL_VERSION));
		KLOG(INFO, "glsl version: %s", 
			glGetString(GL_SHADING_LANGUAGE_VERSION));
		KLOG(INFO, "--------------------------");
	}
	wglGetExtensionsStringARB = 
		reinterpret_cast<fnSig_wglGetExtensionsStringARB*>(
			wglGetProcAddress("wglGetExtensionsStringARB"));
	if(!wglGetExtensionsStringARB)
	{
		KLOG(ERROR, "Failed to get wglGetExtensionsStringARB! "
		     "GetLastError=%i", GetLastError());
	}
	const char*const wglExtensionsString = wglGetExtensionsStringARB(windowDc);
	//ReleaseDC(hwnd, windowDc);
	if(!wglExtensionsString)
	{
		KLOG(ERROR, "Failed to get wgl extensions string! "
		     "GetLastError=%i", GetLastError());
	}
	KLOG(INFO, "wglExtensionsString='%s'", wglExtensionsString);
	if(strstr(wglExtensionsString, "WGL_EXT_swap_control"))
	{
		wglSwapIntervalEXT =
			reinterpret_cast<fnSig_wglSwapIntervalEXT*>(
				wglGetProcAddress("wglSwapIntervalEXT"));
		if(!wglSwapIntervalEXT)
		{
			KLOG(ERROR, "Failed to get 'wglSwapIntervalEXT'! "
			     "GetLastError=%i", GetLastError());
		}
		wglGetSwapIntervalEXT =
			reinterpret_cast<fnSig_wglGetSwapIntervalEXT*>(
				wglGetProcAddress("wglGetSwapIntervalEXT"));
		if(!wglGetSwapIntervalEXT)
		{
			KLOG(ERROR, "Failed to get 'wglGetSwapIntervalEXT'! "
			     "GetLastError=%i", GetLastError());
		}
	}
	else
	{
		KLOG(WARNING, "System does not contain 'WGL_EXT_swap_control'; the "
		     "application will not have vertical-sync capabilities!");
	}
	glLoadTransposeMatrixf = 
		reinterpret_cast<PFNGLLOADTRANSPOSEMATRIXFPROC>(
			wglGetProcAddress("glLoadTransposeMatrixf"));
	if(!glLoadTransposeMatrixf)
	{
		KLOG(ERROR, "Failed to get glLoadTransposeMatrixf! "
		     "GetLastError=%i", GetLastError());
	}
	glMultTransposeMatrixf = 
		reinterpret_cast<PFNGLMULTTRANSPOSEMATRIXFPROC>(
			wglGetProcAddress("glMultTransposeMatrixf"));
	if(!glMultTransposeMatrixf)
	{
		KLOG(ERROR, "Failed to get glMultTransposeMatrixf! "
		     "GetLastError=%i", GetLastError());
	}
}
internal void w32KrbOglSetVSyncPreference(bool value)
{
	if(!wglSwapIntervalEXT)
	{
		KLOG(WARNING, "System failed to initialize vertical-sync capabilities! "
		     "Ignoring request...");
		return;
	}
	const int desiredInterval = value ? 1 : 0;
	if(!wglSwapIntervalEXT(desiredInterval))
	{
		KLOG(ERROR, "Failed call wglSwapIntervalEXT with a value of %i! "
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