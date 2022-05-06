@echo off
call korl-build-configure-common.bat
set "_CL_=%_CL_% /D KORL_DEBUG#0"
rem     maximize speed
set "_CL_=%_CL_% /O2"
rem     recommended by Microsoft to increase robustness of debug info checksums, 
rem     but it's slower so we shouldn't do this in common build configurations
set "_CL_=%_CL_% /ZH:SHA_256"
echo KORL build configured in RELEASE mode.
