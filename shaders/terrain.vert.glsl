#version 450

layout (location = 0) in vec4 vPosition;
layout (location = 1) in vec4 vNormal;

layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
} cameraData;

layout (location = 0) out VS_OUT {
    vec3 position;
    vec3 normal;
} vs_out;

void main()
{
    gl_Position = cameraData.viewproj * vec4(vPosition.xyz, 1.0f);
    vs_out.position = vPosition.xyz;
    vs_out.normal = vNormal.xyz;
}