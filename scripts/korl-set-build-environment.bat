@echo off

rem ----- setup MSVC build tools -----
set vswhere_location=%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere
for /f "usebackq delims=#" %%a in (`"%vswhere_location%" -latest -property installationPath`) do (
    set "VsDevCmd_Path=%%a\Common7\Tools\VsDevCmd.bat"
)
echo "%VsDevCmd_Path%" -no_logo -arch=amd64 ...
call "%VsDevCmd_Path%" -no_logo -arch=amd64

rem ----- save VS environment variable defaults for later reconfiguration -----
set "KORL_VS_ORIGINAL_INCLUDE=%INCLUDE%"
set "KORL_VS_ORIGINAL_LIB=%LIB%"
set "KORL_VS_ORIGINAL_LIBPATH=%LIBPATH%"

rem ----- move to the project's root directory -----
rem       KORL build scripts will assume to start within this directory
echo navigating to KORL_PROJECT_ROOT directory "%KORL_PROJECT_ROOT%"...
cd %KORL_PROJECT_ROOT%

rem ----- add KORL scripts to path for easy execution -----
echo adding "%KORL_PROJECT_SCRIPTS%" and "%KORL_HOME%\scripts" to path...
set path=%KORL_HOME%\scripts;%KORL_PROJECT_SCRIPTS%;%path%

rem ----- initialize environment in DEBUG mode, since it will be most common -----
call korl-build-configure-debug.bat

echo KORL Build Environment Ready!
