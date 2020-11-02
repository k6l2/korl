# Kyle's Operating System Resource Layer (KORL)
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
- Run `%korl_home%\samples\*\misc\shell.bat`.
- Execute `build` from this windows shell.

## Visual Studio Debugger (Windows)
- Run `debug` from the KORL environment shell to debug the project in Visual 
	Studio.

## Create a New Project Procedures
### Windows
- Follow `Installation Procedures` above to install KORL somewhere.
- Run `%korl_home%/misc/create-project.bat`!
- Optionally, edit the contents of `misc\shell.bat` in the new project directory 
	as you see fit.

## Optional Editor
- Run `code %korl_home%` to use VSCode, conveniently configured to build & debug 
	in-editor.

## Developing a New Project
- Follow `Build Procedures` using the new project directory instead of 
	`%korl_home%\samples\*`.
- Explore `%korl_home%\samples` for code samples performing various common tasks, 
	such as drawing things to the window, playing sounds/music, networking & 
	more!
- Optionally, follow `Optional Editor` for this new directory using 
	`%korlApplicationName%.code-workspace` instead of `%korl_home%`.
- We gucci, fam'.