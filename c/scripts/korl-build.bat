@echo off
setlocal
rem ------- save a timestamp so we can report how long it takes to build -------
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
    set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
rem --- Iterate over build script arguments ------------------------------------
set "buildOptionNoThreads=FALSE"
set "buildOptionVerbose=FALSE"
if "%~1"=="" goto ARGUMENT_LOOP_END
rem set argNumber=0
:ARGUMENT_LOOP_START
rem echo arg%argNumber%=%1
if "%~1"=="nothreads" (
    set "buildOptionNoThreads=TRUE"
    echo Running build on single thread...
    echo:
)
if "%~1"=="verbose" (
    set "buildOptionVerbose=TRUE"
)
shift
rem set /A argNumber+=1
if not "%~1"=="" goto ARGUMENT_LOOP_START
:ARGUMENT_LOOP_END
rem ---------------- create & enter the build output directory  ----------------
cd %KORL_PROJECT_ROOT%
if not exist "build" (
    mkdir "build"
)
cd "build"
rem If we're building a dynamic game application, and the platform application 
rem     binary is locked, then we need to just skip the build & link phases of 
rem     the platform code/executable.
if exist %KORL_EXE_NAME%.exe (
    del %KORL_EXE_NAME%.exe >NUL 2>NUL
    IF exist %KORL_EXE_NAME%.exe (
        echo %KORL_EXE_NAME%.exe is locked; skipping build/link phases...
        echo:
        goto :SET_PLATFORM_CODE_SKIP_TRUE
    )
)
goto :SET_PLATFORM_CODE_SKIP_FALSE
:SET_PLATFORM_CODE_SKIP_TRUE
    set "_KORL_BUILD_SKIP_PLATFORM_CODE=TRUE"
    set "buildOptionNoThreads=FALSE"
    goto :SKIP_SET_PLATFORM_CODE_SKIP
:SET_PLATFORM_CODE_SKIP_FALSE
    set "_KORL_BUILD_SKIP_PLATFORM_CODE=FALSE"
    goto :SKIP_SET_PLATFORM_CODE_SKIP
:SKIP_SET_PLATFORM_CODE_SKIP
rem automatically call the script to build shaders for the first time if the 
rem     directory doesn't exist, since there will always be a non-zero amount of 
rem     compiled shaders, as KORL ships with built-in batching pipelines
set "lockFileBuildShaders=lock-build-shaders"
set "statusFileBuildShaders=status-build-shaders.txt"
set "buildCommand=call korl-build-glsl.bat"
if not exist "shaders" (
    rem // create a lock file //
    type NUL >> "%lockFileBuildShaders%"
    rem // clear status file //
    del %statusFileBuildShaders% > NUL 2> NUL
    if "%buildOptionNoThreads%"=="TRUE" (
        call korl-run-threaded-command.bat "%buildCommand%" %lockFileBuildShaders% %statusFileBuildShaders%
    ) else (
        start "Build Shaders Thread" /b "cmd /c korl-run-threaded-command.bat ^"%buildCommand%^" %lockFileBuildShaders% %statusFileBuildShaders%"
    )
)
rem Print out INCLUDE & LIB variables just for diagnostic purposes:
if not "%buildOptionVerbose%"=="TRUE" (
    goto :SKIP_ECHO_INCLUDE_AND_LIB_VARIABLES
)
echo INCLUDE=%INCLUDE%
echo:
echo LIB=%LIB%
echo:
:SKIP_ECHO_INCLUDE_AND_LIB_VARIABLES
rem Generate a file-name-safe timestamp for the purpose of generating reasonably 
rem unique file names:
set fileNameSafeDate=%date:~-4,4%%date:~-10,2%%date:~-7,2%
set fileNameSafeTimestamp=%fileNameSafeDate%_%time:~0,2%%time:~3,2%%time:~6,2%
rem remove any spaces from the generated timestamp:
rem source: https://stackoverflow.com/a/10116045
set fileNameSafeTimestamp=%fileNameSafeTimestamp: =%
rem --------------------- async build the game object file ---------------------
set "lockFileGame=lock-build-game"
set "statusFileGame=status-build-game.txt"
rem We need to specifically clean up old binaries from dynamic code modules 
rem because when they are loaded into an application module they effectively 
rem become locked on the file system until the debugging application is closed; 
rem so if we don't do this, we'll just accumulate files forever.
set "KORL_GAME_SOURCE_BASE_NAME=game"
del *%KORL_GAME_SOURCE_BASE_NAME%*.pdb > NUL 2> NUL
rem // create the async build command //
set "buildCommand="
set "buildCommand=%buildCommand% "%KORL_PROJECT_ROOT%\code\%KORL_GAME_SOURCE_BASE_NAME%.cpp""
rem     Set the VCX0.PDB file name.  Only relevant when used with /Zi
set "buildCommand=%buildCommand% /std:c++20"
rem     enable all exception handling code generation
set "buildCommand=%buildCommand% /EHa"
set "buildCommand=%buildCommand% %KORL_DISABLED_WARNINGS%"
if not "%buildOptionVerbose%"=="TRUE" (
    goto :SKIP_SHOW_GAME_INCLUDES
)
set "buildCommand=%buildCommand% /showIncludes"
:SKIP_SHOW_GAME_INCLUDES
set "buildCommand=cl.exe %CL% %buildCommand% %_CL_%"
if %KORL_GAME_IS_DYNAMIC% == TRUE ( 
    goto :BUILD_GAME_SET_OPTIONS_DYNAMIC 
)
:BUILD_GAME_SET_OPTIONS_STATIC
    rem     remove all previous dynamic build binaries, so the static 
    rem     applicaiton doesn't get confused
    del *%KORL_GAME_SOURCE_BASE_NAME%*.dll > NUL 2> NUL
    rem     set a normal name for the VC_* pdb file, since we don't have to 
    rem     worry about it being locked by the exe multiple times during runtime 
    rem     like when running dynamic application modules
    set "buildCommand=%buildCommand% /Fd"VC_%VCToolsVersion%_%KORL_GAME_SOURCE_BASE_NAME%.pdb""
    rem     ONLY compile; do not link!
    set "buildCommand=%buildCommand% /c"
goto :END_BUILD_GAME_SET_OPTIONS
:BUILD_GAME_SET_OPTIONS_DYNAMIC
    set "buildCommand=%buildCommand% %korlDllOptions%"
    set "buildCommand=%buildCommand% /Fd"%fileNameSafeTimestamp%_VC_%VCToolsVersion%_%KORL_GAME_SOURCE_BASE_NAME%.pdb""
    set "buildCommand=%buildCommand% /link"
    set "buildCommand=%buildCommand% %KORL_LINKER_OPTIONS%"
    set "buildCommand=%buildCommand% /PDB:%fileNameSafeTimestamp%_%KORL_GAME_SOURCE_BASE_NAME%.pdb"
goto :END_BUILD_GAME_SET_OPTIONS
:END_BUILD_GAME_SET_OPTIONS
rem // create a lock file //
rem crappy attempt to prevent the build script from running while another build 
rem     is still running (batch script isn't too great, not sure how much more 
rem     robust we can get here...)
:WHILE_EXISTS_LOCK_BUILD_GAME
if exist "%lockFileGame%" goto :WHILE_EXISTS_LOCK_BUILD_GAME
type NUL >> "%lockFileGame%"
rem // clear status file //
del %statusFileGame% > NUL 2> NUL
rem // launch the async build on another thread //
if "%buildOptionNoThreads%"=="TRUE" (
    call korl-run-threaded-command.bat "%buildCommand%" %lockFileGame% %statusFileGame%
) else (
    start "Build DLL Thread" /b "cmd /c korl-run-threaded-command.bat ^"%buildCommand%^" %lockFileGame% %statusFileGame%"
)
rem ------------------------ build the KORL object file ------------------------
set "KORL_SOURCE_BASE_NAME=korl-windows-main"
if "%_KORL_BUILD_SKIP_PLATFORM_CODE%"=="TRUE" (
    echo Skipping %KORL_SOURCE_BASE_NAME%.obj build...
    echo:
    goto :SKIP_BUILD_PLATFORM_OBJECT
)
rem     NOTE: if you are looking for the rest of the configured options for 
rem           cl.exe, then please refer to `korl-build-configure-*.bat` scripts
set "buildCommand="
set "buildCommand=%buildCommand% "%KORL_HOME%\code\windows\%KORL_SOURCE_BASE_NAME%.c""
rem     Set the VCX0.PDB file name.  Only relevant when used with /Zi
set "buildCommand=%buildCommand% /Fd"VC_%VCToolsVersion%_%KORL_SOURCE_BASE_NAME%.pdb""
rem     Setting the C standard to C11/C17 causes the compile times to blow up, 
rem     so unless we need any features from modern C standards, I'll just stick 
rem     to the janky MSVC "C98" setting (default).
rem set "buildCommand=%buildCommand% /std:c11"
rem     disable exception handling unwind code generation
set "buildCommand=%buildCommand% /EHa-"
rem     ONLY compile; do not link!
set "buildCommand=%buildCommand% /c"
set "buildCommand=%buildCommand% /D KORL_PLATFORM_WINDOWS"
rem     use wide character implementations for Windows API
set "buildCommand=%buildCommand% /D UNICODE"
set "buildCommand=%buildCommand% /D _UNICODE"
rem     Required defines apparently, if you are compiling with SetupAPI.h, but 
rem     not linking to setupapi.lib in the same call to cl.exe (have not tested this theory yet)
set "buildCommand=%buildCommand% /D USE_SP_ALTPLATFORM_INFO_V1#0"
set "buildCommand=%buildCommand% /D USE_SP_ALTPLATFORM_INFO_V3#1"
set "buildCommand=%buildCommand% /D USE_SP_DRVINFO_DATA_V1#0"
set "buildCommand=%buildCommand% /D USE_SP_BACKUP_QUEUE_PARAMS_V1#0"
set "buildCommand=%buildCommand% /D USE_SP_INF_SIGNER_INFO_V1#0"
rem     We cannot define symbols with the '=' character using the CL environment 
rem         variable, so we have to use '#' instead.
rem         See: https://docs.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170
set "buildCommand=%buildCommand% /D KORL_APP_NAME#%KORL_EXE_NAME%"
set "buildCommand=%buildCommand% /D KORL_APP_VERSION#%KORL_EXE_VERSION%"
set "buildCommand=%buildCommand% /D KORL_APP_TARGET_FRAME_HZ#%KORL_GAME_TARGET_FRAMES_PER_SECOND%"
set "buildCommand=%buildCommand% /D KORL_DLL_NAME#%KORL_GAME_SOURCE_BASE_NAME%"
set "buildCommand=%buildCommand% %KORL_DISABLED_WARNINGS%"
if not "%buildOptionVerbose%"=="TRUE" (
    goto :SKIP_SHOW_KORL_INCLUDES
)
set "buildCommand=%buildCommand% /showIncludes"
:SKIP_SHOW_KORL_INCLUDES
echo cl.exe %CL% %buildCommand% %_CL_%
echo:
rem     NOTE: the CL & _CL_ variables are automatically used by cl.exe
rem           See: https://docs.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170
cl.exe %buildCommand%
echo:
IF %ERRORLEVEL% NEQ 0 (
    echo %KORL_SOURCE_BASE_NAME%.obj failed to compile!
    GOTO :ON_FAILURE_EXE
)
echo %KORL_SOURCE_BASE_NAME% build complete!
echo:
:SKIP_BUILD_PLATFORM_OBJECT
rem ---------------------- synchronize game object build  ----------------------
if %KORL_GAME_IS_DYNAMIC% == TRUE ( 
    goto :SKIP_WAIT_FOR_BUILD_PLATFORM_EXE
)
echo Waiting for game obj build...
echo:
:WAIT_FOR_BUILD_PLATFORM_EXE
if exist %lockFileGame% goto :WAIT_FOR_BUILD_PLATFORM_EXE
if exist %statusFileGame% goto :ON_FAILURE_EXE
:SKIP_WAIT_FOR_BUILD_PLATFORM_EXE
rem ------------------------ link the final executable ------------------------
if "%_KORL_BUILD_SKIP_PLATFORM_CODE%"=="TRUE" (
    echo Skipping %KORL_EXE_NAME%.exe build...
    echo:
    goto :SKIP_BUILD_PLATFORM_EXECUTABLE
)
set "buildCommand=link.exe"
set "buildCommand=%buildCommand% %KORL_SOURCE_BASE_NAME%.obj"
:BUILD_LINK_GAME_MODULE_STATIC
    if %KORL_GAME_IS_DYNAMIC% == TRUE (
        goto :END_BUILD_LINK_GAME_MODULE_STATIC
    )
    set "buildCommand=%buildCommand% %KORL_GAME_SOURCE_BASE_NAME%.obj"
:END_BUILD_LINK_GAME_MODULE_STATIC
set "buildCommand=%buildCommand% %KORL_LINKER_OPTIONS%"
rem     set the name of the executable
set "buildCommand=%buildCommand% /OUT:%KORL_EXE_NAME%.exe"
rem     set the PDB file name
set "buildCommand=%buildCommand% /pdb:"%KORL_EXE_NAME%.pdb""
rem     generate a map file (manifest of program symbols)
@REM set "buildCommand=%buildCommand% /map:"%KORL_EXE_NAME%.map""
rem     So this property is actually extremely important:  this tells Windows 
rem     how to hook the executable up to the Windows environment, which 
rem     determines what the application's capabilities are.  Generally, the 
rem     applications KORL will be running will be GUI applications, so we 
rem     specify that here.  This changes some behavior, such as not being able 
rem     to hook standard streams into a console automatically, which is fine 
rem     since that is generally a poor/outdated way of getting program 
rem     input/output anyways.
set "buildCommand=%buildCommand% /subsystem:WINDOWS"
rem     for ExitProcess, GetModuleHandle, etc...
set "buildCommand=%buildCommand% kernel32.lib"
rem     for CommandLineToArgvW
set "buildCommand=%buildCommand% Shell32.lib"
rem     for Windows message APIs, MessageBox, etc...
set "buildCommand=%buildCommand% user32.lib"
rem     for combaseapi.h stuff (CoTaskMemFree etc...)
set "buildCommand=%buildCommand% Ole32.lib"
rem     for CreateSolidBrush, & other GDI API
set "buildCommand=%buildCommand% Gdi32.lib"
rem     for MiniDumpWriteDump
@REM set "buildCommand=%buildCommand% Dbghelp.lib"
rem     for timeDevGetCaps, timeDevGetTime, etc...
set "buildCommand=%buildCommand% Winmm.lib"
rem     for gamepad module stuff
@REM set "buildCommand=%buildCommand% setupapi.lib"
set "buildCommand=%buildCommand% vulkan-1.lib"
rem     for bluetooth module
set "buildCommand=%buildCommand% ws2_32.lib"
rem     for VirtualAlloc2 & MapViewOfFile3
set "buildCommand=%buildCommand% onecore.lib"
echo %buildCommand%
echo:
%buildCommand%
IF %ERRORLEVEL% EQU 0 (
    GOTO :SKIP_BUILD_PLATFORM_EXECUTABLE
)
echo %KORL_EXE_NAME%.exe failed to link!
GOTO :ON_FAILURE_EXE
:SKIP_BUILD_PLATFORM_EXECUTABLE
rem --------- synchronize the platform EXE build w/ the game DLL build ---------
if NOT %KORL_GAME_IS_DYNAMIC% == TRUE ( 
    goto :SKIP_WAIT_FOR_BUILD_DYNAMIC_GAME_MODULE
)
echo Waiting for game DLL build...
echo:
:WAIT_FOR_BUILD_DYNAMIC_GAME_MODULE
if exist %lockFileGame%   goto :WAIT_FOR_BUILD_DYNAMIC_GAME_MODULE
if exist %statusFileGame% goto :ON_FAILURE_EXE
:SKIP_WAIT_FOR_BUILD_DYNAMIC_GAME_MODULE
rem ------------------------ synchronize shaders build  ------------------------
:WAIT_FOR_BUILD_SHADERS
if exist %lockFileBuildShaders%   goto :WAIT_FOR_BUILD_SHADERS
if exist %statusFileBuildShaders% goto :ON_FAILURE_EXE
rem ----- report how long the script took -----
:TIME_REPORT
set "_TIME_ELAPSED="
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
        set "_TIME_ELAPSED=%ss%.%cc% seconds"
    ) ELSE (
        set "_TIME_ELAPSED=%mm%:%ss%.%cc%"
    )
) ELSE (
    set "_TIME_ELAPSED=%hh%:%mm%:%ss%.%cc%"
)
rem save previous codepage, then set codepage to UTF-8
for /f "tokens=2 delims=:" %%a in ('chcp.com') do set "CONSOLE_CODEPAGE=%%a"
set "CONSOLE_CODEPAGE=%CONSOLE_CODEPAGE: =%"
chcp 65001 >NUL
echo ♫ KORL build succeeded ♫. Time elapsed: %_TIME_ELAPSED%
rem restore the codepage to its original value
chcp %CONSOLE_CODEPAGE% >NUL
rem ----- exit the script -----
endlocal
exit /b 0
rem ----- exit on failures -----
:ON_FAILURE_EXE
echo KORL build FAILED! 1>&2
endlocal
rem We need to exit like this in order to be able to do things in the calling 
rem     command prompt like `korl-build && build\korl-windows.exe` to allow us 
rem     to automatically run the program if the build succeeds.  I tested it and 
rem     unfortunately, this does actually work...
rem     SOURCE: https://www.computerhope.com/forum/index.php?topic=65815.0
cmd /c exit %ERRORLEVEL%
