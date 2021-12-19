#version 450
layout(binding = 1) uniform sampler2D textureSampler;
//layout(location = 0) in vec3 fragmentColor;
layout(location = 1) in vec2 fragmentUv;
layout(location = 0) out vec4 outColor;
void main() 
{
    outColor = texture(textureSampler, fragmentUv);
    //outColor = vec4(fragmentUv, 0.0, 1.0);
    //outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
