#version 450
#extension GL_EXT_debug_printf : require
#extension GL_EXT_scalar_block_layout : require

const int chunk_dimension = 32;
const int half_chunk_dimension = chunk_dimension / 2;
layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout( push_constant ) uniform constants
{
  vec4 transform;
} PushConstants;

layout(binding = 0) buffer indirect_buffer
{
  uint    vertexCount;
  uint    instanceCount;
  uint    firstVertex;
  uint    firstInstance;
} indirect;

struct Vertex {
  vec4 position;// 4th dimension corrently unused
  vec4 normal;// 4th dimension corrently unused
};

layout(binding = 1) buffer out_buffer
{
  Vertex vertices[];
};

layout(binding = 2, scalar) buffer edge_table_buffer
{
  uint edge_table[256];
};

layout(binding = 3, scalar) buffer tri_table_buffer
{
  int tri_table[][16];
};

struct Triangle {
  vec3 p[3];
};

struct GridCell {
  vec3 p[8];
  float val[8];
};

const vec3 corner_offsets[8] =
  vec3[8](
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, -0.5f),
    vec3(0.5f, 0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f)
  );

float hash(float p) { p = fract(p * 0.011); p *= p + 7.5; p *= p + p; return fract(p); }

float perlin(vec3 x) {
  const vec3 step = vec3(110, 241, 171);

  vec3 i = floor(x);
  vec3 f = fract(x);

  float n = dot(i, step);

  vec3 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
  mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
  mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
  mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

// Fractal Brownian Motion
float fbm6(vec3 x) {
  const int noise_octaves_count = 9;
  float v = 0.0;
  float a = 0.5;
  vec3 shift = vec3(100);
  for (int i = 0; i < noise_octaves_count; ++i) {
    v += a * perlin(x);
    x = x * 2.0 + shift;
    a *= 0.5;
  }
  return v;
}

float noise(vec3 pt) {
  return fbm6(pt / 20.) - 0.5 - pt.y * 0.01;
}

float inverse_lerp(float a, float b, float v) {
  return (v - a) / (b - a);
}

vec3 vertex_interp(float isolevel, vec3 p1, vec3 p2, float valp1, float valp2) {
  if (abs(isolevel - valp1) < 0.00001f) { return p1; }
  if (abs(isolevel - valp2) < 0.00001f) { return p2; }
  if (abs(valp1 - valp2) < 0.00001f) { return p1; }
  return mix(p1, p2, inverse_lerp(valp1, valp2, isolevel));
}

const uvec2 cornor_table[12] = uvec2[](
  uvec2(0, 1),
  uvec2(1, 2),
  uvec2(2, 3),
  uvec2(3, 0),
  uvec2(4, 5),
  uvec2(5, 6),
  uvec2(6, 7),
  uvec2(7, 4),
  uvec2(0, 4),
  uvec2(1, 5),
  uvec2(2, 6),
  uvec2(3, 7)
);

void polygonize(GridCell cell, float isolevel) {
  uint cubeindex = 0;
  for (uint i = 0; i < 8; ++i) {
    if (cell.val[i] < isolevel) {
      cubeindex |= 1u << i;
    }
  }

  uint edge_set = edge_table[cubeindex];

  /* Cube is entirely in/out of the surface */
  if (edge_set == 0) return;

  /* Find the vertices where the surface intersects the cube */
  vec3 vert_list[12];
  for (int i = 0; i < 12; ++i) {
    if ((edge_set & (1u << i)) != 0u) {
      uvec2 corners = cornor_table[i];
      vert_list[i] = vertex_interp(isolevel, cell.p[corners.x], cell.p[corners.y], cell.val[corners.x], cell.val[corners.y]);
    }
  }

  Triangle triangles[5];
  uint triangle_count = 0;
  for (uint i = 0; tri_table[cubeindex][i] != -1; i += 3) {
    uint vert_index = triangle_count * 3;
    triangles[triangle_count].p =
      vec3[](
             vec3(vert_list[tri_table[cubeindex][vert_index]]),
             vec3(vert_list[tri_table[cubeindex][vert_index + 1]]),
             vec3(vert_list[tri_table[cubeindex][vert_index + 2]])
             );
    ++triangle_count;
  }

  uint vertex_index = atomicAdd(indirect.vertexCount, triangle_count * 3);
  for (uint i = 0; i < triangle_count; ++i) {
    vec3 p0 = triangles[i].p[0];
    vec3 p1 = triangles[i].p[1];
    vec3 p2 = triangles[i].p[2];

    vec3 edge0 = p1 - p0;
    vec3 edge1 = p2 - p1;

    vec3 normal = normalize(cross(edge0, edge1));

    vertices[vertex_index + i * 3]     = Vertex(vec4(p0, 1), vec4(normal, 0));
    vertices[vertex_index + i * 3 + 1] = Vertex(vec4(p1, 1), vec4(normal, 0));
    vertices[vertex_index + i * 3 + 2] = Vertex(vec4(p2, 1), vec4(normal, 0));
  }
}

void main(){
  float fx = float(int(gl_GlobalInvocationID.x) - half_chunk_dimension) + 0.5f;
  float fy = float(int(gl_GlobalInvocationID.y) - half_chunk_dimension) + 0.5f;
  float fz = float(int(gl_GlobalInvocationID.z) - half_chunk_dimension) + 0.5f;
  vec3 voxel_center = vec3(fx, fy, fz);

  GridCell cell;
  for (int i = 0; i < 8; ++i) {
    vec3 corner_point = voxel_center + corner_offsets[i];
    cell.p[i] = corner_point;
    cell.val[i] = noise(corner_point + PushConstants.transform.xyz);
  }

  polygonize(cell, 0);
}
