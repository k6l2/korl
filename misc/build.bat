@echo off
rem prerequisites: env.bat
mkdir %project_root%\build
pushd %project_root%\build
cl %project_root%\code\win32-handmade.cpp ^
	/Zi /std:c++latest ^
	user32.lib Gdi32.lib
popd