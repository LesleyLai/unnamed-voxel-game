#ifndef VOXEL_GAME_VULKAN_DESCRIPTOR_POOL_HPP
#define VOXEL_GAME_VULKAN_DESCRIPTOR_POOL_HPP

#include "vulkan/vulkan_core.h"

#include "error_handling.hpp"

#include <span>

namespace vkh {

struct Context;

struct PoolSize {
  VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  float multiplier = 0;
};

using PoolSizes = std::span<const PoolSize>;

namespace detail {

static constexpr PoolSize default_pool_sizes_array[] = {
    {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
    {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
    {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
    {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};

} // namespace detail

[[nodiscard]] constexpr auto default_pool_sizes() -> std::span<const PoolSize>
{
  return detail::default_pool_sizes_array;
}

struct DescriptorPoolCreateInfo {
  PoolSizes pool_sizes = default_pool_sizes();
  // The count for each descriptor type is multiplied by the multiplier in the
  // pool_sizes
  std::uint32_t count = 10;
  VkDescriptorPoolCreateFlags flags = 0;
  const char* debug_name = nullptr;
};

auto create_descriptor_pool(Context& context,
                            const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_DESCRIPTOR_POOL_HPP
