#include "descriptor_allocator.hpp"

namespace vkh {

DescriptorAllocator::DescriptorAllocator(VkDevice device) : device_{device} {}

DescriptorAllocator::~DescriptorAllocator()
{
  // delete every pool held
  for (VkDescriptorPool p : free_pools_) {
    vkDestroyDescriptorPool(device_, p, nullptr);
  }
  for (VkDescriptorPool p : used_pools_) {
    vkDestroyDescriptorPool(device_, p, nullptr);
  }
}

void DescriptorAllocator::reset_pools() {}

auto DescriptorAllocator::allocate(VkDescriptorSet* /*set*/,
                                   VkDescriptorSetLayout /*layout*/)
    -> beyond::optional<VkDescriptorSet>
{
  return beyond::optional<VkDescriptorSet>();
}

} // namespace vkh
