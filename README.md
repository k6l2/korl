# KORL (Kyle's Operating system Resource Layer)

A minimal operating system abstraction layer, which can be used to build things like video games, simulations, or any other type of computer program that has user I/O.  Heavily inspired by [Casey Muratori's Handmade Hero](https://handmadehero.org/).

KORL is being built with the following tenets:

- _minimization_ of needless abstraction
- _minimization_ of project build times (unoptimized builds _must_ build in <= 1-5 seconds)
- _freedom_ from IDEs
- _improved_ APIs over the often-atrocious CRT (C Runtime) APIs
- _complete_ separation of user/game code from operating system APIs
- _complete_ control over where memory goes during run-time memory allocations
- core development features which Kyle feels there is no excuse to not have:
  - hot-reloading code
  - hot-reloading file assets
  - full-frame memory state snapshots at _all_ times, allowing the developer to debug crashes in symbol-stripped code by loading the state at the top of a frame in a build that includes debug symbols, instead of just being left with a cryptic & often useless memory dump
  - immediate GUI API
  - CPU & Memory metrics

## Dependencies

- [Vulkan SDK](https://vulkan.lunarg.com)

### Windows

- [Visual Studio](https://visualstudio.microsoft.com/downloads/)

## Project Status

- currently, there is only a Windows platform implementation, but I am planning to at least add WASM
- fully-functional I/O:
  - keyboard input
  - mouse input
  - gamepad input
    - XINPUT pads only, for now
  - 3D hardware rendering using Vulkan
    - support for PNG & JPEG textures
  - audio rendering
    - support for WAV & OGG/Vorbis audio files
  - application-specific filesystem I/O
