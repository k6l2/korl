# Kyle's Media Layer
Currently, only the Windows platform is a thing.

## Installation Procedures
### Windows
- [Install Visual Studio w/ C++ to get all the required build tools.](https://visualstudio.microsoft.com/vs/community/)
- [Install Git for Windows.](https://git-scm.com/download/win)
- Clone this repository somewhere.
- Run `misc/install.bat`!

## Build Procedures
- Run `%KML_HOME%\samples\*\misc\shell.bat`.
- Execute `build` from this windows shell.

## Visual Studio Debugger
- Run `devenv build\Win32-KML.exe` to debug the project in Visual Studio.

## Optional Editor
- Run `code %KML_HOME%` to use VSCode, conveniently configured to build & debug 
	in-editor.

## Create a New Game Project Procedures
- Follow `Installation Procedures` above to install KML somewhere.
- Create a new directory anywhere.
- Copy the `%KML_HOME%\samples\000-minimal-template` directory contents into 
	this new directory.
- Edit the contents of `misc\shell.bat` in the new directory as you see fit.
- Optionally, copy the `%KML_HOME%\.vscode` folder into the new directory.

## Developing a New Game Project Procedures
- Follow `Build Procedures` using the new directory instead of 
	`%KML_HOME%\game`.
- Follow `Visual Studio Debugger` using whatever you set `kmlApplicationName` in 
	`misc\shell.bat` instead of `Win32-KML`.exe.
- Explore `%KML_HOME%\samples` for code samples performing various common tasks, 
	such as drawing things to the window, playing sounds/music, networking & 
	more!
- Optionally, follow `Optional Editor` for this new directory using `.` instead 
	of `%KML_HOME%`.
- We gucci, fam'.