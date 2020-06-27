# Kyle's Media Layer
Currently, only the Windows platform is a thing.

## Installation Procedures
- Install Visual Studio w/ C++ to get all the required build tools.
- Clone repository somewhere.
- [Clone the `kasset` repository somewhere.](https://github.com/k6l2/kasset)
- Set the `VISUAL_C_BUILD_SCRIPTS_HOME` environment variable to point to the
	location of `vcvarsall.bat`.
- Set the `KML_HOME` environment variable to point to the root directory of this
	repository.
- Set the `KASSET_HOME` environment variable to point to the root directory of 
	the `kasset` repository.

## Build Procedures
- Run `%KML_HOME%\game\misc\shell.bat`.
- Execute `build` from this windows shell.

## Visual Studio Debugger
- Run `devenv build\Win32-KML.exe` to debug the project in Visual Studio.

## Optional Editor
- Run `code ..` to use VSCode, conveniently configured to build & debug 
	in-editor.

## Create a New Game Project Procedures
- Follow `Installation Procedures` above to install KML somewhere.
- Create a new directory anywhere.
- Copy the `%KML_HOME%\game` directory contents into this new directory.
- Edit the contents of `misc\shell.bat` in the new directory as you see fit.
- Optionally, copy the `%KML_HOME%\.vscode` folder into the new directory.

## Developing a New Game Project Procedures
- Follow `Build Procedures` using the new directory instead of 
	`%KML_HOME%\game`.
- Follow `Visual Studio Debugger` using whatever you set `kmlApplicationName` in 
	`misc\shell.bat` instead of `Win32-KML`.exe.
- Optionally, follow `Optional Editor` for this new directory using `.` instead 
	of `..`.
- We gucci, fam'.