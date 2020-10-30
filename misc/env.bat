@echo off
rem prerequisites: install.bat has been run
set oneFolderBelowProjectRoot=%1
rem setup necessary environment variables to build the project, and switch our
rem current working directory to be the project's root folder
call "%VsDevCmd_Path%" -arch=amd64
echo navigating to batch file's directory "%oneFolderBelowProjectRoot%"...
cd %oneFolderBelowProjectRoot%
cd ..
set project_root=%cd%
echo project_root="%project_root%"
echo adding "%oneFolderBelowProjectRoot%" and "%KML_HOME%\misc" to path...
set path=%KML_HOME%\misc;%oneFolderBelowProjectRoot%;%path%
echo adding "%KML_HOME%\code" to INCLUDE environment variable...
set INCLUDE=%KML_HOME%\code;%INCLUDE%
echo adding "%KML_HOME%\code-utilities" to INCLUDE environment variable...
set INCLUDE=%KML_HOME%\code-utilities;%INCLUDE%
echo adding "%project_root%\code" to INCLUDE environment variable...
set INCLUDE=%project_root%\code;%INCLUDE%
echo adding "%project_root%\build\code" to INCLUDE environment variable...
set INCLUDE=%project_root%\build\code;%INCLUDE%
set KCPP_INCLUDE=%project_root%\build\code;%project_root%\code;%KML_HOME%\code-utilities
echo KCPP_INCLUDE==%KCPP_INCLUDE%
cd %project_root%
