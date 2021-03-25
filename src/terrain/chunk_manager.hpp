#ifndef VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP
#define VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP

#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/context.hpp"

#include <beyond/math/vector.hpp>

#include <span>

struct ChunkVertexCache {
  vkh::Buffer vertex_buffer{};
  std::uint32_t vertex_count = 0;
  beyond::Vec4 transform; // x, y, z for translation, w for scaling
};

class ChunkManager {
  vkh::Context& context_;

  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  VkPipelineLayout meshing_pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline meshing_pipeline_ = VK_NULL_HANDLE;
  VkCommandPool meshing_command_pool_ = VK_NULL_HANDLE;
  VkFence meshing_fence_ = VK_NULL_HANDLE;

  vkh::Buffer edge_table_buffer_;
  vkh::Buffer triangle_table_buffer_;

  vkh::Buffer terrain_vertex_scratch_buffer_;
  vkh::Buffer terrain_reduced_scratch_buffer_;

  std::vector<ChunkVertexCache> vertex_cache_;

public:
  static constexpr int chunk_dimension = 32;

  explicit ChunkManager(vkh::Context& context);
  ~ChunkManager();
  ChunkManager(const ChunkManager&) = delete;
  auto operator=(const ChunkManager&) & -> ChunkManager& = delete;
  ChunkManager(ChunkManager&&) noexcept = delete;
  auto operator=(ChunkManager&&) & noexcept -> ChunkManager& = delete;

  void load_chunk(beyond::IVec3 position);

  [[nodiscard]] auto vertex_cache() -> std::span<ChunkVertexCache>
  {
    return vertex_cache_;
  }

private:
  static auto calculate_chunk_transform(beyond::IVec3 position) -> beyond::Vec4;
  void update_write_descriptor_set();
  void generate_chunk_mesh(beyond::IVec3 position);
  auto get_vertex_count() -> uint32_t;
  auto copy_mesh_from_scratch_buffer(uint32_t vertex_count,
                                     beyond::IVec3 position)
      -> ChunkVertexCache;
};

#endif // VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP
