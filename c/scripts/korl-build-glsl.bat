@echo off
for /r %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc.exe %%i -o %KORL_PROJECT_ROOT%/build/%%~nxi.spv