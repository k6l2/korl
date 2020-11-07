# Kyle's Operating System Resource Layer (KORL)

## Why use KORL? (Features)
- KORL is EASY to install!  There are only two prerequisites to install: Git & 
	the compiler tools.  There is no need for any nonsense like building 
	libraries with bloated build tools, setting up the environment to use them, 
	etc...  Just a simple installer script does everything for you.
- KORL applications compile QUICKLY!  On Kyle's laptop, a full KORL application 
	in unoptimized build of a non-trivial application with cached KORL build 
	tools compiles in 3-4 SECONDS.  Good luck doing that with a standard game 
	engine.  KORL build tools are automatically cached on the first application 
	build, and are simply reused for all future builds on all other KORL 
	applications.
- KORL abstracts boring operating-system-specific code away from you!  Play 
	sounds & music, draw cool 3D graphics, use game pads, and load arbitrary 
	resources from the hard drive all without having to write any operating 
	system driver code.  You are here to make cool programs, not read through 
	terrible & often useless documentation about the platform your application 
	runs on.
- KORL application code can be hot-reloaded at any time!  This allows the 
	application programmer to tweak code features without having to reload the 
	application and return to the same state applicable to said feature.  This 
	feature does not require MSVC's compile-and-continue feature, and can 
	function on any reasonable compiler & platform.  Note: this feature assumes 
	the programmer is competent and does not store much important memory 
	statically in the application's dynamic code module.
- KORL applications give you complete control of application memory!  Gone are 
	the days where you have no idea what the minimum memory requirements of your 
	application are, or where they are stored at runtime.  KORL by default does 
	not use bloated/convoluted modern C++ features which take control of memory 
	away from the programmer (Of course, you are welcome to use those features 
	if you desire at your own risk).
- Currently, only the Windows platform is a thing, but it should be very simple 
	to port a KORL application to another platform, since applications developed 
	using KORL do not care about how the platform manages any resource!

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