@echo off
setlocal
echo Cleaning all build files...
del /S /F /Q "%project_root%\build" > NUL 2> NUL
rmdir /S /Q "%project_root%\build" > NUL 2> NUL
rmdir /S /Q "%project_root%\build" > NUL 2> NUL
echo Clean complete!
endlocal
exit /B 0
