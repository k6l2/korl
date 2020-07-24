@echo off
rem prerequisites: shell.bat has been run successfully
rem                KML_HOME environment variable
rem                KMD5_HOME environment variable
set codeTreeFileNamePrefixGame=code-tree-game
set codeTreeFileNamePrefixKml=code-tree-kml
rem --- Iterate over build script arguments ---
set buildOptionClean=FALSE
set buildOptionRelease=FALSE
if "%~1"=="" goto ARGUMENT_LOOP_END
rem set argNumber=0
:ARGUMENT_LOOP_START
rem echo arg%argNumber%=%1
if "%~1"=="release" (
	set buildOptionRelease=TRUE
)
if "%~1"=="clean" (
	set buildOptionClean=TRUE
)
shift
rem set /A argNumber+=1
if not "%~1"=="" goto ARGUMENT_LOOP_START
:ARGUMENT_LOOP_END
rem --- CLEAN build command ---
if "%buildOptionClean%"=="TRUE" (
	echo Cleaning all build files...
	del /S /F /Q "%project_root%\build" > NUL 2> NUL
	rmdir /S /Q "%project_root%\build" > NUL 2> NUL
	rmdir /S /Q "%project_root%\build" > NUL 2> NUL
	echo Clean complete!
	exit /B 0
)
rem --- Save the timestamp before building for timing metric ---
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
   set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
rem --- generate asset manifest C++ header file using `kasset` ---
rem     NOTE: this also creates the build directory in the process
pushd %KASSET_HOME%
echo Building kasset.exe...
call build.bat
popd
call %KASSET_HOME%\build\kasset.exe %project_root%\assets %project_root%\build\code
rem --- create the build directory ---
if not exist "%project_root%\build" mkdir "%project_root%\build"
rem --- build kmd5 program so we can use it to check if source files have 
rem     changed, requiring new builds ---
pushd %KMD5_HOME%
echo Building kmd5.exe...
call build.bat
popd
rem --- Create a text tree of the code so we can skip the build if nothing 
rem     changed ---
rem Source: https://www.dostips.com/forum/viewtopic.php?t=6223
for %%a in ("%KCPP_INCLUDE:;=" "%") do (
	call %KMD5_HOME%\build\kmd5.exe %%~a %project_root%\build %codeTreeFileNamePrefixGame%-current.txt --append
)
echo buildOptionRelease==%buildOptionRelease%>>"%project_root%\build\%codeTreeFileNamePrefixGame%-current.txt"
rem --- Create a text tree of KML code to conditionally skip the .exe build ---
call %KMD5_HOME%\build\kmd5.exe %KML_HOME%\code %project_root%\build %codeTreeFileNamePrefixKml%-current.txt
echo buildOptionRelease==%buildOptionRelease%>>"%project_root%\build\%codeTreeFileNamePrefixKml%-current.txt"
pushd %project_root%\build
rem --- Compile the win32 application's resource file in release mode ---
rem ---    The resource file contains the application icon ---
if "%buildOptionRelease%"=="TRUE" (
	rc /fo win32.res %KML_HOME%\default-assets\win32.rc
)
set fileNameSafeDate=%date:~-4,4%%date:~-10,2%%date:~-7,2%
set fileNameSafeTimestamp=%fileNameSafeDate%_%time:~0,2%%time:~3,2%%time:~6,2%
rem remove any spaces from the generated timestamp:
rem source: https://stackoverflow.com/a/10116045
set fileNameSafeTimestamp=%fileNameSafeTimestamp: =%
rem --- DEFINES ---
rem     KML_APP_NAME = A string representing the name of the application,
rem                    which is used in operating-system specific features
rem                    such as determining app temporary data folders, etc.
rem     KML_APP_VERSION = A string representing the application version.  This
rem                       is useful for things such as mini dump analysis, as
rem                       this string should be printed into the minidump's file
rem                       name string.
rem     KML_GAME_DLL_FILENAME = A string representing what the name of the game
rem                             code's DLL should be (EXCLUDING the .dll file
rem                             extension!)
rem     INTERNAL_BUILD = set to 0 to disable all code which should never be 
rem                      shipped to the end-user, such as debug functions etc...
rem     SLOW_BUILD = set to 0 to disable code which typically has the following
rem                  properties:
rem                      -does not affect actual gameplay/program logic
rem                      -most likely will assist with debugging during 
rem                           development
rem                      -incurs a non-zero runtime cost
rem                  Example: debug printing timing info to standard output.
rem /w - disable ALL warnings
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
rem /wd4577 - disable warning C4577 `'noexcept' used with no exception handling 
rem           mode specified; termination on exception is not guaranteed. 
rem           Specify /EHsc`
rem /wd4626 - disable warning C4626 `assignment operator was implicitly defined 
rem           as deleted` because this is emitted every time a lambda is 
rem           declared for some stupid reason?  See:
rem https://developercommunity.visualstudio.com/content/problem/749985/msvc-1922-emitts-c4626-warning-on-simple-lambdas-w.html
rem /wd4710 - disable warning C4710 `function not inlined`
rem /wd4711 - disable warning C4711 `automatic inline selection`
rem /wd4820 - disable warning C4820 `x byte padding after data member...`
rem /wd5045 - disable warning C5045 `... Spectre mitigation for memory load ...`
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
rem /Fe<name> - change name of output code file (don't include the file 
rem             extension)
rem /Fd<name> - change name of VCx0.pdb (where x is the major version of Visual 
rem             C++ in use; VCToolsVersion environment variable probably?)
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
rem user32.lib   - various win32 stuff; probably required forever.
rem Gdi32.lib    - required to create an OpenGL render context
rem winmm.lib    - multimedia timer functions (granular sleep functionality)
rem opengl32.lib - You know what this is for...
rem Dbghelp.lib  - generate mini dumps
rem Shell32.lib  - Obtain AppData path at runtime.
rem dinput8.lib  - DirectInput8 support.
rem dxguid.lib   - DirectInput8 support.
rem ole32.lib    - DirectInput8 support.
rem oleaut32.lib - DirectInput8 support.
rem Hid.lib      - RawInput support (indirectly; support features)
set CommonCompilerFlags=/wd4201 /wd4514 /wd4505 /wd4100 /wd5045 /wd4626 ^
	/Oi /GR- /EHa- /Zi /FC /nologo /std:c++latest
set CommonCompilerFlagsRelease=%CommonCompilerFlags% /O2 /MT /w /wd4711 ^
	/DINTERNAL_BUILD=0 /DSLOW_BUILD=0 
set CommonCompilerFlagsDebug=%CommonCompilerFlags% /MTd /Od /WX ^
	/DINTERNAL_BUILD=1 /DSLOW_BUILD=1 
set CommonCompilerFlagsChosen=%CommonCompilerFlagsDebug%
if "%buildOptionRelease%"=="TRUE" (
	set CommonCompilerFlagsChosen=%CommonCompilerFlagsRelease%
)
set CommonLinkerFlags=/opt:ref /incremental:no 
set Win32LinkerFlags=%CommonLinkerFlags%
if "%buildOptionRelease%"=="TRUE" (
	set Win32LinkerFlags=%CommonLinkerFlags% win32.res
)
rem --- Detect if the KML code tree differs ---
set codeTreeIsDifferentKml=FALSE
fc %codeTreeFileNamePrefixKml%-existing.txt %codeTreeFileNamePrefixKml%-current.txt > NUL 2> NUL
if %ERRORLEVEL% GTR 0 (
	set codeTreeIsDifferentKml=TRUE
)
del %codeTreeFileNamePrefixKml%-existing.txt
ren %codeTreeFileNamePrefixKml%-current.txt %codeTreeFileNamePrefixKml%-existing.txt
rem --- Detect if the game code tree differs ---
set codeTreeIsDifferent=FALSE
fc %codeTreeFileNamePrefixGame%-existing.txt %codeTreeFileNamePrefixGame%-current.txt > NUL 2> NUL
if %ERRORLEVEL% GTR 0 (
	set codeTreeIsDifferent=TRUE
)
del %codeTreeFileNamePrefixGame%-previous.txt
ren %codeTreeFileNamePrefixGame%-existing.txt %codeTreeFileNamePrefixGame%-previous.txt
ren %codeTreeFileNamePrefixGame%-current.txt %codeTreeFileNamePrefixGame%-existing.txt
rem --- We can only skip the game code build if BOTH the game code tree AND KML
rem --- are unchanged! ---
IF "%codeTreeIsDifferent%"=="TRUE" (
	echo %kmlGameDllFileName% code tree has changed!  Continuing build...
) ELSE (
	if "%codeTreeIsDifferentKml%"=="TRUE" (
		echo %kmlGameDllFileName% code tree is unchanged, but KML differs! Continuing build...
	) ELSE (
		echo %kmlGameDllFileName% code tree and KML are unchanged!  Skipping all builds...
		GOTO :SKIP_ALL_BUILDS
	)
)
rem --- Clean up build directory ---
del *%kmlGameDllFileName%*.pdb > NUL 2> NUL
del %kmlGameDllFileName%*.dll > NUL 2> NUL
rem --- Compile game code module ---
cl %project_root%\code\%kmlGameDllFileName%.cpp ^
	/Fe%kmlGameDllFileName% /Fm%kmlGameDllFileName%.map ^
	/FdVC_%kmlGameDllFileName%%fileNameSafeTimestamp%.pdb ^
	/Wall %CommonCompilerFlagsChosen% /wd4710 /wd4577 /wd4820 /LDd ^
	/link %CommonLinkerFlags% ^
	/PDB:%kmlGameDllFileName%%fileNameSafeTimestamp%.pdb ^
	/EXPORT:gameInitialize /EXPORT:gameOnReloadCode ^
	/EXPORT:gameRenderAudio /EXPORT:gameUpdateAndDraw 
IF %ERRORLEVEL% NEQ 0 (
	echo %kmlGameDllFileName% build failed!
	GOTO :ON_FAILURE_GAME
)
:SKIP_GAME_BUILD
rem --- If the KML code tree is unchanged, skip the build ---
IF "%codeTreeIsDifferentKml%"=="TRUE" (
	echo KML Code tree has changed!  Continuing build...
) ELSE (
	echo KML Code tree is unchanged!  Skipping build...
	GOTO :SKIP_WIN32_BUILD
)
rem Before building the win32 platform application, check to see if it's already
rem running...
if exist %kmlApplicationName%.exe (
	del %kmlApplicationName%.exe >NUL 2>NUL
	IF exist %kmlApplicationName%.exe (
		echo %kmlApplicationName%.exe is locked! Skipping build...
		del %codeTreeFileNamePrefixKml%-existing.txt
		GOTO :SKIP_WIN32_BUILD
	)
)
rem --- Clean up build directory ---
del *%kmlApplicationName%*.pdb > NUL 2> NUL
del %kmlApplicationName%*.pdb > NUL 2> NUL
del %kmlApplicationName%*.dll > NUL 2> NUL
rem --- Compile Windows Executable ---
cl %KML_HOME%\code\win32-main.cpp /Fe%kmlApplicationName% ^
	/FdVC_%kmlApplicationName%.pdb /Fm%kmlApplicationName%.map ^
	/DKML_APP_NAME=%kmlApplicationName% ^
	/DKML_APP_VERSION=%kmlApplicationVersion% ^
	/DKML_GAME_DLL_FILENAME=%kmlGameDllFileName% ^
	/DKML_MINIMUM_FRAME_RATE=%kmlMinimumFrameRate% ^
	/W4 %CommonCompilerFlagsChosen% /link %Win32LinkerFlags% ^
	user32.lib Gdi32.lib winmm.lib opengl32.lib Dbghelp.lib Shell32.lib ^
	dinput8.lib dxguid.lib ole32.lib oleaut32.lib Hid.lib
IF %ERRORLEVEL% NEQ 0 (
	echo win32 build failed!
	GOTO :ON_FAILURE_KML
)
:SKIP_WIN32_BUILD
:SKIP_ALL_BUILDS
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
IF %hh% LEQ 0 (
	IF %mm% LEQ 0 (
		echo Build script finished. Time elapsed: %ss%.%cc% seconds.
	) ELSE (
		echo Build script finished. Time elapsed: %mm%:%ss%.%cc%
	)
) ELSE (
	echo Build script finished. Time elapsed: %hh%:%mm%:%ss%.%cc%
)
exit /B 0
:ON_FAILURE_GAME
del %codeTreeFileNamePrefixGame%-existing.txt
:ON_FAILURE_KML
del %codeTreeFileNamePrefixKml%-existing.txt
popd
exit /B %ERRORLEVEL%