@echo off
rem change current director to be the parent of this script's directory
cd %~dp0\..
rem cmd /k option tells cmd to run the following command and then return to 
rem the prompt, allowing us to continue using the cmd prompt window
call "%windir%\system32\cmd.exe" /k "%~dp0\korl-set-build-environment.bat"
