#ifndef VOXEL_GAME_VULKAN_DESCRIPTOR_POOL_HPP
#define VOXEL_GAME_VULKAN_DESCRIPTOR_POOL_HPP

#include "vulkan/vulkan_core.h"

#include "error_handling.hpp"

#include <span>

namespace vkh {

class Context;

struct DescriptorPoolCreateInfo {
  VkDescriptorPoolCreateFlags flags = 0;
  std::uint32_t max_sets = 0;
  std::span<const VkDescriptorPoolSize> pool_sizes;
  const char* debug_name = nullptr;
};

auto create_descriptor_pool(Context& context,
                            const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_DESCRIPTOR_POOL_HPP
