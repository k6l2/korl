@echo off
rem setup necessary environment variables to build the project, and switch our
rem current working directory to be the project's root folder
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
echo navigating to batch file's directory "%~dp0"...
cd %~dp0
cd ..
set project_root=%cd%
echo project_root="%project_root%"
echo adding "%project_root%\misc" to path...
set path=%project_root%\misc;%path%
cd %project_root%