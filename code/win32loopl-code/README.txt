"Portability":
  Code should build with any complaint C89 or later compiler.
  Code uses no compiler extensions.
  Code only uses Kernel32 and User32 functions.
  Code works with both ANSI and Unicode windows.
  Code should run under any 32-bit Windows OS, ReactOS, or Wine.

Resource usage:
  Code uses one thread local slot (just to prevent the need for locking,
    probably not necessary and may change in the future)
  Uses very little memory. A little more with the addition of aero features, but not much.


This code has been tested under the following:
  Compile: MinGW-W64 (32-bit)
           -wall -wextra -std=c89 -pedantic
  Run LSMTest: Windows XP 32-bit
               Windows 7 32-bit


To use this code:
  1) Copy the files LooplessSizeMove.c and LooplessSizeMove.h
    into your project's source directory
  2) Add at least LooplessSizeMove.c to your project in Visual
    Studio or your IDE of choice.
  3) Add a call to SizingCheck before calling DispatchMessage,
    like so:
        TranslateMessage(&msg);
        SizingCheck(&msg);
        DispatchMessage(&msg);
  4) In the Window procedure of any Window you would like to
    enable do this for, call LSMProc instead of DefWindowProc
    (LSMProc only processes a couple specific messages and calls
    DefWindowProc for all other messages).
  5) (new as of rev 8) If you create new GUI threads using this
    functionality, you should call LSMCleanup() before exiting the
    thread to prevent memory and resource leaks. You should also
    call this before exiting the process altogether.

Caveat:
  We have to do a lot of processing and use less efficient documented API calls.
  This doesn't seem to be causing any problems though.
