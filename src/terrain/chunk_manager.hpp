#ifndef VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP
#define VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP

#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/context.hpp"

#include <beyond/math/point.hpp>
#include <beyond/math/vector.hpp>

#include <span>
#include <unordered_set>

struct ChunkVertexCache {
  vkh::Buffer vertex_buffer{};
  std::uint32_t vertex_count = 0;
  beyond::Vec4 transform; // x, y, z for translation, w for scaling
  ChunkVertexCache* next = nullptr;
};

struct VertexCachePool {
  static constexpr std::size_t vertex_cache_pool_size = 1000;

  ChunkVertexCache vertex_cache_pool[vertex_cache_pool_size];
  ChunkVertexCache* vertex_cache_pool_first_available = vertex_cache_pool;

  VertexCachePool()
  {
    for (std::size_t i = 0; i < vertex_cache_pool_size - 1; ++i) {
      vertex_cache_pool[i].next = &vertex_cache_pool[i + 1];
    }
    // Next pointer of the last element will be nullptr
  }

  void add(ChunkVertexCache vertex_cache)
  {
    // When we exhaust the pool
    BEYOND_ENSURE(vertex_cache_pool_first_available != nullptr);
    ChunkVertexCache& cache = *vertex_cache_pool_first_available;
    vertex_cache_pool_first_available = cache.next;
    cache = vertex_cache;
  }
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

  std::unordered_set<beyond::IVec3> loaded_chunks_;
  VertexCachePool vertex_caches_;

public:
  static constexpr int chunk_dimension = 32;

  explicit ChunkManager(vkh::Context& context);
  ~ChunkManager();
  ChunkManager(const ChunkManager&) = delete;
  auto operator=(const ChunkManager&) & -> ChunkManager& = delete;
  ChunkManager(ChunkManager&&) noexcept = delete;
  auto operator=(ChunkManager&&) & noexcept -> ChunkManager& = delete;

  void update(beyond::Point3 position);

  [[nodiscard]] auto vertex_caches() -> std::span<ChunkVertexCache>
  {
    return vertex_caches_.vertex_cache_pool;
  }

private:
  void load_chunk(beyond::IVec3 position);

  static auto calculate_chunk_transform(beyond::IVec3 position) -> beyond::Vec4;
  void update_write_descriptor_set();
  void generate_chunk_mesh(beyond::IVec3 position);
  auto get_vertex_count() -> uint32_t;
  auto copy_mesh_from_scratch_buffer(uint32_t vertex_count,
                                     beyond::IVec3 position)
      -> ChunkVertexCache;
};

#endif // VOXEL_GAME_TERRAIN_CHUNK_MANAGER_HPP
