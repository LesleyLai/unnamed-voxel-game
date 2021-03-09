#ifndef VOXEL_GAME_MARCHING_CUBES_HPP
#define VOXEL_GAME_MARCHING_CUBES_HPP

#include <vulkan/vulkan.h>

#include <beyond/math/point.hpp>
#include <beyond/math/vector.hpp>

#include <vector>

struct Vertex {
  beyond::Point3 position;
  beyond::Vec3 normal;

  [[nodiscard]] static constexpr auto binding_description()
  {
    return VkVertexInputBindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
  }

  [[nodiscard]] static constexpr auto attributes_descriptions()
  {
    return std::to_array<VkVertexInputAttributeDescription>(
        {{.location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, position)},
         {.location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, normal)}});
  }
};

[[nodiscard]] auto generate_chunk_mesh() -> std::vector<Vertex>;

#endif // VOXEL_GAME_MARCHING_CUBES_HPP
