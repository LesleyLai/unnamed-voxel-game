#ifndef VOXEL_GAME_VULKAN_SHADER_MODULE_HPP
#define VOXEL_GAME_VULKAN_SHADER_MODULE_HPP

#include <beyond/types/expected.hpp>

#include <string_view>
#include <vulkan/vulkan.h>

namespace vkh {

[[nodiscard]] auto create_shader_module(VkDevice device,
                                        const std::string_view filename)
    -> beyond::expected<VkShaderModule, VkResult>;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_SHADER_MODULE_HPP
