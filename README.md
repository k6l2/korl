# Kyle's Media Layer
Currently, only the Windows platform is a thing.

## Installation Procedures
### Windows
- [Install Visual Studio w/ C++ to get all the required build tools.](https://visualstudio.microsoft.com/vs/community/)
- [Install Git for Windows.](https://git-scm.com/download/win)
- Clone this repository somewhere.
- Run `misc/install.bat`!
- [Optionally, install VSCode.](https://code.visualstudio.com/)

## Build Procedures
### Windows
- Run `%KML_HOME%\samples\*\misc\shell.bat`.
- Execute `build` from this windows shell.

## Visual Studio Debugger (Windows)
- Run `debug` from the KML environment shell to debug the project in Visual 
	Studio.

## Create a New Project Procedures
### Windows
- Follow `Installation Procedures` above to install KML somewhere.
- Run `%kml_home%/misc/create-project.bat`!
- Optionally, edit the contents of `misc\shell.bat` in the new project directory 
	as you see fit.

## Optional Editor
- Run `code %KML_HOME%` to use VSCode, conveniently configured to build & debug 
	in-editor.

## Developing a New Project
- Follow `Build Procedures` using the new project directory instead of 
	`%KML_HOME%\samples\*`.
- Explore `%KML_HOME%\samples` for code samples performing various common tasks, 
	such as drawing things to the window, playing sounds/music, networking & 
	more!
- Optionally, follow `Optional Editor` for this new directory using 
	`%kmlApplicationName%.code-workspace` instead of `%KML_HOME%`.
- We gucci, fam'.