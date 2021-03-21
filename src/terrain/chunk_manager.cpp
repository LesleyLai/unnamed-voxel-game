#include "chunk_manager.hpp"
#include "marching_cube_tables.hpp"

#include "../vertex.hpp"
#include "../vulkan_helpers/debug_utils.hpp"
#include "../vulkan_helpers/shader_module.hpp"
#include "../vulkan_helpers/sync.hpp"

#include <beyond/math/serial.hpp>
#include <beyond/utils/size.hpp>
#include <beyond/utils/to_pointer.hpp>

ChunkManager::ChunkManager(vkh::Context& context,
                           VkDescriptorPool descriptor_pool)
    : context_{context}, descriptor_pool_{descriptor_pool},
      edge_table_buffer_{generate_edge_table_buffer(context).value()},
      triangle_table_buffer_{generate_triangle_table_buffer(context).value()}
{
  static constexpr VkDescriptorSetLayoutBinding
      descriptor_set_layout_bindings[] = {
          {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr}};

  static constexpr VkDescriptorSetLayoutCreateInfo
      descriptor_set_layout_create_info = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .bindingCount = beyond::size(descriptor_set_layout_bindings),
          .pBindings = beyond::to_pointer(descriptor_set_layout_bindings)};

  vkCreateDescriptorSetLayout(context_.device(),
                              &descriptor_set_layout_create_info, nullptr,
                              &descriptor_set_layout_);

  const VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .offset = 0,
      .size = sizeof(beyond::Vec4),
  };

  const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout_,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant_range,
  };
  vkCreatePipelineLayout(context_.device(), &pipeline_layout_create_info,
                         nullptr, &meshing_pipeline_layout_);

  VkShaderModule meshing_shader_module =
      vkh::load_shader_module_from_file(
          context_, "shaders/terrain_meshing.comp.spv",
          {.debug_name = "Terrain Meshing Compute Shader"})
          .expect("Cannot load terrain_meshing.comp.spv");

  const VkComputePipelineCreateInfo meshing_pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage =
          VkPipelineShaderStageCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = meshing_shader_module,
              .pName = "main",
          },
      .layout = meshing_pipeline_layout_,
  };
  VK_CHECK(vkCreateComputePipelines(context_.device(), {}, 1,
                                    &meshing_pipeline_create_info, nullptr,
                                    &meshing_pipeline_));
  VK_CHECK(vkh::set_debug_name(
      context_, beyond::bit_cast<uint64_t>(meshing_pipeline_),
      VK_OBJECT_TYPE_PIPELINE, "Terrian Meshing Pipeline"));

  vkDestroyShaderModule(context_.device(), meshing_shader_module, nullptr);

  const VkCommandPoolCreateInfo compute_command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = context_.compute_queue_family_index()};
  VK_CHECK(vkCreateCommandPool(context_.device(),
                               &compute_command_pool_create_info, nullptr,
                               &meshing_command_pool_));
  meshing_fence_ =
      vkh::create_fence(context_, {.debug_name = "Meshing Fence"}).value();
}

ChunkManager::~ChunkManager()
{
  vkDestroyFence(context_.device(), meshing_fence_, nullptr);
  vkDestroyCommandPool(context_.device(), meshing_command_pool_, nullptr);
  vkDestroyPipeline(context_.device(), meshing_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_.device(), meshing_pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(context_.device(), descriptor_set_layout_,
                               nullptr);

  for (auto cache : vertex_cache_) {
    vkh::destroy_buffer(context_, cache.vertex_buffer);
    vkh::destroy_buffer(context_, cache.indirect_buffer);
  }

  vkh::destroy_buffer(context_, triangle_table_buffer_);
  vkh::destroy_buffer(context_, edge_table_buffer_);
}

void ChunkManager::load_chunk(beyond::IVec3 position)
{
  static constexpr VkDrawIndirectCommand indirect_command{
      .vertexCount = 0,
      .instanceCount = 1,
      .firstVertex = 0,
      .firstInstance = 0,
  };
  vkh::Buffer indirect_buffer =
      vkh::create_buffer_from_data(
          context_,
          {.size = sizeof(VkDrawIndirectCommand),
           .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
           .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
           .debug_name =
               fmt::format("Chunk Indirect Buffer {}", position).c_str()},
          indirect_command)
          .value();

  constexpr size_t max_triangles_per_cell = 5;
  constexpr size_t vertices_per_triangle = 3;
  constexpr size_t vertex_buffer_size =
      sizeof(Vertex) * max_triangles_per_cell * vertices_per_triangle *
      chunk_dimension * chunk_dimension * chunk_dimension;

  vkh::Buffer vertex_buffer =
      vkh::create_buffer(
          context_,
          {.size = vertex_buffer_size,
           .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
           .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
           .debug_name =
               fmt::format("Chunk Vertex Buffer {}", position).c_str()})
          .value();

  const auto chunk_x = static_cast<float>(chunk_dimension * position.x);
  const auto chunk_y = static_cast<float>(chunk_dimension * position.y);
  const auto chunk_z = static_cast<float>(chunk_dimension * position.z);
  beyond::Vec4 transform{chunk_x, chunk_y, chunk_z, 1.f};

  const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptor_pool_,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptor_set_layout_,
  };
  VkDescriptorSet descriptor_set = {};
  VK_CHECK(vkAllocateDescriptorSets(
      context_.device(), &descriptor_set_allocate_info, &descriptor_set));

  const VkDescriptorBufferInfo indirect_descriptor_buffer_info = {
      indirect_buffer, 0, VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo out_descriptor_buffer_info = {vertex_buffer, 0,
                                                             VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo edge_table_descriptor_buffer_info = {
      edge_table_buffer_, 0, VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo tri_table_descriptor_buffer_info = {
      triangle_table_buffer_, 0, VK_WHOLE_SIZE};

  const VkWriteDescriptorSet write_descriptor_set[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
       &indirect_descriptor_buffer_info, nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 1, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &out_descriptor_buffer_info,
       nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
       &edge_table_descriptor_buffer_info, nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 3, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
       &tri_table_descriptor_buffer_info, nullptr}};
  vkUpdateDescriptorSets(context_.device(), beyond::size(write_descriptor_set),
                         beyond::to_pointer(write_descriptor_set), 0, nullptr);

  const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
      meshing_command_pool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

  VkCommandBuffer command_buffer{};
  VK_CHECK(vkAllocateCommandBuffers(
      context_.device(), &command_buffer_allocate_info, &command_buffer));

  static constexpr VkCommandBufferBeginInfo command_buffer_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    meshing_pipeline_);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          meshing_pipeline_layout_, 0, 1, &descriptor_set, 0,
                          nullptr);
  vkCmdPushConstants(command_buffer, meshing_pipeline_layout_,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(beyond::Vec4),
                     &transform);

  constexpr std::uint32_t local_size = 4;
  constexpr auto dispatch_size = chunk_dimension / local_size;
  vkCmdDispatch(command_buffer, dispatch_size, dispatch_size, dispatch_size);
  VK_CHECK(vkEndCommandBuffer(command_buffer));

  const VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
  };

  VK_CHECK(
      vkQueueSubmit(context_.compute_queue(), 1, &submit_info, meshing_fence_));

  vkWaitForFences(context_.device(), 1, &meshing_fence_, true, 1e9);
  vkResetFences(context_.device(), 1, &meshing_fence_);
  vkResetCommandPool(context_.device(), meshing_command_pool_, 0);

  auto* indirect_buffer_data =
      context_.map<VkDrawIndirectCommand>(indirect_buffer).value();
  bool is_empty_chunk = indirect_buffer_data->vertexCount == 0;
  context_.unmap(indirect_buffer);

  if (is_empty_chunk) {
    vkh::destroy_buffer(context_, vertex_buffer);
    vkh::destroy_buffer(context_, indirect_buffer);
  } else {
    vertex_cache_.emplace_back(vertex_buffer, indirect_buffer, transform);
  }
}