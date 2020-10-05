@echo off
setlocal
set cmdBuild=%~1
set lockFileName=%~2
set statusFileName=%~3
echo cmdBuild='%cmdBuild%'
%cmdBuild%
if %ERRORLEVEL% NEQ 0 (
	type NUL >> "%statusFileName%"
)
del %lockFileName%
endlocal
exit /b 0
