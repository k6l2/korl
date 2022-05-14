@echo off
setlocal
rem ----- save a timestamp so we can report how long it takes to build -----
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
    set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
rem ----- create & enter the build output directory -----
cd %KORL_PROJECT_ROOT%
if not exist "build" (
    mkdir "build"
)
cd "build"
rem automatically call the script to build shaders for the first time if the 
rem     directory doesn't exist, since there will always be a non-zero amount of 
rem     compiled shaders, as KORL ships with built-in batching pipelines
if not exist "shaders" (
    call korl-build-glsl.bat
)
rem ------------------------ build the KORL object file ------------------------
rem     NOTE: if you are looking for the rest of the configured options for 
rem           cl.exe, then please refer to `korl-build-configure-*.bat` scripts
set "KORL_SOURCE_BASE_NAME=korl-windows-main"
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
rem     use wide character implementations for Windows API
set "buildCommand=%buildCommand% /D UNICODE"
set "buildCommand=%buildCommand% /D _UNICODE"
rem     We cannot define symbols with the '=' character using the CL environment 
rem         variable, so we have to use '#' instead.
rem         See: https://docs.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170
set "buildCommand=%buildCommand% /D KORL_APP_NAME#%KORL_EXE_NAME%"
set "buildCommand=%buildCommand% /D KORL_APP_VERSION#%KORL_EXE_VERSION%"
set "buildCommand=%buildCommand% %KORL_DISABLED_WARNINGS%"
echo cl.exe %CL% %buildCommand% %_CL_%
echo:
rem     NOTE: the CL & _CL_ variables are automatically used by cl.exe
rem           See: https://docs.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170
rem KORL-PERFORMANCE-000-000-019: build: investigate potential benefits of multi-threading cl.exe
cl.exe %buildCommand%
echo:
IF %ERRORLEVEL% NEQ 0 (
    echo %KORL_SOURCE_BASE_NAME%.obj failed to compile!
    GOTO :ON_FAILURE_EXE
)
rem ------------------------ build the game object file ------------------------
set "KORL_GAME_SOURCE_BASE_NAME=game"
set "buildCommand="
set "buildCommand=%buildCommand% "%KORL_PROJECT_ROOT%\code\%KORL_GAME_SOURCE_BASE_NAME%.cpp""
rem     Set the VCX0.PDB file name.  Only relevant when used with /Zi
set "buildCommand=%buildCommand% /Fd"VC_%VCToolsVersion%_%KORL_GAME_SOURCE_BASE_NAME%.pdb""
set "buildCommand=%buildCommand% /std:c++20"
set "buildCommand=%buildCommand% %KORL_DISABLED_WARNINGS%"
echo cl.exe %CL% %buildCommand% %_CL_%
echo:
rem     NOTE: the CL & _CL_ variables are automatically used by cl.exe
rem           See: https://docs.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170
rem KORL-PERFORMANCE-000-000-019: build: investigate potential benefits of multi-threading cl.exe
cl.exe %buildCommand%
echo:
IF %ERRORLEVEL% NEQ 0 (
    echo %KORL_GAME_SOURCE_BASE_NAME%.obj failed to compile!
    GOTO :ON_FAILURE_EXE
)
rem ------------------------ link the final executable ------------------------
set "buildCommand=link.exe"
set "buildCommand=%buildCommand% %KORL_SOURCE_BASE_NAME%.obj"
set "buildCommand=%buildCommand% %KORL_GAME_SOURCE_BASE_NAME%.obj"
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
set "buildCommand=%buildCommand% Dbghelp.lib"
set "buildCommand=%buildCommand% vulkan-1.lib"
echo %buildCommand%
echo:
%buildCommand%
IF %ERRORLEVEL% NEQ 0 (
    echo %KORL_EXE_NAME%.exe failed to link!
    GOTO :ON_FAILURE_EXE
)
rem ----- report how long the script took -----
:TIME_REPORT
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
rem ----- exit the script -----
endlocal
exit /b 0
rem ----- exit on failures -----
:ON_FAILURE_EXE
echo KORL build failed! 1>&2
endlocal
rem We need to exit like this in order to be able to do things in the calling 
rem     command prompt like `korl-build && build\korl-windows.exe` to allow us 
rem     to automatically run the program if the build succeeds.  I tested it and 
rem     unfortunately, this does actually work...
rem     SOURCE: https://www.computerhope.com/forum/index.php?topic=65815.0
cmd /c exit %ERRORLEVEL%
