@echo off
rem prerequisites: env.bat
mkdir %project_root%\build
pushd %project_root%\build
cl /Zi %project_root%\code\win32-handmade.cpp user32.lib
popd