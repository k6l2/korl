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
rem ----- create the command to build the EXE -----
set buildCommand=cl
rem :::::::::::::::::::::::::::: COMPILER SETTINGS ::::::::::::::::::::::::::::
set buildCommand=%buildCommand% "%KORL_PROJECT_ROOT%\code\korl-windows-main.c"
rem set the executable's file name
set buildCommand=%buildCommand% /Fe"%KORL_EXE_NAME%"
rem set the VCX0.PDB file name
set buildCommand=%buildCommand% /Fd"VC_%KORL_EXE_NAME%"
rem treat warnings as errors
set buildCommand=%buildCommand% /WX
rem enable all warnings
set buildCommand=%buildCommand% /Wall
rem So... for some reason this switch completely DESTROYS compile times (nearly 
rem a 2x increase! wtf?) Consequently, I am keeping this commented out for now.
rem set buildCommand=%buildCommand% /std:c17
rem disable annoying verbose compiler output
set buildCommand=%buildCommand% /nologo
rem use wide character implementations for Windows API
set buildCommand=%buildCommand% /D UNICODE
rem generate a PDB file, no "edit-and-continue" support
set buildCommand=%buildCommand% /Zi
rem disable optimization
set buildCommand=%buildCommand% /Od
rem generate compiler intrinsics
set buildCommand=%buildCommand% /Oi
rem disable run-time type information (dynamic_cast, typeid)
set buildCommand=%buildCommand% /GR-
rem disable stack buffer overrun security checks
rem @todo: consider enabling this & linking to CRT in debug builds only
set buildCommand=%buildCommand% /GS-
rem set the stack dynamic allocator probe to be the max value
rem this allows us to disable dynamic stack allocation features
set buildCommand=%buildCommand% /Gs2147483647
rem disable exception handling unwind code generation
set buildCommand=%buildCommand% /EHa-
rem display the full path of source code files passed in diagnostics
set buildCommand=%buildCommand% /FC
rem ::::::::::::::::::::::::::::: LINKER SETTINGS :::::::::::::::::::::::::::::
set buildCommand=%buildCommand% /link
rem set the PDB file name
set buildCommand=%buildCommand% /pdb:"%KORL_EXE_NAME%.pdb"
rem generate a map file (manifest of program symbols)
set buildCommand=%buildCommand% /map:"%KORL_EXE_NAME%.map"
rem reserve AND commit 1MB of stack space, preventing dynamic stack allocations
set buildCommand=%buildCommand% /stack:0x100000,0x100000
rem do not link to the C runtime (CRT) libraries
set buildCommand=%buildCommand% /nodefaultlib
rem we're no longer using the CRT, so we have to define a custom entry point
set buildCommand=%buildCommand% /entry:korl_windows_main
rem this is required since the compiler can't deduce this option anymore
set buildCommand=%buildCommand% /subsystem:WINDOWS
rem for ExitProcess, GetModuleHandle, etc...
set buildCommand=%buildCommand% kernel32.lib
rem ----- run the build command -----
echo Running "%buildCommand%"...
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
exit /B %ERRORLEVEL%
