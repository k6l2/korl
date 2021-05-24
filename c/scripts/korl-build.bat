@echo off
setlocal
rem ----- setup build variables -----
set buildExeName=korl-windows
rem ----- create the build output directory -----
cd %KORL_PROJECT_ROOT%
if not exist "build" (
	mkdir "build"
)
cd "build"
rem ----- create the command to build the EXE -----
set buildCommand=cl
set buildCommand=%buildCommand% "%KORL_PROJECT_ROOT%\code\korl-windows-main.c"
rem set the executable's file name
set buildCommand=%buildCommand% /Fe"%buildExeName%"
rem set the PDB file name
set buildCommand=%buildCommand% /Fd"%buildExeName%"
rem generate a PDB file, no "edit-and-continue" support
set buildCommand=%buildCommand% /Zi
rem disable annoying verbose compiler output
set buildCommand=%buildCommand% /nologo
rem use wide character implementations for Windows API
set buildCommand=%buildCommand% /D UNICODE
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
set buildCommand=%buildCommand% /link
rem reserve AND commit 1MB of stack space, preventing dynamic stack allocations
set buildCommand=%buildCommand% /stack:0x100000,0x100000
rem do not link to the C runtime libraries
set buildCommand=%buildCommand% /nodefaultlib
rem generate a map file (manifest of program symbols)
set buildCommand=%buildCommand% /map:%buildExeName%.map
rem we're no longer using the CRT, so we have to define a custom entry point
set buildCommand=%buildCommand% /entry:korl_windows_main
rem this is required since the compiler can't deduce this option anymore
set buildCommand=%buildCommand% /subsystem:WINDOWS
rem for ExitProcess, GetModuleHandle, etc...
set buildCommand=%buildCommand% kernel32.lib
rem ----- run the build command -----
echo Running "%buildCommand%"...
%buildCommand%
rem ----- exit the script -----
endlocal
exit /b 0
