#version 450

const int chunk_size = 16;
layout (local_size_x = chunk_size, local_size_y = chunk_size) in;

layout(binding = 0) buffer indirect_buffer
{
    uint    vertexCount;
    uint    instanceCount;
    uint    firstVertex;
    uint    firstInstance;
} indirect;

layout(binding = 1) buffer in_buffer
{
    uint indata[];
};

struct Vertex {
    vec4 position;// 4th dimension corrently unused
    vec4 normal;// 4th dimension corrently unused
};

layout(binding = 2) buffer out_buffer
{
    Vertex vertices[];
};

const Vertex[] const_vertices = Vertex[](
Vertex(vec4(-0.5, -0.5, 0.0, 0.0), vec4(-0.5, -0.5, 0.0, 0.0)),
Vertex(vec4(0.5, -0.5, 0.0, 0.0), vec4(-0.5, -0.5, 0.0, 0.0)),
Vertex(vec4(0.0, 0.5, 0.0, 0.0), vec4(-0.5, -0.5, 0.0, 0.0))
);

void main(){
    if (gl_GlobalInvocationID.x < 3 && gl_GlobalInvocationID.y == 0 && gl_GlobalInvocationID.z == 0) {
        vertices[gl_GlobalInvocationID.x] = const_vertices[gl_GlobalInvocationID.x];
    }

    atomicAdd(indirect.vertexCount, 1);
}
