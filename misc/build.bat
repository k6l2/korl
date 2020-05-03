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
rem                  Example: debug printing timing info to standard output.
rem /W4 - warning level 4
rem /WX - treat all warnings as errors
rem /wd4100 - disable warning C4100 `unreferenced formal parameter`
rem /wd4201 - disable warning C4201 
rem           `nonstandard extension used: nameless struct/union`
rem /Zi - Generates complete debugging information.
rem /Oi - Generate intrinsic opcodes
rem /Od - Disable all optimization.
rem /GR- - disable RTTI (Runtime Type Information)
rem /EHa- - disable exception handling
rem /FC - Display full path of source code files passed to cl.exe in diagnostic 
rem       text.
rem /MT - statically link to LIBCMT.  This is necessary to eliminate the need to 
rem       dynamically link to libcrt at runtime.
rem /MTd - statically link to LIBCMTD.  This is necessary to eliminate the need 
rem        to dynamically link to libcrt at runtime.
rem /nologo - prevent the compiler from outputing the compiler version info,
rem           architecture info, and other verbose output messages
rem /std:c++latest - used for C++20 designated initializers
rem /Fm<name> - generate a map file
rem /link - all linker options are placed after this token.
rem --- LINKER OPTIONS ---
rem /subsystem - affects the entry point symbol that the linker will select
rem /opt:ref - removes unreferenced packaged functions and data, known as 
rem            COMDATs. This optimization is known as transitive COMDAT 
rem            elimination. The /OPT:REF option also disables incremental 
rem            linking.
rem user32.lib - ??? various win32 stuff
rem Gdi32.lib - used for windows software drawing operations.  ///TODO: remove
rem             later when using OpenGL or Vulkan backend renderers probably?
set CommonCompilerFlags=/Fmwin32-main.map ^
	/DINTERNAL_BUILD=1 /DSLOW_BUILD=1 ^
	/MTd /W4 /WX /wd4100 /wd4201 /Oi /Od /GR- /EHa- /Zi /FC ^
	/nologo /std:c++latest
set CommonLinkerFlags=/opt:ref user32.lib Gdi32.lib
rem 32-bit build
rem cl %project_root%\code\win32-main.cpp %CommonCompilerFlags% ^
rem 	/link /subsystem:windows,5.02 %CommonLinkerFlags%
rem 64-bit build
cl %project_root%\code\win32-main.cpp %CommonCompilerFlags% ^
	/link %CommonLinkerFlags%
popd
echo Build script finished.