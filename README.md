# Kyle's Media Layer
Currently, only the Windows platform is a thing.

## Installation Procedures
- Install Visual Studio w/ C++ to get all the required build tools.
- Clone repository somewhere.
- Set the `VISUAL_C_BUILD_SCRIPTS_HOME` environment variable to point to the
	location of `vcvarsall.bat`.
- Set the `KML_HOME` environment variable to point to the root directory of this
	repository.

## Build Procedures
- Run `%KML_HOME%\misc\shell.bat`.
- Execute `build` from this windows shell.

## Visual Studio Debugger
- Run `devenv build\win32-main.exe` to debug the project in Visual Studio.

## Optional Editor
- Run `code .` to use VSCode, conveniently configured to build & debug 
	in-editor.

## Create a New Game Project Procedures
- Follow `Installation Procedures` above to install KML somewhere.
- Create a new directory anywhere.
- Copy `misc\shell.bat` into this directory in the same folder structure & edit 
	the options as you see fit.
- Copy `code\game.*` into this directory in the same folder structure.
- Optionally, copy the `.vscode` folder into this directory.
- Follow the `Build Procedures` & `Visual Studio Debugger` steps in this 
	document for this directory.
- Optionally, follow `Optional Editor` for this directory.
- We gucci, fam'.