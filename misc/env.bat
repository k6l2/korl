@echo off
rem prerequisites: VISUAL_C_BUILD_SCRIPTS_HOME environment variable set
rem                KML_HOME environment variable set
set oneFolderBelowProjectRoot=%1
rem setup necessary environment variables to build the project, and switch our
rem current working directory to be the project's root folder
call "%VISUAL_C_BUILD_SCRIPTS_HOME%\vcvarsall.bat" x64
echo navigating to batch file's directory "%oneFolderBelowProjectRoot%"...
cd %oneFolderBelowProjectRoot%
cd ..
set project_root=%cd%
echo project_root="%project_root%"
echo adding "%oneFolderBelowProjectRoot%" and "%KML_HOME%\misc" to path...
set path=%KML_HOME%\misc;%oneFolderBelowProjectRoot%;%path%
echo adding "%KML_HOME%\code" to INCLUDE environment variable...
set INCLUDE=%KML_HOME%\code;%INCLUDE%
echo adding "%project_root%\code" to INCLUDE environment variable...
set INCLUDE=%project_root%\code;%INCLUDE%
cd %project_root%