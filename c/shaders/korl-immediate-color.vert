#version 450
layout(location = 0) in vec3 attributePosition;
layout(location = 1) in vec3 attributeColor;
layout(location = 0) out vec3 fragmentColor;
void main() 
{
    gl_Position = vec4(attributePosition, 1.0);
    fragmentColor = attributeColor;
}
