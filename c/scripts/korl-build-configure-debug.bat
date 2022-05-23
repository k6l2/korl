@echo off
call korl-build-configure-common.bat
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledOptimizationWarnings%
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledReleaseWarnings%
set "_CL_=%_CL_% /D KORL_DEBUG#1"
rem     disable optimization
set "_CL_=%_CL_% /Od"
rem     statically link to LIBCMTD.lib; defines _MT & _DEBUG (removes 
rem     requirement to dynamically link to CRT at runtime)
set "_CL_=%_CL_% /MTd"
rem     we always want to create a PDB file for all build types; FULL takes 
rem     longer to complete, but generates a PDB file that includes private 
rem     symbols, meaning that we only require the .exe/.pdb/.dmp files to debug 
rem     crashes
set "KORL_LINKER_OPTIONS=%KORL_LINKER_OPTIONS% /DEBUG:FULL"
echo KORL build configured in DEBUG mode.
