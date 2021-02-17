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
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}
#define WGL_CONTEXT_MAJOR_VERSION_ARB             0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB             0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB               0x2093
#define WGL_CONTEXT_FLAGS_ARB                     0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB                 0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB    0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_GET_EXTENSIONS_STRING_ARB(name) const char* WINAPI name(HDC hdc)
#define WGL_CREATE_CONTEXT_ATTRIBS_ARB(name) \
	HGLRC name(HDC hDC, HGLRC hShareContext, const int *attribList)
#define WGL_SWAP_INTERVAL(name) BOOL WINAPI name(int interval)
#define WGL_GET_SWAP_INTERVAL(name) int WINAPI name(void)
typedef WGL_GET_EXTENSIONS_STRING_ARB(fnSig_wglGetExtensionsStringARB);
typedef WGL_CREATE_CONTEXT_ATTRIBS_ARB(fnSig_wglCreateContextAttribsARB);
typedef WGL_SWAP_INTERVAL(fnSig_wglSwapIntervalEXT);
typedef WGL_GET_SWAP_INTERVAL(fnSig_wglGetSwapIntervalEXT);
global_variable fnSig_wglGetExtensionsStringARB* wglGetExtensionsStringARB;
global_variable fnSig_wglCreateContextAttribsARB* wglCreateContextAttribsARB;
global_variable fnSig_wglSwapIntervalEXT* wglSwapIntervalEXT;
global_variable fnSig_wglGetSwapIntervalEXT* wglGetSwapIntervalEXT;
internal void w32KrbOglInitialize(HWND hwnd)
{
	PIXELFORMATDESCRIPTOR desiredPixelFormat = {};
	desiredPixelFormat.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
	desiredPixelFormat.nVersion     = 1;
	desiredPixelFormat.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL 
	                                  | PFD_DOUBLEBUFFER;
	desiredPixelFormat.iPixelType   = PFD_TYPE_RGBA;
	desiredPixelFormat.cColorBits   = 32;
	desiredPixelFormat.cDepthBits   = 24;
	desiredPixelFormat.cStencilBits = 8;
	desiredPixelFormat.iLayerType   = PFD_MAIN_PLANE;
	HDC windowDc = GetDC(hwnd);
	if(!windowDc)
		KLOG(ERROR, "Failed to get window DC!");
	defer(ReleaseDC(hwnd, windowDc));
	const int pixelFormatIndex = 
		ChoosePixelFormat(windowDc, &desiredPixelFormat);
	if(pixelFormatIndex == 0)
		KLOG(ERROR, "Failed to choose pixel format! GetLastError=%i",
			GetLastError());
	// verify that the OS-provided pixel format is good enough for our needs //
	{
		PIXELFORMATDESCRIPTOR providedPixelFormat;
		const int maxPixelFormatIndex = 
			DescribePixelFormat(
				windowDc, pixelFormatIndex, 
				sizeof(providedPixelFormat), &providedPixelFormat);
		if(maxPixelFormatIndex == 0)
			KLOG(ERROR, "Failed to describe pixel format! GetLastError=%i",
				GetLastError());
		if(    providedPixelFormat.iPixelType != desiredPixelFormat.iPixelType 
			|| providedPixelFormat.cColorBits <  desiredPixelFormat.cColorBits) 
			/* @todo: do a more robust job of this for shipping */
			KLOG(ERROR, "Invalid provided pixel format!");
	}
	if(!SetPixelFormat(windowDc, pixelFormatIndex, &desiredPixelFormat))
		KLOG(ERROR, "Failed to set pixel format! GetLastError=%i", 
			GetLastError());
	/* In order to create a "REAL" OpenGL context, we need to obtain function 
		pointers to functions described in WGL extensions.  In order to do this, 
		we have to create a context which will be discarded afterwards. */
	HGLRC dummyGlRc = wglCreateContext(windowDc);
	if(!dummyGlRc)
		KLOG(ERROR, "Failed to create context! GetLastError=%i", 
			GetLastError());
	if(!wglMakeCurrent(windowDc, dummyGlRc))
		KLOG(ERROR, "Failed to set the current OpenGL render context! "
			"GetLastError=%i", GetLastError());
	/* get a string from WGL with a list of extensions supported by this 
		context */
	wglGetExtensionsStringARB = 
		reinterpret_cast<fnSig_wglGetExtensionsStringARB*>(
			wglGetProcAddress("wglGetExtensionsStringARB"));
	if(!wglGetExtensionsStringARB)
		KLOG(ERROR, "Failed to get wglGetExtensionsStringARB! "
			"GetLastError=%i", GetLastError());
	const char*const wglExtensionsString = wglGetExtensionsStringARB(windowDc);
	if(!wglExtensionsString)
		KLOG(ERROR, "Failed to get wgl extensions string! "
		     "GetLastError=%i", GetLastError());
	KLOG(INFO, "wglExtensionsString='%s'", wglExtensionsString);
	/* get WGL extensions */
	if(strstr(wglExtensionsString, "WGL_ARB_create_context"))
	{
		wglCreateContextAttribsARB = 
			reinterpret_cast<fnSig_wglCreateContextAttribsARB*>(
				wglGetProcAddress("wglCreateContextAttribsARB"));
		if(!wglCreateContextAttribsARB)
			KLOG(ERROR, "Failed to get 'wglCreateContextAttribsARB'! "
				"GetLastError=%i", GetLastError());
	}
	else
		KLOG(ERROR, "System does not contain 'WGL_ARB_create_context'; unable "
			"to properly obtain an OpenGL context!");
	if(strstr(wglExtensionsString, "WGL_EXT_swap_control"))
	{
		wglSwapIntervalEXT =
			reinterpret_cast<fnSig_wglSwapIntervalEXT*>(
				wglGetProcAddress("wglSwapIntervalEXT"));
		if(!wglSwapIntervalEXT)
			KLOG(ERROR, "Failed to get 'wglSwapIntervalEXT'! "
				"GetLastError=%i", GetLastError());
		wglGetSwapIntervalEXT =
			reinterpret_cast<fnSig_wglGetSwapIntervalEXT*>(
				wglGetProcAddress("wglGetSwapIntervalEXT"));
		if(!wglGetSwapIntervalEXT)
			KLOG(ERROR, "Failed to get 'wglGetSwapIntervalEXT'! "
				"GetLastError=%i", GetLastError());
	}
	else
		KLOG(WARNING, "System does not contain 'WGL_EXT_swap_control'; the "
			"application will not have vertical-sync capabilities!");
	/* now that we have the wglCreateContextAttribsARB function, we can create 
		the REAL OpenGL context and discard the dummy context used to just get 
		these WGL extensions */
	/* @todo: If we ever want to change more advanced settings with respect to 
		the window's pixel format (using WGL_ARB_pixel_format, etc), we're going 
		to have to do some bs where we destroy the window and create a new one 
		after using the pixel format WGL extensions and storing the output.  
		This is because windows can only set the pixel format ONCE in their 
		entire lifetime!  Perhaps we can have code in win32-krb-opengl which 
		creates a window in the smallest amount of code which then extracts WGL 
		extensions & passes the results of pixel format extension calls on the 
		dummy hdc back to this thread.  See:
		https://www.khronos.org/opengl/wiki/Creating_an_OpenGL_Context_(WGL)#Proper_Context_Creation */
	const int realGlRcAttributeList[] = 
		{ WGL_CONTEXT_MAJOR_VERSION_ARB, 3
		, WGL_CONTEXT_MINOR_VERSION_ARB, 3
		, WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#if SLOW_BUILD
		, WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB
#endif//SLOW_BUILD
		, 0 };
	HGLRC realGlRc = 
		wglCreateContextAttribsARB(windowDc, NULL, realGlRcAttributeList);
	if(!realGlRc)
		KLOG(ERROR, "Failed to create context! GetLastError=%i", 
			GetLastError());
	if(!wglMakeCurrent(windowDc, realGlRc))
		KLOG(ERROR, "Failed to set the current OpenGL render context! "
			"GetLastError=%i", GetLastError());
	if(!wglDeleteContext(dummyGlRc))
		KLOG(ERROR, "Failed to delete dummy OpenGL render context! "
			"GetLastError=%i", GetLastError());
	/* get CORE OpenGL extensions */
	glGetStringi = 
		reinterpret_cast<PFNGLGETSTRINGIPROC>(
			wglGetProcAddress("glGetStringi"));
	if(!glGetStringi)
		KLOG(ERROR, "Failed to get glGetStringi! "
			"GetLastError=%i", GetLastError());
	glLoadTransposeMatrixf = 
		reinterpret_cast<PFNGLLOADTRANSPOSEMATRIXFPROC>(
			wglGetProcAddress("glLoadTransposeMatrixf"));
	if(!glLoadTransposeMatrixf)
		KLOG(ERROR, "Failed to get glLoadTransposeMatrixf! "
			"GetLastError=%i", GetLastError());
	glMultTransposeMatrixf = 
		reinterpret_cast<PFNGLMULTTRANSPOSEMATRIXFPROC>(
			wglGetProcAddress("glMultTransposeMatrixf"));
	if(!glMultTransposeMatrixf)
		KLOG(ERROR, "Failed to get glMultTransposeMatrixf! "
			"GetLastError=%i", GetLastError());
	/* Our OpenGL context should now be set up, so let's print out some info 
		about it to the log. */
	{
		KLOG(INFO, "----- OpenGL Context -----");
		KLOG(INFO, "vendor: %s", glGetString(GL_VENDOR));
		KLOG(INFO, "renderer: %s", glGetString(GL_RENDERER));
		KLOG(INFO, "version: %s", glGetString(GL_VERSION));
		KLOG(INFO, "glsl version: %s", 
			glGetString(GL_SHADING_LANGUAGE_VERSION));
		GLubyte oglExtensionCStrBuffer[16*1024] = {};
		int oglExtensionCStrBufferSize = 0;
		GLint oglExtensions;
		glGetIntegerv(GL_NUM_EXTENSIONS, &oglExtensions);
		for(GLint e = 0; e < oglExtensions; e++) 
		{
			const GLubyte*const cStrExtension = 
				glGetStringi(GL_EXTENSIONS, e);
			const int charsWritten = sprintf_s(
				reinterpret_cast<char*>(
					oglExtensionCStrBuffer + oglExtensionCStrBufferSize), 
				CARRAY_SIZE(oglExtensionCStrBuffer) - 
					oglExtensionCStrBufferSize, 
				"%s ", cStrExtension);
			if(charsWritten > 0)
				oglExtensionCStrBufferSize += charsWritten;
			/* since we're logging extensions, we might as well obtain advanced 
				extensions that are not guaranteed to be available by the core 
				implementation */
			/* @todo: obtain and implement GL_ARB_debug_output */
#if 0/* OpenGL extension compatibility check example */
			if ( strcmp(ccc, (const GLubyte *)"GL_ARB_debug_output") == 0 )
			{
				// The extension is supported by our hardware and driver
				// Try to get the "glDebugMessageCallbackARB" function :
				glDebugMessageCallbackARB  = 
					(PFNGLDEBUGMESSAGECALLBACKARBPROC) 
						wglGetProcAddress("glDebugMessageCallbackARB");
			}
#endif//0
		}
		KLOG(INFO, "extensions: %s", oglExtensionCStrBuffer);
		KLOG(INFO, "--------------------------");
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