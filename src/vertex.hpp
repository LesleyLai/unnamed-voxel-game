#ifndef VOXEL_GAME_VERTEX_HPP
#define VOXEL_GAME_VERTEX_HPP

#include <vulkan/vulkan.h>

#include <beyond/math/vector.hpp>

#include <array>
#include <cstddef>

struct Vertex {
  beyond::Vec4 position;
  beyond::Vec4 normal;

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
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = offsetof(Vertex, position)},
         {.location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = offsetof(Vertex, normal)}});
  }
};

#endif // VOXEL_GAME_VERTEX_HPP
