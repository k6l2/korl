@echo off
setlocal
if not exist "%KORL_PROJECT_ROOT%\build" (
    mkdir "%KORL_PROJECT_ROOT%\build"
)
if not exist "%KORL_PROJECT_ROOT%\build\shaders" (
    mkdir "%KORL_PROJECT_ROOT%\build\shaders"
)
for /r %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc.exe %%i -o %KORL_PROJECT_ROOT%/build/shaders/%%~nxi.spv
endlocal
exit /b 0