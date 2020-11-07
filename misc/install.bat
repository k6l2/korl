@echo off
setlocal


set "psCmd=&{"
set "psCmd=%psCmd%[System.Reflection.Assembly]::LoadWithPartialName('System.windows.forms') | Out-Null;"
set "psCmd=%psCmd%$FolderBrowserDialog = New-Object System.Windows.Forms.FolderBrowserDialog;"
set "psCmd=%psCmd%$FolderBrowserDialog.Description = \"Select KORL toolchain install path:\";"
set "psCmd=%psCmd%$FolderBrowserDialog.ShowDialog()|out-null;"
set "psCmd=%psCmd%$FolderBrowserDialog.SelectedPath}"
for /f "delims=" %%I in ('powershell -noprofile -command "%psCmd%"') do (
	set "SelectedPath=%%I"
)
echo SelectedPath="%SelectedPath%"





set toolchain_install_path=%SelectedPath%
set vswhere_location=%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere
for /f "usebackq delims=#" %%a in (`"%vswhere_location%" -latest -property installationPath`) do (
	echo Setting environment variable 'VsDevCmd_Path=%%a\Common7\Tools\VsDevCmd.bat'
	setx VsDevCmd_Path "%%a\Common7\Tools\VsDevCmd.bat"
)
if not exist "%toolchain_install_path%" (
	mkdir "%toolchain_install_path%"
)
pushd "%toolchain_install_path%"
call :installTool kmd5
call :installTool kasset
call :installTool kcpp
popd



rem @TODO: check to see if anything didn't work
cd %~dp0
cd ..
echo Setting environment variable 'korl_home=%cd%'
setx korl_home "%cd%"
pause
endlocal
exit /b 0




rem @param %~1 name of build tool to install
:installTool
	setlocal
	if exist "%~1" (
		echo Updating %~1... "%toolchain_install_path%/%~1"
		pushd "%~1"
		git pull
		popd
		echo %~1 update complete!
	) else (
		echo Installing %~1... "%toolchain_install_path%/%~1"
		git clone https://github.com/k6l2/%~1
		echo %~1 install complete!
	)
	echo Setting environment variable '%~1_home=%toolchain_install_path%\%~1'
	setx %~1_home "%toolchain_install_path%\%~1"
	endlocal
	exit /b 0