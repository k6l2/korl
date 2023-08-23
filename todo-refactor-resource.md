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
        - needed for: TEXTURE, SHADER, SCENE3D, AUDIO
    - pre-process : function*, codeModule, C API string
      - _optional_, as not all Resources need pre-processing
        - needed for: TEXTURE (pre-multiply alpha)
        - pre-processed assets should be saved/cached on the hard drive in "build/data/" directory
          - fuck... I don't think stb-image has an encoder... ☹
            - https://github.com/lvandeve/lodepng
              - Zlib license
          - can we write the saved asset with the same metadata (last-write timestamp)?
    - flush : function*, codeModule, C API string
      - _optional_, as not all Resources need to be transcoded into a multimedia module
        - needed for: TEXTURE, BUFFER, AUDIO (? for resampling?)
    - perhaps a global context of some kind?
      - useful for: 
        - global AUDIO resampling
        - perhaps for SCENE3D Resources, we can remember the Resource Type Id of MESH & TEXTURE Resources
  - register an extremely simple resource type to test functionality
    - let's start with SHADER, since this is needed for _all_ drawing operations, and should let us at least perform primitive drawing operations
[x] add UTF16=>Pool_Handle data structure
  - test obtaining Resource Pool_Handle using a file name string
[x] test korl-resource by drawing a bunch of korl-gfx primitives using simple SHADER Resources
  - only pipelines that don't use images/samplers for now...
  - no BUFFER resources just yet
  [x] test SHADER resource hot-reloading
  [x] test SHADER resource compatibility with korl-memoryState
[x] add BUFFER resource to test runtime-data-backed resources (same tests as above)
[x] add TEXTURE resource
[~] add functionality for Resource hierarchy
  [x] add FONT resource
    [x] KORL-ISSUE-000-000-129: gfx/font: move all "FontCache" functionality into korl-resource
      [x] create korl-resource-font out of recycled code
      [x] delete all font stuff in korl-gfx
    [x] KORL-PERFORMANCE-000-000-001: gfx: inefficient texture upload process
  [x] add BUFFER & TEXTURE child RUNTIME resources
    - this kinda already works without any special "Resource_Item DAG" support, so...
  [x] refactor & test korl-utility-gfx text drawing APIs
    - accept a FONT Korl_Resource_Handle instead of a utf16FontAssetName
[x] fix crash when loading a korl-memoryState that has the logConsole open
[x] investigate logConsole functionality breaking after loading a korl-memoryState
  - the crux of this issue is the fact that file-backed resources are _not_ being fully loaded after a memoryState load occurs
  - this causes the font resource we are using in korl-gui to return empty Font_Metrics, as the data hasn't been transcoded yet
  - this causes user code to divide (textPixelSize / 0), resulting in NaN, infecting various other korl-gui calculations
  - solutions:
    - force file-backed resources to transcode before continuing the rest of the frame, regardless of assetCache get flags (lazy flag, specifically)
      - this seems like the quick/dirty solution to a deeper issue with resource management; the user is still subject to attempts to utilize unloaded resources at any time during regular execution regardless of this specific memoryState-load issue
    - modify usage of all korl-resource-* APIs, such that the user must be responsible for checking if a resource is actually transcoded before attempting any operations on it, especially operations which bake other runtime resources from that resource's transcoded data
      - even though this is more annoying (more work), I feel like we should just do this, since it make sense for us to assume by default that a resource can be in an unloaded state at any time
      - there are only 2 places currently where korl-resource-font APIs are used (thankfully): korl-gui & korl-utility-gfx
      - what should we do when font resources are needed, but the font is not yet loaded?
        - in the case where we only care about the results for the current frame (not future frames), it's okay to do _nothing_
        - otherwise, we have to make sure to remember that we have not actually been able to use the resource yet
[x] test korl-resource-font hot-reload of font assetCache file
[ ] add SCENE3D Resource
  [x] transcode materials
    [x] transcode & manage runtime TEXTURE resources using raw PNG data (images.bufferView) & sampler data
    - MVP: obtain a material (via a material index), & use it to draw arbitrary geometry, such as a test quad
      - thankfully, there isn't really a need to create a MATERIAL resource, as far as I can tell so far...
  [x] transcode meshes
    - create & manage one MESH resource per mesh
    - store mesh primitives as Korl_Gfx_Drawables (BUFFER resources + vertex attribute meta data)
    - MVP: user can obtain a MESH resource handle from a SCENE3D resource (even if the SCENE3D resource is not yet transcoded), then render it
    - scratch all that, we're just going to draw meshes from a SCENE3D resource by looking up their index using their string name like before; it simplifies a lot of stuff
  [x] transcode extra material strings to assign korl shaders to material pipeline stages
  [x] transcode material.alphaMode to determine korl material.modes.flags
  [x] transcode specularFactor & specularColorFactor
  [x] refactor MESH Korl_Gfx_Drawable types to use the new scene3d APIs
  [ ] transcode skins
    - store inverseBindMatrices as BUFFER resource
    - store joint hierarchy information?
    - MVP: user can apply a skin pose to a MESH resource & draw it
      - we should also be able to make a simple code interface to manually drive individual joint transforms to modify the skin pose in real-time
      - https://webglfundamentals.org/webgl/lessons/webgl-skinning.html
    - scene3d_skin; what we need to transcode from the glb/gltf decoder output
      - pre-computed row-major bone inverseBindMatrices (so we can apply these to bones on the CPU)
      - topological bone order (root bone first, followed by all children)
    - SkinInstance
      - bones[skinBoneCount] : Korl_Math_Transform3d
        - initialized to the same value as its corresponding gltf.node
        - modifiable at-will by the user of SkinInstance
      - API: prepare : void
        - compute each bone's local=>world xform matrix
          - we do this by iterating over each bone in topological order
          - boneXform[i] = boneXform[parent] * boneLocal[i]
        - pre-multiply all boneXform matrices by their respective inverseBindMatrix
          - boneXform[i] *= inverseBindMatrix[i]
        - return a staging buffer to the final boneXform matrices
    - wherever mesh gets rendered:
      - pass the SkinInstance we want to use on the skinned mesh
    - inside of draw call:
      - send boneXform array to graphics device via `setDrawState`
      - modify mesh material to utilize skinning (+ lighting) vertex shader
  [ ] transcode animations
    - MVP: user can apply animations to mesh skins to render an animated MESH
[ ] add AUDIO Resource (last resource type)
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
