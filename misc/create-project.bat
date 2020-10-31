@echo off
setlocal
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




set /p projectName=Enter project name:
if not exist "%projectFolderParentPath%/%projectName%" (
	mkdir "%toolchain_install_path%/%projectName%"
)
pushd "%projectFolderParentPath%/%projectName%"







rem return from %projectFolderParentPath%/%projectName%
popd
endlocal
exit /b 0