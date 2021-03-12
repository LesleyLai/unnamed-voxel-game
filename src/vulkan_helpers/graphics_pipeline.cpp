#include "graphics_pipeline.hpp"

#include <beyond/utils/size.hpp>
#include <beyond/utils/to_pointer.hpp>

#include "../marching_cubes.hpp"

namespace vkh {

[[nodiscard]] auto
create_graphics_pipeline(VkDevice device,
                         const GraphicsPipelineCreateInfo& create_info)
    -> beyond::expected<VkPipeline, VkResult>
{

  static constexpr auto vertex_binding_description =
      Vertex::binding_description();
  static constexpr auto vertex_attribute_descriptions =
      Vertex::attributes_descriptions();

  static constexpr VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertex_binding_description,
      .vertexAttributeDescriptionCount =
          static_cast<std::uint32_t>(vertex_attribute_descriptions.size()),
      .pVertexAttributeDescriptions = vertex_attribute_descriptions.data()};

  static constexpr VkPipelineInputAssemblyStateCreateInfo input_assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  const VkExtent2D window_extend = create_info.window_extend;

  const VkViewport viewport{
      .x = 0.0f,
      .y = static_cast<float>(create_info.window_extend.height),
      .width = static_cast<float>(window_extend.width),
      .height = -static_cast<float>(window_extend.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  const VkRect2D scissor{.offset = {0, 0}, .extent = window_extend};

  const VkPipelineViewportStateCreateInfo viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  const VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = static_cast<VkPolygonMode>(create_info.polygon_mode),
      .cullMode = static_cast<VkCullModeFlags>(create_info.cull_mode),
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  static constexpr VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  static constexpr VkPipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  static constexpr VkPipelineColorBlendStateCreateInfo color_blending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  const VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .minDepthBounds = 0.0f, // Optional
      .maxDepthBounds = 1.0f, // Optional
  };

  const VkGraphicsPipelineCreateInfo pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount =
          static_cast<std::uint32_t>(create_info.shader_stages.size()),
      .pStages = create_info.shader_stages.data(),
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depth_stencil_state,
      .pColorBlendState = &color_blending,
      .layout = create_info.pipeline_layout,
      .renderPass = create_info.render_pass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };

  VkPipeline pipeline{};
  const auto result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline);

  if (result == VK_SUCCESS) {
    return pipeline;
  } else {
    return beyond::unexpected{result};
  }
}

} // namespace vkh