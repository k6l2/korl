/** The purpose of this geometry shader is to send out flat triangles containing 
 * encoded barycentric coordinates via KORL_FRAGMENT_INPUT_BARYCENTRIC_POSITION.  
 * For details on the barycentric coordinate encoding, see the definition of 
 * KORL_FRAGMENT_INPUT_BARYCENTRIC_POSITION in `korl.glsl`. */
#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
layout(set     = KORL_DESCRIPTOR_SET_SCENE_TRANSFORMS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_SCENE_TRANSFORMS_UBO_VP
      ,row_major) 
    uniform Korl_UniformBufferObject_SceneProperties
{
    Korl_SceneProperties sceneProperties;
};
layout(push_constant, row_major) uniform Korl_UniformBufferObject_VertexPushConstants
{
    Korl_Vertex_PushConstants pushConstants;
};
layout(triangles)                            in; // take a triangle as input
layout(location = KORL_FRAGMENT_INPUT_COLOR) in vec4 vertexColor[];
layout(triangle_strip, max_vertices = 3)                    out;// output the same triangle
layout(location = KORL_FRAGMENT_INPUT_UV)                   out vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)          out vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION)        out vec3 fragmentViewPosition;
layout(location = KORL_FRAGMENT_INPUT_COLOR)                out vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_BARYCENTRIC_POSITION) out vec2 fragmentBarycentricPosition;
void main()
{
    /* we perform the proper view-space transforms here instead of in the vertex stage; 
        this allows us to modify vertex positions in local-space instead of view-space or clip-space */
    const mat4  modelView           = sceneProperties.view * pushConstants.model;
    const mat4  modelViewProjection = sceneProperties.projection * modelView;
    const mat3  normalMatrix        = transpose(inverse(mat3(modelView)));
    const vec3  triangleNormalLocal = normalize(cross(vec3(gl_in[1].gl_Position) - vec3(gl_in[0].gl_Position)
                                                     ,vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position)));
    const vec3  triangleNormalView  = normalize(normalMatrix * triangleNormalLocal);
    gl_Position                 = modelViewProjection * gl_in[0].gl_Position;
    fragmentUv                  = vec2(0);
    fragmentViewNormal          = triangleNormalView;
    fragmentViewPosition        = vec3(modelView * gl_in[0].gl_Position);
    fragmentColor               = vertexColor[0];
    fragmentBarycentricPosition = vec2(1, 0);
    EmitVertex();
    gl_Position                 = modelViewProjection * gl_in[1].gl_Position;
    fragmentUv                  = vec2(0);
    fragmentViewNormal          = triangleNormalView;
    fragmentViewPosition        = vec3(modelView * gl_in[1].gl_Position);
    fragmentColor               = vertexColor[1];
    fragmentBarycentricPosition = vec2(0, 1);
    EmitVertex();
    gl_Position                 = modelViewProjection * gl_in[2].gl_Position;
    fragmentUv                  = vec2(0);
    fragmentViewNormal          = triangleNormalView;
    fragmentViewPosition        = vec3(modelView * gl_in[2].gl_Position);
    fragmentColor               = vertexColor[2];
    fragmentBarycentricPosition = vec2(0, 0);
    EmitVertex();
    EndPrimitive();
}
