@echo off
setlocal

rem set the parent directory to be the root of the KORL installation
cd %~dp0
cd ..
echo Setting environment variable 'korl_root=%cd%'...
setx korl_root "%cd%"

pause
endlocal
exit /b 0