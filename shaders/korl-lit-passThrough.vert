/** The point of this shader is to just send vertex attributes to the geometry stage.  
 * Performing these transform calculations in the geometry stage lets us perform 
 * geometry calculations on vertex attributes in local-space instead of some 
 * wacky basis, like view-space or clip-space. */
#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
/* NOTE: we don't read SceneProperties UBO, and we don't read any PushConstants, because we're pushing that functionality to the GEOMETRY stage */
layout(location = KORL_VERTEX_INPUT_POSITION) in vec3 attributePosition;
layout(location = KORL_VERTEX_INPUT_UV)       in vec2 attributeUv;
layout(location = KORL_VERTEX_INPUT_NORMAL)   in vec3 attributeNormal;
layout(location = KORL_FRAGMENT_INPUT_COLOR)         out vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_UV)            out vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)   out vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION) out vec3 fragmentViewPosition;
void main()
{
    fragmentColor        = vec4(1);
    fragmentUv           = attributeUv;
    fragmentViewNormal   = attributeNormal;
    fragmentViewPosition = attributePosition;// NOTE: redundant/meaningless
    gl_Position          = vec4(attributePosition, 1);
}
