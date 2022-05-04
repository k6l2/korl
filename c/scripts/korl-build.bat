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
rem ----- create the command to build the EXE -----
rem 4061: enumerator 'X' in switch of enum '<X>' is not explicitly handled by a 
rem       case label; WHO CARES?  4062 will prevent us from not having at least 
rem       a default case to handle these enum values, which should catch most of 
rem       these types of bugs, assuming you write decent default case logic...  
rem       I just don't think this warning is useful since it seems more useful 
rem       to have the ability to switch on a subset of enum values, and dump the 
rem       rest into the default case.
rem 4505: unreferenced function with internal linkage has been removed; this is 
rem       completely useless as far as I can tell, since if we try to call a 
rem       function that isn't defined in the module, the compiler will complain
rem 4514: unreferenced inline function has been removed; see 4505
rem 4706: assignment within conditional expression; I will leave this off until 
rem       the day that I ever make a mistake which this would have caught
rem 4710: function not inlined; I just don't care about this, and don't see 
rem       myself caring about this anytime soon
set disableUselessWarnings=/wd4061 /wd4505 /wd4514 /wd4706 /wd4710
rem 4100: unreferenced formal parameter; same reasoning as 4189
rem 4101: unreferenced local variable; same reasoning as 4189 
rem       (why are these different warnings? lol)
rem 4189: local variable is initialized but not referenced; this is useful for 
rem       debugging transient values, but introduces possible bugs so it 
rem       shouldn't be part of release builds
set disableReleaseWarnings=/wd4100 /wd4101 /wd4189
rem 4820: 'x' bytes padding added after data member 'y'; data structures should 
rem       be fully optimized in released builds, but during development this is 
rem       just annoying
rem 5045: Compiler will insert Spectre mitigation for memory load if /Qspectre 
rem       switch specified
set disableOptimizationWarnings=/wd4820 /wd5045
set buildCommand=cl
rem :::::::::::::::::::::::::::: COMPILER SETTINGS :::::::::::::::::::::::::::::
set buildCommand=%buildCommand% "%korl_home%\code\windows\korl-windows-main.c"
set buildCommand=%buildCommand% "%KORL_PROJECT_ROOT%\code\game.cpp"
set buildCommand=%buildCommand% /I "%KORL_PROJECT_ROOT%\code"
rem allow OS-specific code to include global headers/code
set buildCommand=%buildCommand% /I "%korl_home%\code"
rem include additional libraries
set buildCommand=%buildCommand% /I "%korl_home%\code\stb"
rem set the executable's file name
set buildCommand=%buildCommand% /Fe"%KORL_EXE_NAME%"
rem Set the VCX0.PDB file name.  Only relevant when used with /Zi
set buildCommand=%buildCommand% /Fd"VC_%VCToolsVersion%_%KORL_EXE_NAME%.pdb"
rem treat warnings as errors
set buildCommand=%buildCommand% /WX
rem enable all warnings
set buildCommand=%buildCommand% /Wall
rem disable warnings that are not useful for the current configuration
set buildCommand=%buildCommand% %disableUselessWarnings%
set buildCommand=%buildCommand% %disableReleaseWarnings%
set buildCommand=%buildCommand% %disableOptimizationWarnings%
rem So... for some reason this switch completely DESTROYS compile times (nearly 
rem     a 2x increase! wtf?) Consequently, I am keeping this commented out for 
rem     now.
rem KORL-ISSUE-000-000-037: remove later :(
set buildCommand=%buildCommand% /std:c++20
rem disable annoying verbose compiler output
set buildCommand=%buildCommand% /nologo
rem KORL-specific pre-processor definitions
set buildCommand=%buildCommand% /D KORL_DEBUG=1
rem use wide character implementations for Windows API
set buildCommand=%buildCommand% /D UNICODE
set buildCommand=%buildCommand% /D _UNICODE
set buildCommand=%buildCommand% /D KORL_APP_NAME=%KORL_EXE_NAME%
rem generate debug symbols 
rem   Zi => no "edit-and-continue" support + a "VCX.pdb" PDB file
rem   Z7 => no "edit-and-continue" support, no "VCX.pdb" PDB file, not compatible w/ incremental linking, potentially faster?
set buildCommand=%buildCommand% /Z7
rem disable optimization
set buildCommand=%buildCommand% /Od
rem generate compiler intrinsics
set buildCommand=%buildCommand% /Oi
rem disable run-time type information (dynamic_cast, typeid)
set buildCommand=%buildCommand% /GR-
rem disable stack buffer overrun security checks
rem set buildCommand=%buildCommand% /GS-
rem set the stack dynamic allocator probe to be the max value
rem     this allows us to disable dynamic stack allocation features
rem set buildCommand=%buildCommand% /Gs2147483647
rem disable exception handling unwind code generation
rem set buildCommand=%buildCommand% /EHa-
rem KORL-ISSUE-000-000-041: build: remove exception handling code generation from platform (use the above setting instead)
set buildCommand=%buildCommand% /EHsc
rem display the full path of source code files passed in diagnostics
set buildCommand=%buildCommand% /FC
rem KORL-ISSUE-000-000-044: build: for deployments, consider using /ZH:SHA_256 to generate more robust PDB hashes
rem for multi-line errors/warnings, the additional info lines are all placed on 
rem the same line, instead of being broken up into multiple lines
set buildCommand=%buildCommand% /WL
rem ::::::::::::::::::::::::::::: LINKER SETTINGS ::::::::::::::::::::::::::::::
set buildCommand=%buildCommand% /link
rem set the PDB file name
set buildCommand=%buildCommand% /pdb:"%KORL_EXE_NAME%.pdb"
rem generate a map file (manifest of program symbols)
set buildCommand=%buildCommand% /map:"%KORL_EXE_NAME%.map"
rem we don't need to prepare for any subsequent incremental links, so disable 
rem any generation of padding/THUNKS; also disables generation of *.ilk files
set buildCommand=%buildCommand% /INCREMENTAL:NO
rem reserve AND commit 1MB of stack space, preventing dynamic stack allocations
rem set buildCommand=%buildCommand% /stack:0x100000,0x100000
rem do not link to the C runtime (CRT) libraries
rem set buildCommand=%buildCommand% /nodefaultlib
rem we're no longer using the CRT, so we have to define a custom entry point
rem KORL-ISSUE-000-000-036: (low priority) we are actually using the CRT, so we need this
rem set buildCommand=%buildCommand% /entry:korl_windows_main
rem So this property is actually extremely important:  this tells Windows how to 
rem     hook the executable up to the Windows environment, which determines what 
rem     the application's capabilities are.  Generally, the applications KORL 
rem     will be running will be GUI applications, so we specify that here.  This 
rem     changes some behavior, such as not being able to hook standard streams 
rem     into a console automatically, which is fine since that is generally a 
rem     poor/outdated way of getting program input/output anyways.
set buildCommand=%buildCommand% /subsystem:WINDOWS
rem for ExitProcess, GetModuleHandle, etc...
set buildCommand=%buildCommand% kernel32.lib
rem for CommandLineToArgvW
set buildCommand=%buildCommand% Shell32.lib
rem for Windows message APIs, MessageBox, etc...
set buildCommand=%buildCommand% user32.lib
rem for combaseapi.h stuff (CoTaskMemFree etc...)
set buildCommand=%buildCommand% Ole32.lib
rem for C standard lib functions (math functions, etc.), also some STB libs are 
rem pulling in standard libs!
rem KORL-ISSUE-000-000-036: (low priority) configure STB libs to not link to standard libraries
rem set buildCommand=%buildCommand% ucrt.lib
rem for CreateSolidBrush, & other GDI API
set buildCommand=%buildCommand% Gdi32.lib
set buildCommand=%buildCommand% vulkan-1.lib
rem set buildCommand=%buildCommand% legacy_stdio_definitions.lib
rem set buildCommand=%buildCommand% libucrt.lib
rem set buildCommand=%buildCommand% libvcruntime.lib
rem ----- run the build command -----
echo %buildCommand%...
%buildCommand%
IF %ERRORLEVEL% NEQ 0 (
    echo Windows EXE build failed!
    GOTO :ON_FAILURE_EXE
)
rem ----- report how long the script took -----
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
