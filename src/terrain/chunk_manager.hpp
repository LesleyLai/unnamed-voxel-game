#ifndef VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP
#define VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP

#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/context.hpp"

#include <span>

struct ChunkVertexCache {
  vkh::Buffer vertex_buffer{};
  vkh::Buffer indirect_buffer{};
};

class ChunkManager {
  vkh::Context& context_;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;

  vkh::Buffer edge_table_buffer_;
  vkh::Buffer triangle_table_buffer_;

  std::vector<ChunkVertexCache> vertex_cache_;

  VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
  VkPipelineLayout meshing_pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline meshing_pipeline_ = VK_NULL_HANDLE;
  VkCommandPool meshing_command_pool_ = VK_NULL_HANDLE;
  VkFence meshing_fence_ = VK_NULL_HANDLE;

public:
  static constexpr int chunk_dimension = 32;

  explicit ChunkManager(vkh::Context& context,
                        VkDescriptorPool descriptor_pool);
  ~ChunkManager();
  ChunkManager(const ChunkManager&) = delete;
  auto operator=(const ChunkManager&) & -> ChunkManager& = delete;
  ChunkManager(ChunkManager&&) noexcept = delete;
  auto operator=(ChunkManager&&) & noexcept -> ChunkManager& = delete;

  void load_chunk();

  [[nodiscard]] auto vertex_cache() -> std::span<ChunkVertexCache>
  {
    return vertex_cache_;
  }
};

#endif // VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP
