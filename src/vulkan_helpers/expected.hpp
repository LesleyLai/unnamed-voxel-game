#ifndef VOXEL_GAME_VULKAN_EXPECTED_HPP
#define VOXEL_GAME_VULKAN_EXPECTED_HPP

#include <beyond/types/expected.hpp>

#include <vulkan/vulkan_core.h>

namespace vkh {

template <class T> using Expected = beyond::expected<T, VkResult>;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_EXPECTED_HPP
