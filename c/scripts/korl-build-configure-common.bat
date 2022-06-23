@REM Variables created here that are in SCREAMING_SNAKE_CASE will be carried all 
@REM the way to the actual build command.  Other variables like camelCase ones 
@REM will be optionally used/discarded in specific configuration scripts.
@echo off
rem ----- set common disabled warnings -----
rem 4061: enumerator 'X' in switch of enum '<X>' is not explicitly handled by a 
rem       case label; WHO CARES?  4062 will prevent us from not having at least 
rem       a default case to handle these enum values, which should catch most of 
rem       these types of bugs, assuming you write decent default case logic...  
rem       I just don't think this warning is useful since it seems more useful 
rem       to have the ability to switch on a subset of enum values, and dump the 
rem       rest into the default case.
rem 4201: nonstandard extension used: nameless struct/union; this would be a 
rem       problem if we cared about supporting C98 (these were added in C11)
rem 4505: unreferenced function with internal linkage has been removed; this is 
rem       completely useless as far as I can tell, since if we try to call a 
rem       function that isn't defined in the module, the compiler will complain
rem 4514: unreferenced inline function has been removed; see 4505
rem 4706: assignment within conditional expression; I will leave this off until 
rem       the day that I ever make a mistake which this would have caught
rem 4710: function not inlined; I just don't care about this, and don't see 
rem       myself caring about this anytime soon
rem 4711: function 'X' selected for automatic inline expansion; this only 
rem       happens in optimized builds, and I don't really care about code size 
rem       metrics or how the asm looks at all right now.  Maybe in the future...
set KORL_DISABLED_WARNINGS=/wd4061 /wd4201 /wd4505 /wd4514 /wd4706 /wd4710 /wd4711
rem 4100: unreferenced formal parameter; same reasoning as 4189
rem 4101: unreferenced local variable; same reasoning as 4189 
rem       (why are these different warnings? lol)
rem 4189: local variable is initialized but not referenced; this is useful for 
rem       debugging transient values, but introduces possible bugs so it 
rem       shouldn't be part of release builds
set korlDisabledReleaseWarnings=/wd4100 /wd4101 /wd4189
rem 4820: 'x' bytes padding added after data member 'y'; data structures should 
rem       be fully optimized in released builds, but during development this is 
rem       just annoying
rem 5045: Compiler will insert Spectre mitigation for memory load if /Qspectre 
rem       switch specified
set korlDisabledOptimizationWarnings=/wd4820 /wd5045
rem ----- set common compiler include directories -----
set "INCLUDE=%KORL_VS_ORIGINAL_INCLUDE%"
set "INCLUDE=%INCLUDE%;%VULKAN_SDK%\Include"
set "INCLUDE=%INCLUDE%;%KORL_PROJECT_ROOT%\code"
set "INCLUDE=%INCLUDE%;%KORL_HOME%\code"
set "INCLUDE=%INCLUDE%;%KORL_HOME%\code\stb"
set "INCLUDE=%INCLUDE%;%KORL_HOME%\code\sort"
rem ----- set common compiler library search directories -----
set "LIB=%KORL_VS_ORIGINAL_LIB%"
set "LIB=%LIB%;%VULKAN_SDK%\Lib"
rem ----- set common compiler options -----
set "CL="
set "_CL_="
rem     disable annoying verbose compiler output
set "_CL_=%_CL_% /nologo"
rem     ONLY compile; do not link!
set "_CL_=%_CL_% /c"
rem     set source & execution character set to UTF-8; required in order to 
rem     correctly store & display unicode characters apparently 😒
set "_CL_=%_CL_% /utf-8"
rem     treat warnings as errors
set "_CL_=%_CL_% /WX"
rem     enable all warnings
set "_CL_=%_CL_% /Wall"
rem     generate debug symbols 
rem   Zi => no "edit-and-continue" support + a "VCX.pdb" PDB file
rem   Z7 => no "edit-and-continue" support, no "VCX.pdb" PDB file, not compatible w/ incremental linking, potentially faster?
set "_CL_=%_CL_% /Zi"
rem     generate compiler intrinsics
set "_CL_=%_CL_% /Oi"
rem     disable run-time type information (dynamic_cast, typeid)
set "_CL_=%_CL_% /GR-"
rem     display the full path of source code files passed in diagnostics
set "_CL_=%_CL_% /FC"
rem     for multi-line errors/warnings, the additional info lines are all placed 
rem     on the same line, instead of being broken up into multiple lines
set "_CL_=%_CL_% /WL"
rem     potentially very useful for optimizing builds by eliminating bloated 
rem     standard libraries!
@REM set "_CL_=%_CL_% /showIncludes"
rem ----- set common linker options -----
set "KORL_LINKER_OPTIONS="
rem     disable useless "startup banner" message in linker output
set "KORL_LINKER_OPTIONS=%KORL_LINKER_OPTIONS% /NOLOGO"
rem     we don't need to prepare for any subsequent incremental links, so 
rem     disable any generation of padding/THUNKS; also disables generation of 
rem     *.ilk files
set "KORL_LINKER_OPTIONS=%KORL_LINKER_OPTIONS% /INCREMENTAL:NO"
rem     set the checksum in the header of the .exe file (required in order to 
rem     allow WinDbg to verify the timestamp of the executable, apparently)
set "KORL_LINKER_OPTIONS=%KORL_LINKER_OPTIONS% /RELEASE"
