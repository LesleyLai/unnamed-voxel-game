#ifndef VOXEL_GAME_VULKAN_ERROR_HANDLING_HPP
#define VOXEL_GAME_VULKAN_ERROR_HANDLING_HPP

#include <beyond/types/expected.hpp>

#include <vulkan/vulkan_core.h>

#include <fmt/format.h>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) { fmt::format("Vulkan error: {}\n", err); }                       \
  } while (0)

#define VKH_TRY(expr)                                                          \
  if (VkResult result = (expr); result != VK_SUCCESS) {                        \
    return beyond::make_unexpected(result);                                    \
  }

namespace vkh {

template <class T> using Expected = beyond::expected<T, VkResult>;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_ERROR_HANDLING_HPP
