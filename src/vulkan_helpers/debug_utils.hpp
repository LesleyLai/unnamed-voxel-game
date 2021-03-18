#ifndef VOXEL_GAME_VULKAN_DEBUG_UTILS_HPP
#define VOXEL_GAME_VULKAN_DEBUG_UTILS_HPP

#include <vulkan/vulkan_core.h>

namespace vkh {

class Context;

[[nodiscard]] auto set_debug_name(Context& context, uint64_t object_handle,
                                  VkObjectType object_type,
                                  const char* name) noexcept -> VkResult;

void report_fail_to_set_debug_name(const char* name) noexcept;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_DEBUG_UTILS_HPP
