#version 450
layout(location = 0) in vec4 fragmentColor;
//layout(location = 1) in vec2 fragmentUv;
layout(location = 0) out vec4 outColor;
void main() 
{
    outColor = fragmentColor;
}
