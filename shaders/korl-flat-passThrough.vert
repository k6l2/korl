/** The point of this shader is to just send vertex attributes to the geometry stage.  
 * Performing these transform calculations in the geometry stage lets us perform 
 * geometry calculations on vertex attributes in local-space instead of some 
 * wacky basis, like view-space or clip-space. */
#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
/* NOTE: we don't read SceneProperties UBO, and we don't read any PushConstants, because we're pushing that functionality to the GEOMETRY stage */
layout(location = KORL_VERTEX_INPUT_POSITION) in vec3 attributePosition;
layout(location = KORL_FRAGMENT_INPUT_COLOR) out vec4 fragmentColor;
void main()
{
    fragmentColor = vec4(1);
    gl_Position   = vec4(attributePosition, 1);
}
