# @TODO: GPU-Driven Rendering

- with a minimum-viable-product 3D game application finally up and running, the inefficiencies of the GFX/Vulkan modules are now a huge bottle-neck with respect to CPU performance; we need to optimize the rendering functionality of KORL to make it viable in a real-world application
- our objective here is to optimize rendering such that 
    - we can draw more 3D stuff
    - we don't over-burdon the user of the GFX APIs
- currently, my research is pointing me towards so-called GPU-driven architecture: 
    - https://vkguide.dev/docs/gpudriven/gpu_driven_engines/
    - the idea is that we move towards a "bindless" approach to graphics resources
    - once we minimize resource binds, we can utilize indirect-draw APIs to dispatch _minimal_ draw calls
- in addition to the optimization of draw calls, we will first refactor the graphics APIs to allow user configurable "render graph"
    - support for user-configurable RenderPasses
    - configure a render graph via connection between render graph nodes & {transient, persistent} resources (framebuffers, etc.)
    - the user then builds graphics command buffers for each render pass

# Vulkan Pipeline Notes

- Pipeline
    - RenderPass
    - PipelineLayout
    - ShaderStage[]
    - VertexInputState
    - InputAssemblyState
    - ViewportState
    - RasterizationState
    - MultisampleState
    - ColorBlendState
    - DepthStencilState
    - DynamicState

    - RenderPass
        - AttachmentDescription[]
        - SubpassDescription[]
        - SubpassDependency[]

    - PipelineLayout
        - DescriptorSetLayout[]
        - PushConstantRange[]

        // for textures, use texture array pools + per-object(instance) texture index offsets
        // for material UBO, use a giant array + per-object(instance) index offset
        // for bones UBO, use giant array + per-object(instance) index offset
        // for vertex SSBO (glyph mesh vertices), use giant array
        // for lights SSBO, use giant array, and use some global values (push constants?) to determine which lights we must utilize
        // for SceneProperties UBO, use array + PushConstant index
        - DescriptorSetLayout
            - VkDescriptorSetLayoutBinding[]

            - VkDescriptorSetLayoutBinding
                - binding
                - type {UBO, SSBO, COMBINED_IMAGE_SAMPLER, ...}
                - stageFlags

        // stop using PushConstants for per-primitive draws; only use for "global" settings
        - PushConstantRange
            - stageFlags
            - byteOffset
            - byteSize

    // use few shaders; uber-shader approach //
    - ShaderStage
        - stage
        - module
        - pName

    // use one big vertex buffer //
    - VertexInputState
        - Binding[]
        - Attribute[]

# TODO list

[ ] disable all current korl-vulkan operations
    [ ] stubify all korl-vulkan platform APIs
    [ ] stubify all korl-vulkan internal APIs
        - no more vkBegin/EndRenderPass calls, etc.
[ ] allow user to compose a "render graph"
    - RenderNodes can be a resource (ImageBuffer/View) or a RenderPass; or, maybe RenderPass nodes are just a separate thing altogether
    - user can declare transient resources, such as framebuffers, or use existing persistent resources, such as texture images
    - user assigns RenderPass inputs (attachments) to either transient/persistent resources, or attachments to other RenderPasses, creating a graph structure
    - Node => {RenderPass | Resource}
    - Edge => source Node (attachment | resource) -> destination Node attachment
    [ ] allow user to create & manage RenderPass RenderNodes
    <!-- [ ] create a default "presentation" RenderPass, whose handle is accessible via a special korl-vulkan API -->
    [ ] allow user to create & manage FrameBuffer RenderNodes, using the previously-created RenderPass as input creation parameter
        - how do we create all the swap chain image framebuffers?
            - do we need swapChainImageCount special copies of just the final RenderNode?...  How/when do we determine this?... I am confus
            - do _all_ FrameBuffer RenderNodes just have swapChainImageCount copies?, and do we just pick the swapChainIndex sub-FrameBuffer of each FrameBuffer RenderNode of any given frame?  Seems... wasteful, but it would work I suppose
            - so, looking at Sascha Willems' vulkan example for offscreen (render-to-texture) example (https://github.com/SaschaWillems/Vulkan.git), he seems to be able to set up a separate off-screen render pass with a single image resource as the color attachment; the notes on the example say that explicit synchronization is not necessary since it is taken care of via SubpassDependencies
    <!-- [ ] create multiple default "swap chain framebuffers", whose handles are accessible via a special korl-vulkan API -->
    [ ] compose a default render graph to replicate the original behavior
        [ ] obtain swap chain color/depth attachments as RenderNode handles
        [ ] create a "presentation" RenderPass, and configure it to use the swap chain framebuffer attachment
[ ] allow user to compose & manage custom DescriptorSetLayouts
[ ] allow user to compose & manage custom PipelineLayouts
    - submit RenderPass RenderNode as a parameter to PipelineLayout creation
[ ] refactor descriptors to access descriptor arrays in each shader using an instance or PushConstant index
[ ] attempt to combine _all_ mesh data into a single buffer
[ ] re-implement all korl-gui drawing operations
[ ] re-implement all other drawing operations
[ ] KORL-ISSUE-000-000-145: vulkan: color attributes & uniforms are being mangled by linear=>SRGB conversion; texture colors & clear colors are unaffected, since those seem to match the swap chain format; maybe we just need to manually convert color values of attributes/uniforms in shaders?  https://www.shadertoy.com/view/4tXcWr ; _no_! what we _really_ should be doing is: (1) store textures in UNORM color-space, (2) draw everything to a transient UNORM image buffer, (3) at end-of-frame, transfer the transient UNORM image buffer to swapchain SRGB buffer, applying all necessary post-processing in one final render pass (including UNORM=>SRGB conversion); this technique would also allow us to support things like HDR, and custom post-processing shaders
[ ] KORL-ISSUE-000-000-184: vulkan: descriptor set flexibility; in here we are arbitrarily picking which data channels belong to each descriptor set; we _should_ be able to allow the user to define their own VkDescriptorSetLayouts & VkPipelineLayouts, which would certainly give Vulkan a much better idea of how to optimize, as well as give the user the ability to compose descriptors however they wish without having to modify this code :|; related to KORL-ISSUE-000-000-185
[ ] KORL-ISSUE-000-000-185: gfx: manage custom descriptor set layouts; here, we are currently manually selecting arbitrary descriptor data channels to propagate various data, which looks & feels very hacky; we should be able to modify korl-vulkan to allow us to create & manage our own custom descriptor set & pipeline layouts for specific rendering purposes; related to the todo item currently found in _korl_vulkan_flushDescriptors; related to KORL-ISSUE-000-000-184
