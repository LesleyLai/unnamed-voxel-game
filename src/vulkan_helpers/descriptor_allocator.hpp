#ifndef VOXEL_GAME_VULKAN_DESCRIPTOR_ALLOCATOR_HPP
#define VOXEL_GAME_VULKAN_DESCRIPTOR_ALLOCATOR_HPP

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

#include <beyond/types/optional.hpp>

namespace vkh {

class DescriptorAllocator {
  struct PoolSizes {
    std::vector<std::pair<VkDescriptorType, float>> sizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};
  };

  VkDevice device_;
  VkDescriptorPool current_pool_ = VK_NULL_HANDLE;
  PoolSizes descriptor_sizes_;
  std::vector<VkDescriptorPool> used_pools_;
  std::vector<VkDescriptorPool> free_pools_;

public:
  explicit DescriptorAllocator(VkDevice device);
  ~DescriptorAllocator();
  DescriptorAllocator(const DescriptorAllocator&) = delete;
  auto operator=(const DescriptorAllocator&) & -> DescriptorAllocator& = delete;
  DescriptorAllocator(DescriptorAllocator&&) noexcept = delete;
  auto operator=(DescriptorAllocator&&) & noexcept
      -> DescriptorAllocator& = delete;

  void reset_pools();
  [[nodiscard]] auto allocate(VkDescriptorSet* set,
                              VkDescriptorSetLayout layout)
      -> beyond::optional<VkDescriptorSet>;

  [[nodiscard]] auto grab_pool() -> VkDescriptorPool
  {
    return current_pool_;
  }
};

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_DESCRIPTOR_ALLOCATOR_HPP
