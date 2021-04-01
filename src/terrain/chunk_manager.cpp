#include "chunk_manager.hpp"
#include "marching_cube_tables.hpp"

#include "../vertex.hpp"
#include "../vulkan_helpers/commands.hpp"
#include "../vulkan_helpers/debug_utils.hpp"
#include "../vulkan_helpers/descriptor_pool.hpp"
#include "../vulkan_helpers/shader_module.hpp"
#include "../vulkan_helpers/sync.hpp"

#include <beyond/coroutine/generator.hpp>
#include <beyond/math/serial.hpp>
#include <beyond/utils/size.hpp>
#include <beyond/utils/to_pointer.hpp>

#include <imgui.h>

namespace {

struct TerrainReducedBuffer {
  uint32_t vertex_count = 0;
};

} // anonymous namespace

ChunkManager::ChunkManager(vkh::Context& context)
    : context_{context},
      edge_table_buffer_{generate_edge_table_buffer(context).value()},
      triangle_table_buffer_{generate_triangle_table_buffer(context).value()},
      vertex_caches_(context.allocator())
{
  const VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}};

  descriptor_pool_ =
      vkh::create_descriptor_pool(
          context_, {.max_sets = 1,
                     .pool_sizes = pool_sizes,
                     .debug_name = "Terrain Chunk Descriptor Pool"})
          .value();

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

  const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptor_pool_,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptor_set_layout_,
  };
  descriptor_set = {};
  VK_CHECK(vkAllocateDescriptorSets(
      context_.device(), &descriptor_set_allocate_info, &descriptor_set));

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

  constexpr size_t max_triangles_per_cell = 5;
  constexpr size_t vertices_per_triangle = 3;
  constexpr size_t max_vertex_count = max_triangles_per_cell *
                                      vertices_per_triangle * chunk_dimension *
                                      chunk_dimension * chunk_dimension;
  constexpr size_t vertex_buffer_size = sizeof(Vertex) * max_vertex_count;

  terrain_reduced_scratch_buffer_ =
      vkh::create_buffer_from_data(
          context_,
          {.size = sizeof(std::uint32_t),
           .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
           .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
           .debug_name = "Terrain Reduced Scratch Buffer"},
          TerrainReducedBuffer{})
          .value();

  terrain_vertex_scratch_buffer_ =
      vkh::create_buffer(context_,
                         {.size = vertex_buffer_size,
                          .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
                          .debug_name = "Terrain Vertex Scratch Buffer"})
          .value();
}

ChunkManager::~ChunkManager()
{
  vkDestroyFence(context_.device(), meshing_fence_, nullptr);
  vkDestroyCommandPool(context_.device(), meshing_command_pool_, nullptr);
  vkDestroyPipeline(context_.device(), meshing_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_.device(), meshing_pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(context_.device(), descriptor_set_layout_,
                               nullptr);
  vkDestroyDescriptorPool(context_.device(), descriptor_pool_, nullptr);

  for (auto cache : vertex_caches_.vertex_cache_pool) {
    if (cache.vertex_count == 0) { continue; }
    vkh::destroy_buffer(context_, cache.vertex_buffer);
  }

  vkh::destroy_buffer(context_, terrain_reduced_scratch_buffer_);
  vkh::destroy_buffer(context_, terrain_vertex_scratch_buffer_);
  vkh::destroy_buffer(context_, triangle_table_buffer_);
  vkh::destroy_buffer(context_, edge_table_buffer_);
}

[[nodiscard]] auto
ChunkManager::calculate_chunk_transform(beyond::IVec3 position) -> beyond::Vec4
{
  const auto chunk_x = static_cast<float>(chunk_dimension * position.x);
  const auto chunk_y = static_cast<float>(chunk_dimension * position.y);
  const auto chunk_z = static_cast<float>(chunk_dimension * position.z);

  return beyond::Vec4{chunk_x, chunk_y, chunk_z, 1.f};
}

void ChunkManager::update_write_descriptor_set()
{
  const VkDescriptorBufferInfo indirect_descriptor_buffer_info = {
      terrain_reduced_scratch_buffer_, 0, VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo out_descriptor_buffer_info = {
      terrain_vertex_scratch_buffer_, 0, VK_WHOLE_SIZE};
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
}

void ChunkManager::generate_chunk_mesh(beyond::IVec3 position)
{
  const beyond::Vec4 transform = calculate_chunk_transform(position);

  VkCommandBuffer meshing_command_buffer =
      vkh::allocate_command_buffer(
          context_,
          {.command_pool = meshing_command_pool_,
           .debug_name =
               fmt::format("Meshing command buffer at {}", position).c_str()})
          .value();

  static constexpr VkCommandBufferBeginInfo command_buffer_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_CHECK(
      vkBeginCommandBuffer(meshing_command_buffer, &command_buffer_begin_info));

  vkCmdBindPipeline(meshing_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    meshing_pipeline_);
  vkCmdBindDescriptorSets(
      meshing_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      meshing_pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
  vkCmdPushConstants(meshing_command_buffer, meshing_pipeline_layout_,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(beyond::Vec4),
                     &transform);

  constexpr std::uint32_t local_size = 4;
  constexpr auto dispatch_size = chunk_dimension / local_size;
  vkCmdDispatch(meshing_command_buffer, dispatch_size, dispatch_size,
                dispatch_size);
  VK_CHECK(vkEndCommandBuffer(meshing_command_buffer));

  const VkSubmitInfo meshing_submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &meshing_command_buffer,
  };
  VK_CHECK(vkQueueSubmit(context_.compute_queue(), 1, &meshing_submit_info,
                         meshing_fence_));

  vkWaitForFences(context_.device(), 1, &meshing_fence_, true, 1e9);
  vkResetFences(context_.device(), 1, &meshing_fence_);
}

[[nodiscard]] auto ChunkManager::copy_mesh_from_scratch_buffer(
    uint32_t vertex_count, beyond::IVec3 position) -> ChunkVertexCache
{
  const beyond::Vec4 transform = calculate_chunk_transform(position);

  const std::uint32_t vertex_buffer_size = vertex_count * sizeof(Vertex);
  vkh::Buffer vertex_buffer =
      vkh::create_buffer(
          context_,
          {.size = vertex_buffer_size,
           .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
           .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
           .debug_name = fmt::format("Terrain chunk at {}", position).c_str()})
          .value();

  VkCommandBuffer transfer_command_buffer =
      vkh::allocate_command_buffer(
          context_,
          {.command_pool = meshing_command_pool_,
           .debug_name =
               fmt::format("Transfer command buffer at {}", position).c_str()})
          .value();

  static constexpr VkCommandBufferBeginInfo command_buffer_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_CHECK(vkBeginCommandBuffer(transfer_command_buffer,
                                &command_buffer_begin_info));

  const VkBufferCopy buffer_copy{
      .srcOffset = 0,
      .dstOffset = 0,
      .size = vertex_buffer_size,
  };
  vkCmdCopyBuffer(transfer_command_buffer, terrain_vertex_scratch_buffer_,
                  vertex_buffer, 1, &buffer_copy);
  VK_CHECK(vkEndCommandBuffer(transfer_command_buffer));

  const VkSubmitInfo transfer_submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &transfer_command_buffer,
  };
  VK_CHECK(vkQueueSubmit(context_.compute_queue(), 1, &transfer_submit_info,
                         meshing_fence_));

  vkWaitForFences(context_.device(), 1, &meshing_fence_, true, 1e9);
  vkResetFences(context_.device(), 1, &meshing_fence_);

  return ChunkVertexCache{
      .vertex_buffer = vertex_buffer,
      .vertex_count = vertex_count,
      .transform = transform,
  };
}

[[nodiscard]] auto ChunkManager::get_vertex_count() -> uint32_t
{
  auto* indirect_buffer_data =
      context_.map<TerrainReducedBuffer>(terrain_reduced_scratch_buffer_)
          .value();
  std::uint32_t count = indirect_buffer_data->vertex_count;
  indirect_buffer_data->vertex_count = 0;
  context_.unmap(terrain_reduced_scratch_buffer_);
  return count;
}

using ChunkMap = std::unordered_map<beyond::IVec3, ChunkVertexCache*>;

auto chunks_to_load(const ChunkMap& loaded_chunks, beyond::IVec3 center)
    -> beyond::Generator<beyond::IVec3>
{
  for (int radius = 0; radius < 5; ++radius) {
    for (int x = -radius; x <= radius; ++x) {
      for (int y = -radius; y <= radius; ++y) {
        for (int z = -radius; z <= radius; ++z) {
          if (std::abs(x) < radius && std::abs(y) < radius &&
              std::abs(z) < radius) {
            continue;
          }

          const auto chunk_coord = beyond::IVec3{x, y, z} + center;
          if (!loaded_chunks.contains(chunk_coord)) { co_yield chunk_coord; }
        }
      }
    }
  }
}

void ChunkManager::update(beyond::Point3 position)
{
  if (!generating_terrain_) { return; }

  const int x =
      (static_cast<int>(position.x) + chunk_dimension / 2) / chunk_dimension;
  const int y =
      (static_cast<int>(position.y) + chunk_dimension / 2) / chunk_dimension;
  const int z =
      (static_cast<int>(position.z) + chunk_dimension / 2) / chunk_dimension;

  for (beyond::IVec3 chunk_coord :
       chunks_to_load(loaded_chunks_, beyond::IVec3{x, y, z})) {
    loaded_chunks_.emplace(chunk_coord, load_chunk(chunk_coord));
  }

  //  for (auto [chunk_coord, vertex_cache_ptr] : loaded_chunks_) {
  //    if (chunk_coord.x < -5 + x || chunk_coord.x > 5 + x ||
  //        chunk_coord.y < -5 + y || chunk_coord.y > 5 + y ||
  //        chunk_coord.z < -5 + z || chunk_coord.z > 5 + z) {
  //      if (vertex_cache_ptr != nullptr) {
  //        vertex_caches_.remove(*vertex_cache_ptr);
  //      }
  //    }
  //  }
}

[[nodiscard]] auto ChunkManager::load_chunk(beyond::IVec3 position)
    -> ChunkVertexCache*
{
  update_write_descriptor_set();

  generate_chunk_mesh(position);

  const uint32_t vertex_count = get_vertex_count();
  const bool is_empty_chunk = vertex_count == 0;

  if (is_empty_chunk) { return nullptr; }

  ChunkVertexCache vertex_cache =
      copy_mesh_from_scratch_buffer(vertex_count, position);
  vkResetCommandPool(context_.device(), meshing_command_pool_, 0);
  return &vertex_caches_.add(vertex_cache);
}

void ChunkManager::draw_gui()
{
  ImGui::Text("Terrain Generation");
  ImGui::Checkbox("Generating", &generating_terrain_);
}