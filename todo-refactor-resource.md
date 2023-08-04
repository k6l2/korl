# @TODO: refactor korl-resource

## lessons learned

- do we actually need to construct ResourceHandles out of file name hashes?
  - yes, since we want the user to be able to call `fromFile(...)` at any time in the code and be able to obtain the exact same ResourceHandle
    - is this functionality actually important?
      - yeah, I think so, but... maybe my implementation is a bad idea; maybe Resources should just be a simple memory pool, and korl-resource can just manage a separate UTF16=>Pool_Handle/fileName=>ResourceHandle data structure for this feature; having Resource structs remain static in memory at all times will certainly help simplify implementation

## goals

- allow the user to configure custom resource types
  - KORL will configure its own built-in resource types by default
- support Resource hierarchies

## action items

[x] rip out all korl-resource code
[x] add `pool` as a  `utility` module
[x] create new Resource database; just a Pool of Resources
[x] create a mechanism to register a "Resource type"
  - resource type registration requirements:
    - resource type id : u16
    - file transcoder : function*, codeModule, C API string
      - this is _optional_, as not all Resources will be decoded from raw file data!
        - needed for: IMAGE, SHADER, SCENE3D, AUDIO
    - pre-process : function*, codeModule, C API string
      - _optional_, as not all Resources need pre-processing
        - needed for: IMAGE (pre-multiply alpha)
        - pre-processed assets should be saved/cached on the hard drive in "build/data/" directory
          - fuck... I don't think stb-image has an encoder... ☹
            - https://github.com/lvandeve/lodepng
              - Zlib license
          - can we write the saved asset with the same metadata (last-write timestamp)?
    - flush : function*, codeModule, C API string
      - _optional_, as not all Resources need to be transcoded into a multimedia module
        - needed for: IMAGE, BUFFER, AUDIO (? for resampling?)
    - perhaps a global context of some kind?
      - useful for: 
        - global AUDIO resampling
        - perhaps for SCENE3D Resources, we can remember the Resource Type Id of MESH & IMAGE Resources
  - register an extremely simple resource type to test functionality
    - let's start with SHADER, since this is needed for _all_ drawing operations, and should let us at least perform primitive drawing operations
[x] add UTF16=>Pool_Handle data structure
  - test obtaining Resource Pool_Handle using a file name string
[ ] test korl-resource by drawing a bunch of korl-gfx primitives using simple SHADER Resources
  - only pipelines that don't use images/samplers for now...
  - no BUFFER resources just yet
  [x] test SHADER resource hot-reloading
  [ ] test SHADER resource compatibility with korl-memoryState
[ ] add functionality for Resource hierarchy
  - compose a SCENE3D Resource out of MESH & IMAGE Resources
    - GLB files can contain IMAGE Resources!
  - compose MESH Resource out of BUFFER & MATERIAL Resources
  - compose MATERIAL Resource out of SHADER & IMAGE Resources
    - MATERIAL Resources can reference IMAGE Resources; we want to use the same IMAGE Resource Handles that are children of the SCENE3D potentially
[ ] finish implementation & registration of all current Resource types
[ ] perform defragmentation on korl-resource persistent memory
[ ] we're going to have to add some kind of system at some point to pre-process file resources, which requires having a "resource-manifest" file; why not just add this functionality now?
  - user defines a "resource-manifest.txt" file somewhere
  - KORL contains an internal tool "korl-inventory"
    - output build binaries to "%PROJECT_ROOT%/build/tools"
    - input: "resource-manifest.txt"
      - a list of all resource asset file names, relative to the root of the project's development directory
      - any additional options the user wants to perform pre-processing 
    - output:
      - a "korl-resource.h" file
        - compile-time enumeration of resource indices
        - constant UTF strings for each file name
        - save this file to "%PROJECT_ROOT%/build/code/"
      - an "assets.bin" file
        - pre-processed raw file assets
        - at the end of file, a manifest containing a directory of all contained assets
          - by using a trailing manifest at end-of-file, this allows us to easily merge the "assets.bin" file with the game's executable to create a 1-file distributable (if desired)
          - maybe include the file's "last-edited" timestamp here, so we can still implement a reasonably efficient hot-reloading solution
[ ] test hot-reloading of code module that contains a registered resource descriptor
  - followed by hot-reload of one of the resources using said descriptor
  - to ensure that we are updating function pointers of resource descriptor callbacks
