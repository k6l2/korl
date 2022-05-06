@echo off
call korl-build-configure-common.bat
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledOptimizationWarnings%
set KORL_DISABLED_WARNINGS=%KORL_DISABLED_WARNINGS% %korlDisabledReleaseWarnings%
set "_CL_=%_CL_% /D KORL_DEBUG#1"
rem     disable optimization
set "_CL_=%_CL_% /Od"
echo KORL build configured in DEBUG mode.
