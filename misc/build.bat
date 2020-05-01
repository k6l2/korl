@echo off
rem prerequisites: env.bat
if not exist "%project_root%\build" mkdir %project_root%\build
pushd %project_root%\build
rem --- DEFINES ---
rem     INTERNAL_BUILD = set to 0 to disable all code which should never be 
rem                      shipped to the end-user, such as debug functions etc...
rem     SLOW_BUILD = set to 0 to disable code which typically has the following
rem                  properties:
rem                      -does not affect actual gameplay/program logic
rem                      -most likely will assist with debugging during 
rem                           development
rem                      -incurs a non-zero runtime cost
rem /Zi - Generates complete debugging information.
rem /FC - Display full path of source code files passed to cl.exe in diagnostic 
rem       text.
rem /nologo - prevent the compiler from outputing the compiler version info,
rem           architecture info, and other verbose output messages
rem /std:c++latest - used for C++20 designated initializers
rem user32.lib - ??? various win32 stuff
rem Gdi32.lib - used for windows software drawing operations.  ///TODO: remove
rem             later when using OpenGL or Vulkan backend renderers probably?
cl %project_root%\code\win32-main.cpp ^
	/DINTERNAL_BUILD=1 /DSLOW_BUILD=1 /Zi /FC /nologo /std:c++latest ^
	user32.lib Gdi32.lib
popd
echo Build script finished.