@echo off
rem prerequisites: shell.bat has been run successfully
rem                KML_HOME environment variable
rem                KMD5_HOME environment variable
setlocal





rem --- Save the timestamp before building for timing metric ----------------{{{
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
   set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
rem }}}----- save build start timestamp





rem --- Iterate over build script arguments ---------------------------------{{{
set buildOptionRelease=FALSE
set buildOptionDisableKmd5=FALSE
set buildOptionNoThreads=FALSE
if "%~1"=="" goto ARGUMENT_LOOP_END
rem set argNumber=0
:ARGUMENT_LOOP_START
rem echo arg%argNumber%=%1
if "%~1"=="release" (
	set buildOptionRelease=TRUE
)
if "%~1"=="nohash" (
	set buildOptionDisableKmd5=TRUE
)
if "%~1"=="nothreads" (
	set buildOptionNoThreads=TRUE
)
shift
rem set /A argNumber+=1
if not "%~1"=="" goto ARGUMENT_LOOP_START
:ARGUMENT_LOOP_END
rem }}}----- build script arguments





rem --- generate asset manifest C++ header file using `kasset` --------------{{{
rem     NOTE: this also creates the build directory in the process
pushd %KASSET_HOME%
echo Building kasset.exe...
call build.bat
popd
call %KASSET_HOME%\build\kasset.exe %project_root%\assets ^
	%project_root%\build\code
rem }}}----- run KAsset





rem --- create the build directory ------------------------------------------{{{
if not exist "%project_root%\build" mkdir "%project_root%\build"
rem }}}----- create build directory





rem --- build kmd5 program --------------------------------------------------{{{
rem we can use it to check if source files have changed, requiring new builds
if "%buildOptionDisableKmd5%"=="FALSE" (
	pushd %KMD5_HOME%
	echo Building kmd5.exe...
	call build.bat
	popd
)
rem }}}----- build kmd5 program





rem --- hash the DLL source to see if it requires a rebuild -----------------{{{
set hashFilePrefixDll=source-hash-dll
set hashFileCurrentDll=%hashFilePrefixDll%-current.txt
if "%buildOptionDisableKmd5%"=="FALSE" (
	for %%a in ("%KCPP_INCLUDE:;=" "%") do (
		call %KMD5_HOME%\build\kmd5.exe "%%~a" "%project_root%\build" ^
			"%hashFileCurrentDll%" --append
	)
	echo buildOptionRelease==%buildOptionRelease%>>^
"%project_root%\build\%hashFileCurrentDll%"
)
rem }}}----- hash the DLL source





rem --- hash the EXE source to see if it requires a rebuild -----------------{{{
set hashFilePrefixExe=source-hash-exe
set hashFileCurrentExe=%hashFilePrefixExe%-current.txt
if "%buildOptionDisableKmd5%"=="FALSE" (
	call %KMD5_HOME%\build\kmd5.exe "%KML_HOME%\code" "%project_root%\build" ^
		"%hashFileCurrentExe%"
	rem append the build files (including this one) to the EXE build 
	rem dependency hash
	call %KMD5_HOME%\build\kmd5.exe "%KML_HOME%\misc" "%project_root%\build" ^
		"%hashFileCurrentExe%" --append
	echo buildOptionRelease==%buildOptionRelease%>>^
"%project_root%\build\%hashFileCurrentExe%"
)
rem }}}----- hash the EXE source




rem --- ENTER THE BUILD DIRECTORY HERE ---
pushd %project_root%\build





rem --- Detect if the EXE code differs ---
set hashFileExistingExe=%hashFilePrefixExe%-existing.txt
set codeIsDifferentExe=TRUE
if "%buildOptionDisableKmd5%"=="FALSE" (
call :checkHash %hashFileExistingExe%, %hashFileCurrentExe%, codeIsDifferentExe
)





rem --- Detect if the DLL code differs ---
set hashFileExistingDll=%hashFilePrefixDll%-existing.txt
set codeIsDifferentDll=TRUE
if "%buildOptionDisableKmd5%"=="FALSE" (
call :checkHash %hashFileExistingDll%, %hashFileCurrentDll%, codeIsDifferentDll
)





rem --- Compile the win32 application's resource file in release mode -------{{{
rem ---    The resource file contains the application icon ---
if "%buildOptionRelease%"=="TRUE" (
	rc /fo win32.res %KML_HOME%\default-assets\win32.rc
)
rem }}}----- compile win32 app resource file





rem --- We can only skip the game code build if BOTH the game code tree AND KML
rem --- are unchanged! ---
IF "%codeIsDifferentDll%"=="TRUE" (
	echo %kmlGameDllFileName% code has changed!  Continuing build...
) ELSE (
	if "%codeIsDifferentExe%"=="TRUE" (
		echo %kmlGameDllFileName% code is unchanged, but EXE differs! Continuing build...
	) ELSE (
		echo %kmlGameDllFileName% code and EXE are unchanged!  Skipping all builds...
		GOTO :SKIP_ALL_BUILDS
	)
)





rem --- generate a filename-safe timestamp ----------------------------------{{{
set fileNameSafeDate=%date:~-4,4%%date:~-10,2%%date:~-7,2%
set fileNameSafeTimestamp=%fileNameSafeDate%_%time:~0,2%%time:~3,2%%time:~6,2%
rem remove any spaces from the generated timestamp:
rem source: https://stackoverflow.com/a/10116045
set fileNameSafeTimestamp=%fileNameSafeTimestamp: =%
rem }}}----- generate a filename-safe timestamp




rem --- Choose the compiler options -----------------------------------------{{{
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
rem /wd4189 - disable warning C4189 `local variable is initialized but not 
rem           referenced`
rem /wd4200 - disable warning C4200: nonstandard extension used: zero-sized 
rem           array in struct/union
rem /wd4201 - disable warning C4201 
rem           `nonstandard extension used: nameless struct/union`
rem /wd4505 - disable warning C4505 
rem           `unreferenced local function has been removed`
rem /wd4514 - disable warning C4514 
rem           `unreferenced inline function has been removed`
rem /wd4577 - disable warning C4577 `'noexcept' used with no exception handling 
rem           mode specified; termination on exception is not guaranteed. 
rem           Specify /EHsc`
rem /wd4623 - disable warning C4623 `default constructor was implicitly defined 
rem           as deleted`
rem /wd4626 - disable warning C4626 `assignment operator was implicitly defined 
rem           as deleted` because this is emitted every time a lambda is 
rem           declared for some stupid reason?  See:
rem https://developercommunity.visualstudio.com/content/problem/749985/msvc-1922-emitts-c4626-warning-on-simple-lambdas-w.html
rem /wd4710 - disable warning C4710 `function not inlined`
rem /wd4711 - disable warning C4711 `automatic inline selection`
rem /wd4820 - disable warning C4820 `x byte padding after data member...`
rem /wd5027 - disable warning C5027 `move assignment operator was implicitly 
rem           defined as deleted`
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
rem ws2_32.lib   - winsock (networking support)
set CommonCompilerFlags=/wd4201 /wd4514 /wd4505 /wd4100 /wd5045 /wd4626 ^
	/wd4200 /wd4623 /wd5027 /Oi /GR- /EHa- /Zi /FC /nologo /std:c++latest
set CommonCompilerFlagsRelease=%CommonCompilerFlags% /O2 /MT /w /wd4711 ^
	/DINTERNAL_BUILD=0 /DSLOW_BUILD=0 
set CommonCompilerFlagsDebug=%CommonCompilerFlags% /MTd /Od /WX /wd4189 ^
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
rem }}}----- choose compiler options






rem --- Clean up old DLL binaries from the build directory ---
set statusFileDll=status-build-dll.txt
del %statusFileDll% > NUL 2> NUL
del *%kmlGameDllFileName%*.pdb > NUL 2> NUL
del %kmlGameDllFileName%*.dll > NUL 2> NUL




rem --- create a lock file so we can wait for the build to finish on another 
rem     thread ---
rem Source: https://stackoverflow.com/a/211045/4526664
set lockFileDll=lock-build-dll
type NUL >> "%lockFileDll%"




rem --- generate the DLL build command string -----
set              cmdBuildDll=cl %project_root%\code\%kmlGameDllFileName%.cpp 
set cmdBuildDll=%cmdBuildDll%/Fe%kmlGameDllFileName% 
set cmdBuildDll=%cmdBuildDll%/Fm%kmlGameDllFileName%.map 
set cmdBuildDll=%cmdBuildDll%^
/FdVC_%kmlGameDllFileName%%fileNameSafeTimestamp%.pdb 
set cmdBuildDll=%cmdBuildDll%/Wall %CommonCompilerFlagsChosen% /wd4710 /wd4577 ^
/wd4820 /LDd 
set cmdBuildDll=%cmdBuildDll%/link %CommonLinkerFlags% 
set cmdBuildDll=%cmdBuildDll%^
/PDB:%kmlGameDllFileName%%fileNameSafeTimestamp%.pdb 
set cmdBuildDll=%cmdBuildDll%/EXPORT:gameInitialize /EXPORT:gameOnReloadCode 
set cmdBuildDll=%cmdBuildDll%/EXPORT:gameRenderAudio /EXPORT:gameUpdateAndDraw ^
/EXPORT:gameOnPreUnload




rem --- launch the DLL build in a separate thread -----
if "%buildOptionNoThreads%"=="TRUE" (
	call build-atom.bat "%cmdBuildDll%" %lockFileDll% %statusFileDll%
) else (
	start "Build DLL Thread" /b ^
		"cmd /c build-atom.bat ^"%cmdBuildDll%^" %lockFileDll% %statusFileDll%"
)










rem --- Compile game code module ---
rem cl %project_root%\code\%kmlGameDllFileName%.cpp ^
rem 	/Fe%kmlGameDllFileName% /Fm%kmlGameDllFileName%.map ^
rem 	/FdVC_%kmlGameDllFileName%%fileNameSafeTimestamp%.pdb ^
rem 	/Wall %CommonCompilerFlagsChosen% /wd4710 /wd4577 /wd4820 /LDd ^
rem 	/link %CommonLinkerFlags% ^
rem 	/PDB:%kmlGameDllFileName%%fileNameSafeTimestamp%.pdb ^
rem 	/EXPORT:gameInitialize /EXPORT:gameOnReloadCode ^
rem 	/EXPORT:gameRenderAudio /EXPORT:gameUpdateAndDraw /EXPORT:gameOnPreUnload
rem IF %ERRORLEVEL% NEQ 0 (
rem 	echo %kmlGameDllFileName% build failed!
rem 	GOTO :ON_FAILURE_GAME
rem )
rem :SKIP_GAME_BUILD





rem --- If the KML code tree is unchanged, skip the build ---
IF "%codeIsDifferentExe%"=="TRUE" (
	echo EXE Code tree has changed!  Continuing build...
) ELSE (
	echo EXE Code tree is unchanged!  Skipping build...
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





rem --- Clean up old EXE binaries from the build directory ---
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
	dinput8.lib dxguid.lib ole32.lib oleaut32.lib Hid.lib ws2_32.lib 
IF %ERRORLEVEL% NEQ 0 (
	echo win32 build failed!
	GOTO :ON_FAILURE_EXE
)




rem --- wait for the DLL build thread to complete -----
rem @TODO: automatically terminate the DLL build thread if it hangs for like 10
rem        seconds or something
:SKIP_WIN32_BUILD
:WAIT_FOR_DLL_BUILD
if exist %lockFileDll% goto WAIT_FOR_DLL_BUILD
:DLL_BUILD_COMPLETE
if exist %statusFileDll% goto ON_FAILURE_DLL
:SKIP_ALL_BUILDS
rem ----- LEAVE THE BUILD DIRECTORY -----
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




rem ----- SUCCESSFULLY END THE BUILD -----
endlocal
exit /B 0




rem ----- FAILURE END THE BUILD -----
:ON_FAILURE_DLL
del %hashFileExistingDll%
:ON_FAILURE_EXE
del %hashFileExistingExe%
rem ----- LEAVE THE BUILD DIRECTORY -----
popd
echo KML build failed! 1>&2
endlocal
exit /B %ERRORLEVEL%





rem @param %~1 hash file name (existing)
rem @param %~2 hash file name (current)
rem @param %~3 return status (TRUE || FALSE)
:checkHash
	setlocal
	fc %~1 %~2 > NUL 2> NUL
	if %ERRORLEVEL% GTR 0 (
		set "result=TRUE"
	) else (
		set "result=FALSE"
	)
	del %~1 2> NUL
	ren %~2 %~1
	endlocal&set "%~3=%result%"
	exit /b 0
