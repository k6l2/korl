@echo off
call korl-build-configure-common.bat
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledOptimizationWarnings%
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledReleaseWarnings%
set "_CL_=%_CL_% /D KORL_DEBUG#1"
rem     disable optimization
set "_CL_=%_CL_% /Od"
rem     we always want to create a PDB file for all build types; FASTLINK allows 
rem     faster linking, but generates a limited PDB file that doesn't include 
rem     private symbol files, requiring object files to debug which should be 
rem     _okay_ in a development environment since we have these readily available
set "KORL_LINKER_OPTIONS=%KORL_LINKER_OPTIONS% /DEBUG:FASTLINK"
echo KORL build configured in DEBUG mode.
