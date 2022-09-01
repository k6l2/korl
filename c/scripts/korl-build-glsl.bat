@echo off
setlocal
if not exist "%KORL_PROJECT_ROOT%\build" (
    mkdir "%KORL_PROJECT_ROOT%\build"
)
if not exist "%KORL_PROJECT_ROOT%\build\shaders" (
    mkdir "%KORL_PROJECT_ROOT%\build\shaders"
)
echo Building shaders in %KORL_HOME%\shaders...
cd %KORL_HOME%\shaders
for /r %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc.exe %%i -o %KORL_PROJECT_ROOT%/build/shaders/%%~nxi.spv
echo:
if not exist "%KORL_PROJECT_ROOT%\shaders" (
    goto :SKIP_KORL_PROJECT_SHADERS
)
echo Building shaders in %KORL_PROJECT_ROOT%\shaders...
cd %KORL_PROJECT_ROOT%\shaders
for /r %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc.exe %%i -o %KORL_PROJECT_ROOT%/build/shaders/%%~nxi.spv
echo:
:SKIP_KORL_PROJECT_SHADERS
endlocal
exit /b 0
