@echo off
setlocal
rem prompt the user for the directory which will contain the project folder
set "psCmd=&{"
set "psCmd=%psCmd%[System.Reflection.Assembly]::LoadWithPartialName('System.windows.forms') | Out-Null;"
set "psCmd=%psCmd%$FolderBrowserDialog = New-Object System.Windows.Forms.FolderBrowserDialog;"
set "psCmd=%psCmd%$FolderBrowserDialog.Description = \"Select project folder's parent directory:\";"
set "psCmd=%psCmd%$FolderBrowserDialog.ShowDialog()|out-null;"
set "psCmd=%psCmd%$FolderBrowserDialog.SelectedPath}"
for /f "delims=" %%I in ('powershell -noprofile -command "%psCmd%"') do (
	set "projectFolderParentPath=%%I"
)
echo projectFolderParentPath="%projectFolderParentPath%"



rem prompt the user for the name of the project & project's folder
set /p projectName=Enter project name:
if not exist "%projectFolderParentPath%/%projectName%" (
	mkdir "%projectFolderParentPath%/%projectName%"
)
pushd "%projectFolderParentPath%/%projectName%"


rem conditionally create the `misc` directory in our project folder
if not exist "misc" ( mkdir "misc" )



rem conditionally create a new `shell.bat`
if not exist "misc/shell.bat" (
	echo @rem prerequisites: install.bat has been run> "misc/shell.bat"
	echo @rem simply launch a command prompt with the proper environment variables>> "misc/shell.bat"
	echo @rem necessary to build the project>> "misc/shell.bat"
	echo set korlApplicationName=%projectName%>> "misc/shell.bat"
	echo set korlApplicationVersion=r0>> "misc/shell.bat"
	echo set korlGameDllFileName=korlDynApp>> "misc/shell.bat"
	echo @rem To prevent simulation from becoming unstable at low frame rates due to lag>> "misc/shell.bat"
	echo @rem	etc, `korlMinimumFrameRate` will ensure that the platform layer never>> "misc/shell.bat"
	echo @rem	sends game code a deltaSeconds value less than 1/%korlMinimumFrameRate%!>> "misc/shell.bat"
	echo set korlMinimumFrameRate=30>> "misc/shell.bat"
	echo call %%windir%%\system32\cmd.exe /k %%korl_home%%\misc\env.bat %%~dp0>> "misc/shell.bat"
)




rem conditionally copy the `code` & `assets` folders from samples-000
if not exist "code" (
	mkdir "code"
	xcopy "%korl_home%/samples\000-minimal-template/code" "code" /E
	rem robocopy %korl_home%/code . /COPYALL /E
)
if not exist "assets" (
	mkdir "assets"
	xcopy "%korl_home%/samples\000-minimal-template/assets" "assets" /E
	rem robocopy %korl_home%/assets . /COPYALL /E
)




rem conditionally add VSCode dotfiles
if not exist ".vscode" (
	mkdir ".vscode"
	xcopy "%korl_home%\.vscode" ".vscode" /E
)



rem conditionally add git dotfiles
if not exist ".gitignore" (
	xcopy "%korl_home%\.gitignore" "."
)
if not exist ".gitattributes" (
	xcopy "%korl_home%\.gitattributes" "."
)

rem convert korl_home path to forward slashes to prevent JSON errors
set "korl_home=%korl_home:\=/%"
rem generate a VSCode `*.code-workspace` file
if not exist "%projectName%.code-workspace" (
	echo {> "%projectName%.code-workspace"
	echo 	"folders": [>> "%projectName%.code-workspace"
	echo 		{>> "%projectName%.code-workspace"
	echo 			"path": ".">> "%projectName%.code-workspace"
	echo 		},>> "%projectName%.code-workspace"
	echo 		{>> "%projectName%.code-workspace"
	echo 			"name": "KGT Code Utilities",>> "%projectName%.code-workspace"
	echo 			"path": "%korl_home%/code-utilities">> "%projectName%.code-workspace"
	echo 			/* apparently VsCode does not support environment variable >> "%projectName%.code-workspace"
	echo 				resolution in *.code-workspace files...>> "%projectName%.code-workspace"
	echo 				https://github.com/microsoft/vscode/issues/44755 */>> "%projectName%.code-workspace"
	echo 			//"path": "${env:korl_home}/code-utilities">> "%projectName%.code-workspace"
	echo 		},>> "%projectName%.code-workspace"
	echo 		{>> "%projectName%.code-workspace"
	echo 			"name": "KORL Code",>> "%projectName%.code-workspace"
	echo 			"path": "%korl_home%/code">> "%projectName%.code-workspace"
	echo 			/* apparently VsCode does not support environment variable >> "%projectName%.code-workspace"
	echo 				resolution in *.code-workspace files...>> "%projectName%.code-workspace"
	echo 				https://github.com/microsoft/vscode/issues/44755 */>> "%projectName%.code-workspace"
	echo 			//"path": "${env:korl_home}/code">> "%projectName%.code-workspace"
	echo 		},>> "%projectName%.code-workspace"
	echo 		{>> "%projectName%.code-workspace"
	echo 			"name": "KORL Misc.",>> "%projectName%.code-workspace"
	echo 			"path": "%korl_home%/misc">> "%projectName%.code-workspace"
	echo 			/* apparently VsCode does not support environment variable >> "%projectName%.code-workspace"
	echo 				resolution in *.code-workspace files...>> "%projectName%.code-workspace"
	echo 				https://github.com/microsoft/vscode/issues/44755 */>> "%projectName%.code-workspace"
	echo 			//"path": "${env:korl_home}/misc">> "%projectName%.code-workspace"
	echo 		}>> "%projectName%.code-workspace"
	echo 	],>> "%projectName%.code-workspace"
	echo 	"settings": {>> "%projectName%.code-workspace"
	echo 		"C_Cpp.default.cppStandard": "c++20",>> "%projectName%.code-workspace"
	echo 		"C_Cpp.default.includePath": >> "%projectName%.code-workspace"
	echo 			[ "${env:project_root}/code">> "%projectName%.code-workspace"
	echo 			, "${env:project_root}/build/code">> "%projectName%.code-workspace"
	echo 			, "${env:korl_home}/code-utilities">> "%projectName%.code-workspace"
	echo 			, "${env:korl_home}/code" ],>> "%projectName%.code-workspace"
	echo 		"C_Cpp.default.defines": >> "%projectName%.code-workspace"
	echo 			[ "INTERNAL_BUILD=1;SLOW_BUILD=1;KORL_MINIMUM_FRAME_RATE=30" ]>> "%projectName%.code-workspace"
	echo 	}>> "%projectName%.code-workspace"
	echo }>> "%projectName%.code-workspace"
)



rem return from %projectFolderParentPath%/%projectName%
popd
endlocal
exit /b 0