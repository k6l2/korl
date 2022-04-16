@echo off
set oneFolderBelowProjectRoot=%1
rem ----- setup MSVC build tools -----
set vswhere_location=%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere
for /f "usebackq delims=#" %%a in (`"%vswhere_location%" -latest -property installationPath`) do (
	echo Setting environment variable 'VsDevCmd_Path=%%a\Common7\Tools\VsDevCmd.bat'
	set "VsDevCmd_Path=%%a\Common7\Tools\VsDevCmd.bat"
)
call "%VsDevCmd_Path%" -arch=amd64
rem ----- set the project root directory -----
echo navigating to batch file's directory "%oneFolderBelowProjectRoot%"...
cd %oneFolderBelowProjectRoot%
cd ..
set KORL_PROJECT_ROOT=%cd%
echo KORL_PROJECT_ROOT=%KORL_PROJECT_ROOT%
rem ----- add KORL scripts to path for easy execution -----
echo adding "%oneFolderBelowProjectRoot%" and "%korl_home%\scripts" to path...
set path=%korl_home%\scripts;%oneFolderBelowProjectRoot%;%path%
rem echo Adding "%korl_root%\scripts" to PATH...
rem set PATH=%PATH%;%korl_root%\scripts
rem ----- set build global variables -----
rem set KORL_EXE_NAME=korl-windows
rem echo KORL_EXE_NAME=%KORL_EXE_NAME%
rem ----- add libraries to build environment -----
rem we do this here so that editors like VSCode can easily find library headers
set INCLUDE=%INCLUDE%;%VULKAN_SDK%\Include
set LIB=%LIB%;%VULKAN_SDK%\Lib
rem ----- simple notification to the user that we're done ---
echo KORL Build Environment Ready!
