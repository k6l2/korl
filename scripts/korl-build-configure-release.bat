@echo off
call korl-build-configure-common.bat
rem     create a DLL (or debug, if using the `d` switch).  Implies /MT 
rem     unless already using /MD
set "korlDllOptions=%korlDllOptions% /LD"
set "_CL_=%_CL_% /D KORL_DEBUG#0"
rem     statically link to LIBCMT.lib; defines _MT (removes 
rem     requirement to dynamically link to CRT at runtime)
set "_CL_=%_CL_% /MT"
rem     recommended by Microsoft to increase robustness of debug info checksums, 
rem     but it's slower so we shouldn't do this in common build configurations
set "_CL_=%_CL_% /ZH:SHA_256"
rem     maximize speed
set "_CL_=%_CL_% /O2"
rem     we always want to create a PDB file for all build types; FULL takes 
rem     longer to complete, but generates a PDB file that includes private 
rem     symbols, meaning that we only require the .exe/.pdb/.dmp files to debug 
rem     crashes
set "KORL_LINKER_OPTIONS=%KORL_LINKER_OPTIONS% /DEBUG:FULL"
echo KORL build configured in RELEASE mode.
