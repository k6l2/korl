# @TODO: vulkan-refactor

The main reason for this branch is because all the old "batch" APIs from korl-gfx & korl-vulkan are terribly flawed.  
Their original goal was to achieve an SFML-like drawing API for primitives.  
Unfortunately, not only was a simple API not achieved, but they also _required_ at least one implicit pointless memory copy for all immediate draw calls, since the "Drawable" object maintained its own vertex buffer, and it needed to copy this data to vulkan staging memory every time it needed to draw.
This time, we are going to construct a Vulkan API to just give the user direct access to the transient staging memory buffer, so they can dump whatever data they want to it at any point for the rest of the frame.  In doing so, we allow the user to generate their immediate draw call mesh data directly into staging memory, without the need to do any memory copies.

[x] build new korl-vulkan API to allow immediate draw calls
[x] add support for construction & drawing of data stored in Vulkan Buffers
    - at this point, we should be able to draw meshes from simple GLB files again
[x] refactor korl-gui to not use old "korl-gfx-batch" APIs
    [x] draw quads
    [x] draw triangles (with vertex colors)
    [x] draw text
    [x] draw buffered text (Korl_Gfx_Text objects)
[x] refactor all the garbage from korl-gfx to use new APIs
    [x] destroy all the korl-batch* nonsense
        [x] KORL-ISSUE-000-000-097: interface-platform, gfx: maybe just destroy Korl_Gfx_Batch & start over, since we've gotten rid of the concept of "batching" in the platform renderer; the primitive storage struct might also benefit from being a polymorphic tagged union
        [x] KORL-ISSUE-000-000-005: simplify: is it possible to just have a "createRectangle" function, and then add texture or color components to it in later calls?  And if so, is this API good (benefits/detriments to usability/readability/performance?)? My initial thoughts on this are: (1) this would introduce unnecessary performance penalties since we cannot know ahead of time what memory to allocate for the batch (whether or not we need UVs, colors, etc.) (2) the resulting API might become more complex anyways, and increase friction for the user
    [x] refactor Text stuff
    [x] refactor korl-gui to use this stuff
[x] flesh out immediate draw calls to make them as streamlined as possible
    - ???
[x] add support for modification of vertex attribute data for an existing "Drawable" primitive
    - for example: we create a quad (with or without vertex color attribute), draw it, then we want to change the quad's vertex colors & draw it somewhere else
[x] KORL-ISSUE-000-000-096: interface-platform, gfx: get rid of all "Vulkan" identifiers here; we don't want to expose the underlying renderer implementation to the user!
[x] KORL-ISSUE-000-000-155: vulkan/gfx: separate depth test & depth write operations in draw state
[x] KORL-ISSUE-000-000-160: vulkan: Korl_Gfx_DrawState_Material; likely unnecessary abstraction
[~] KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    - we _should_ be able to do this now, since korl-gfx now manages a default texture
[ ] finish @TODOs
[ ] ensure usability/versatility of the new rendering APIs by integrating this branch with a more fleshed out project, such as the `farm` project