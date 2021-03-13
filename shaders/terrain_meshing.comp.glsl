#version 450

const int size = 8;
layout(local_size_x_id = 0, local_size_y_id = 1) in;

layout(binding = 0) buffer in_buffer
{
    uint indata[];
};

layout(binding = 1) buffer out_buffer
{
    uint outdata[];
};

void main(){
    for (int i = 0; i < size; ++i) {
        outdata[i] = indata[i];
    }
}