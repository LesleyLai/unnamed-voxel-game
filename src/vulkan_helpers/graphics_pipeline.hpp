#ifndef VOXEL_GAME_VULKAN_GRAPHICS_PIPELINE_HPP
#define VOXEL_GAME_VULKAN_GRAPHICS_PIPELINE_HPP

#include <vulkan/vulkan.h>

#include <span>
#include <string>

#include "expected.hpp"
#include "unique_resource.hpp"

namespace vkh {

class Context;

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
  const char* debug_name = nullptr;
  std::span<const VkPipelineShaderStageCreateInfo> shader_stages;
  PolygonMode polygon_mode = PolygonMode::fill;
  CullMode cull_mode = CullMode::none;
};

[[nodiscard]] auto
create_graphics_pipeline(Context& context,
                         const GraphicsPipelineCreateInfo& create_info)
    -> Expected<VkPipeline>;

struct Pipeline : UniqueResource<VkPipeline, vkDestroyPipeline> {
  using UniqueResource<VkPipeline, vkDestroyPipeline>::UniqueResource;
};

[[nodiscard]] auto
create_graphics_pipeline_unique(Context& context,
                                const GraphicsPipelineCreateInfo& create_info)
    -> Expected<Pipeline>;

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_GRAPHICS_PIPELINE_HPP
