@rem ----- turn off echo of commands to the prompt -----
@echo off
rem : cmd /k option tells cmd to run the following command and then return to 
rem : the prompt, allowing us to continue using the cmd prompt window
call "%windir%\system32\cmd.exe" /k "%~dp0\korl-set-build-environment.bat"
