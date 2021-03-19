#ifndef VOXEL_GAME_VULKAN_SYNC_HPP
#define VOXEL_GAME_VULKAN_SYNC_HPP

#include <vulkan/vulkan_core.h>

#include "error_handling.hpp"

namespace vkh {

class Context;

struct FenceCreateInfo {
  const char* debug_name = nullptr;
};

struct SemaphoreCreateInfo {
  const char* debug_name = nullptr;
};

[[nodiscard]] auto create_fence(Context& context,
                                const FenceCreateInfo& create_info)
    -> Expected<VkFence>;

[[nodiscard]] auto create_semaphore(Context& context,
                                    const SemaphoreCreateInfo& create_info)
    -> Expected<VkSemaphore>;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_SYNC_HPP
