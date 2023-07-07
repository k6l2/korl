# @TODO: vulkan-refactor

The main reason for this branch is because all the old "batch" APIs from korl-gfx & korl-vulkan are terribly flawed.  
Their original goal was to achieve an SFML-like drawing API for primitives.  
Unfortunately, not only was a simple API not achieved, but they also _required_ at least one implicit pointless memory copy for all immediate draw calls, since the "Drawable" object maintained its own vertex buffer, and it needed to copy this data to vulkan staging memory every time it needed to draw.
This time, we are going to construct a Vulkan API to just give the user direct access to the transient staging memory buffer, so they can dump whatever data they want to it at any point for the rest of the frame.  In doing so, we allow the user to generate their immediate draw call mesh data directly into staging memory, without the need to do any memory copies.

[x] build new korl-vulkan API to allow immediate draw calls
[x] add support for construction & drawing of data stored in Vulkan Buffers
    - at this point, we should be able to draw meshes from simple GLB files again
[ ] refactor korl-gui to not use old "korl-gfx-batch" APIs
    [x] draw quads
    [x] draw triangles (with vertex colors)
    [ ] draw text
[ ] refactor all the garbage from korl-gfx to use new APIs
    [ ] destroy all the korl-batch* nonsense
    [ ] refactor Text stuff
    [ ] refactor korl-gui to use this stuff
[ ] flesh out immediate draw calls to make them as streamlined as possible
    - ???
[ ] add support for modification of vertex attribute data for an existing "Drawable" primitive
    - for example: we create a quad (with or without vertex color attribute), draw it, then we want to change the quad's vertex colors & draw it somewhere else