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
[ ] add `pool` as a  `utility` module
[ ] create new Resource database; just a Pool of Resources
[ ] add UTF16=>Pool_Handle data structure
  - test obtaining Resource Pool_Handle using a file name string
[ ] create a mechanism to register a "Resource type"
  - resource type registration requirements:
    - resource type id : u16
    - file transcoder : function*, codeModule, C API string
      - this is _optional_, as not all Resources will be decoded from raw file data!
        - needed for: IMAGE, SHADER, SCENE3D, AUDIO
    - flush : function*, codeModule, C API string
      - _optional_, as not all Resources need to be transcoded into a multimedia module
        - needed for: IMAGE, BUFFER, AUDIO (? for resampling?)
    - perhaps a global context of some kind?
      - useful for: 
        - global AUDIO resampling
        - perhaps for SCENE3D Resources, we can remember the Resource Type Id of MESH & IMAGE Resources
  - register an extremely simple resource type to test functionality
    - let's start with SHADER, since this is needed for _all_ drawing operations, and should let us at least perform primitive drawing operations
[ ] test korl-resource by drawing a bunch of korl-gfx primitives using simple SHADER Resources
  - only pipelines that don't use images/samplers for now...
  - no BUFFER resources just yet
  [ ] test SHADER resource hot-reloading
  [ ] test SHADER resource compatibility with korl-memoryState
[ ] add functionality for Resource hierarchy
  - compose a SCENE3D Resource out of MESH & IMAGE Resources
    - GLB files can contain IMAGE Resources!
  - compose MESH Resource out of BUFFER & MATERIAL Resources
  - compose MATERIAL Resource out of SHADER & IMAGE Resources
    - MATERIAL Resources can reference IMAGE Resources; we want to use the same IMAGE Resource Handles that are children of the SCENE3D potentially
[ ] finish implementation & registration of all current Resource types
