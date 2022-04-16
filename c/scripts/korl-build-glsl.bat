@echo off
setlocal
if not exist "%KORL_PROJECT_ROOT%\build" (
    mkdir "%KORL_PROJECT_ROOT%\build"
)
if not exist "%KORL_PROJECT_ROOT%\build\shaders" (
    mkdir "%KORL_PROJECT_ROOT%\build\shaders"
)
echo "Building shaders in %korl_home%\shaders..."
cd %korl_home%\shaders
for /r %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc.exe %%i -o %KORL_PROJECT_ROOT%/build/shaders/%%~nxi.spv
echo "Building shaders in %KORL_PROJECT_ROOT%\shaders..."
cd %KORL_PROJECT_ROOT%\shaders
for /r %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc.exe %%i -o %KORL_PROJECT_ROOT%/build/shaders/%%~nxi.spv
endlocal
exit /b 0