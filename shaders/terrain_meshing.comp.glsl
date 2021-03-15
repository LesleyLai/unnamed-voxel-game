#version 450

const int size = 8;
layout(local_size_x_id = 0, local_size_y_id = 1) in;

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

layout(binding = 2) buffer out_buffer
{
    uint outdata[];
};

void main(){
    indirect.vertexCount = 312;
    indirect.instanceCount = 1;
    indirect.firstVertex = 0;
    indirect.firstInstance = 0;

    for (int i = 0; i < size; ++i) {
        outdata[i] = indata[i];
    }
}