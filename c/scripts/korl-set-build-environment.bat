@echo off
rem ----- setup MSVC build tools -----
call "%VsDevCmd_Path%" -arch=amd64
rem ----- set the project root directory -----
set KORL_PROJECT_ROOT=%cd%
echo KORL_PROJECT_ROOT=%KORL_PROJECT_ROOT%
rem ----- add KORL scripts to path for easy execution -----
echo Adding "%KORL_PROJECT_ROOT%\scripts" to PATH...
set PATH=%PATH%;%KORL_PROJECT_ROOT%\scripts
rem ----- set build global variables -----
set KORL_EXE_NAME=korl-windows
echo KORL_EXE_NAME=%KORL_EXE_NAME%
rem ----- simple notification to the user that we're done ---
echo KORL Build Environment Ready!
