# @TODO: Geometry Shader Feature

This feature would give us an easy way to do fairly useful things like:
- debug draw normal vectors
- single-pass wireframe rendering
    http://www2.imm.dtu.dk/pubdb/edoc/imm4884.pdf

After researching this a bit, the geometry stage seems like it's being deprecated in modern rendering hardware in favor of task/mesh shader stages:
- Metal API doesn't support geometry shaders at all, but is supporting mesh shaders in Metal 3
- UE Nanite uses mesh shaders
- everyone online just seem to just know that geometry shaders are bad performance

So really, this feature's implementation should be emphasized as _optional_.

[ ] KORL-FEATURE-000-000-070: vulkan: geometry shader support
    [x] update korl-build-glsl.bat to handle .geom shaders
    [x] _optionally_ enable geometry shader feature via VkPhysicalDeviceFeatures
    [x] add geometry VkShaderModule to _Korl_Vulkan_Pipeline state
    [x] add geometry shader Resource_Handle to material shader
    [x] test a material configured to use a geometry shader
