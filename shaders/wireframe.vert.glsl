#version 450

layout (location = 0) in vec4 vPosition;
layout (location = 1) in vec4 vNormal;

layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
} cameraData;

layout( push_constant ) uniform constants
{
    vec4 transform;
} PushConstants;

void main()
{
    vec3 world_position = PushConstants.transform.xyz + vPosition.xyz;
    gl_Position = cameraData.viewproj * vec4(world_position, 1.0f);
}