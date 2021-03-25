#ifndef VOXEL_GAME_VULKAN_DESCRIPTOR_ALLOCATOR_HPP
#define VOXEL_GAME_VULKAN_DESCRIPTOR_ALLOCATOR_HPP

#include <span>
#include <vector>
#include <vulkan/vulkan.h>

#include <beyond/types/optional.hpp>

namespace vkh {

class DescriptorAllocator {
  VkDevice device_;
  VkDescriptorPool current_pool_ = VK_NULL_HANDLE;
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
