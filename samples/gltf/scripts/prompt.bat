@echo off
set  KORL_EXE_NAME=korl-sample-gltf
echo KORL_EXE_NAME=%KORL_EXE_NAME%
set  KORL_EXE_VERSION=0
echo KORL_EXE_VERSION=%KORL_EXE_VERSION%
set  KORL_GAME_TARGET_FRAMES_PER_SECOND=120
echo KORL_GAME_TARGET_FRAMES_PER_SECOND=%KORL_GAME_TARGET_FRAMES_PER_SECOND%
rem Maybe set this configuration in a more general "config" script at some point as an argument?
set  KORL_GAME_IS_DYNAMIC=TRUE
echo KORL_GAME_IS_DYNAMIC=%KORL_GAME_IS_DYNAMIC%
set  KORL_PROJECT_ROOT=%~dp0..\
echo KORL_PROJECT_ROOT=%KORL_PROJECT_ROOT%
set  KORL_PROJECT_SCRIPTS=%~dp0
echo KORL_PROJECT_SCRIPTS=%KORL_PROJECT_SCRIPTS%
cd %KORL_PROJECT_ROOT%\..\..\
set "KORL_HOME=%cd%"
echo KORL_HOME=%KORL_HOME%
rem cmd /k option tells cmd to run the following command and then return to 
rem the prompt, allowing us to continue using the cmd prompt window
call "%windir%\system32\cmd.exe" /k "%KORL_HOME%\scripts\korl-set-build-environment.bat"

