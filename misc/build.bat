@echo off
rem prerequisites: env.bat
rem --- Save the timestamp before building for timing metric ---
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
   set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
rem --- Clean up build directory ---
if not exist "%project_root%\build" mkdir %project_root%\build
pushd %project_root%\build
del *.pdb > NUL 2> NUL
del *.dll > NUL 2> NUL
set fileNameSafeTimestamp=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
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
rem /Wall - use ALL warnings
rem /WX - treat all warnings as errors
rem /wd4100 - disable warning C4100 `unreferenced formal parameter`
rem /wd4201 - disable warning C4201 
rem           `nonstandard extension used: nameless struct/union`
rem /wd4505 - disable warning C4505 
rem           `unreferenced local function has been removed`
rem /wd4514 - disable warning C4514 
rem           `unreferenced inline function has been removed`
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
rem /LD[d] - create a DLL (or debug, if using the `d` switch).  Implies /MT 
rem          unless already using /MD
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
rem /EXPORT:<name> - exports a function for a DLL
rem /incremental:no - turn off incremental builds! wasting time for no reason.
rem /pdb:<name> - specify a specific name for the PDB file
rem user32.lib - ??? various win32 stuff
rem Gdi32.lib - required to create an OpenGL render context
rem winmm.lib - multimedia timer functions (granular sleep functionality)
set CommonCompilerFlagsDebug= /DINTERNAL_BUILD=1 /DSLOW_BUILD=1 ^
	/MTd /WX /wd4201 /wd4514 /Oi /Od /GR- /EHa- /Zi /FC ^
	/nologo /std:c++latest
set CommonLinkerFlags=/opt:ref /incremental:no 
rem 32-bit build
rem cl %project_root%\code\win32-main.cpp /Fmwin32-main.map ^
rem 	%CommonCompilerFlagsDebug% ^
rem 	/link /subsystem:windows,5.02 %CommonLinkerFlags%
rem 64-bit build
cl %project_root%\code\game.cpp /Fmgame.map ^
	%CommonCompilerFlagsDebug% /Wall /LDd /link %CommonLinkerFlags% ^
	/PDB:game%fileNameSafeTimestamp%.pdb ^
	/EXPORT:gameRenderAudio /EXPORT:gameUpdateAndDraw
IF %ERRORLEVEL% NEQ 0 (
	echo Game build failed!
	GOTO :ON_FAILURE
)
rem Before building the win32 platform application, check to see if it's already
rem running...
if exist win32-main.exe (
	del win32-main.exe >NUL 2>NUL
	IF exist win32-main.exe (
		echo win32-main.exe is locked! Skipping build...
		GOTO :SKIP_WIN32_BUILD
	)
)
cl %project_root%\code\win32-main.cpp /Fmwin32-main.map ^
	%CommonCompilerFlagsDebug% /W4 /wd4100 /wd4505 /link %CommonLinkerFlags% ^
	user32.lib Gdi32.lib winmm.lib opengl32.lib
IF %ERRORLEVEL% NEQ 0 (
	echo win32 build failed!
	GOTO :ON_FAILURE
)
:SKIP_WIN32_BUILD
popd
rem --- Calculate how long it took the build script to run ---
rem Source: https://stackoverflow.com/a/9935540
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
   set /A "end=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
set /A elapsed=end-start
set /A hh=elapsed/(60*60*100), rest=elapsed%%(60*60*100)
set /A mm=rest/(60*100), rest%%=60*100, ss=rest/100, cc=rest%%100
if %mm% lss 10 set mm=0%mm%
if %ss% lss 10 set ss=0%ss%
if %cc% lss 10 set cc=0%cc%
echo Build script finished. Time elapsed: %hh%:%mm%:%ss%.%cc%
exit /B 0
:ON_FAILURE
exit /B %ERRORLEVEL%