@echo off
call korl-build-configure-common.bat
rem we still disable optimization warnings here because this build configuration 
rem is for development, and these types of warnings are still going to get in 
rem our way
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledOptimizationWarnings%
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledReleaseWarnings%
set "_CL_=%_CL_% /D KORL_DEBUG#0"
rem     maximize speed
set "_CL_=%_CL_% /O2"
rem     statically link to LIBCMT.lib; defines _MT (removes 
rem     requirement to dynamically link to CRT at runtime)
set "_CL_=%_CL_% /MT"
rem     we always want to create a PDB file for all build types; FULL takes 
rem     longer to complete, but generates a PDB file that includes private 
rem     symbols, meaning that we only require the .exe/.pdb/.dmp files to debug 
rem     crashes
set "KORL_LINKER_OPTIONS=%KORL_LINKER_OPTIONS% /DEBUG:FULL"
echo KORL build configured in OPTIMIZED mode.
