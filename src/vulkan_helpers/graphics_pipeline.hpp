#ifndef VOXEL_GAME_VULKAN_GRAPHICS_PIPELINE_HPP
#define VOXEL_GAME_VULKAN_GRAPHICS_PIPELINE_HPP

#include <beyond/types/expected.hpp>

#include <vulkan/vulkan.h>

#include <span>
#include <string>

namespace vkh {

class Context;

template <class T> using Expected = beyond::expected<T, VkResult>;

enum class PolygonMode {
  fill = VK_POLYGON_MODE_FILL,
  line = VK_POLYGON_MODE_LINE,
  point = VK_POLYGON_MODE_POINT,
};

enum class CullMode {
  none = VK_CULL_MODE_NONE,
  front = VK_CULL_MODE_FRONT_BIT,
  back = VK_CULL_MODE_BACK_BIT,
  front_and_back = VK_CULL_MODE_FRONT_AND_BACK,
};

struct GraphicsPipelineCreateInfo {
  // Required
  VkPipelineLayout pipeline_layout = {};
  VkRenderPass render_pass = {};
  VkExtent2D window_extend = {};

  // Optional
  std::span<const VkPipelineShaderStageCreateInfo> shader_stages;
  PolygonMode polygon_mode = PolygonMode::fill;
  CullMode cull_mode = CullMode::none;
};

[[nodiscard]] auto
create_graphics_pipeline(VkDevice device,
                         const GraphicsPipelineCreateInfo& create_info)
    -> Expected<VkPipeline>;

[[nodiscard]] auto set_debug_name(Context& context, VkPipeline pipeline,
                                  const char* name) noexcept -> VkResult;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_GRAPHICS_PIPELINE_HPP
