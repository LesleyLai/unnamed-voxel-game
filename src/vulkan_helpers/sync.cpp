#include "sync.hpp"

#include "context.hpp"
#include "debug_utils.hpp"

#include <beyond/utils/bit_cast.hpp>

namespace vkh {

[[nodiscard]] auto create_fence(Context& context,
                                const FenceCreateInfo& create_info)
    -> Expected<VkFence>
{
  const VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  VkFence fence = {};
  if (auto result = vkCreateFence(context, &fence_create_info, nullptr, &fence);
      result != VK_SUCCESS) {
    return beyond::unexpected(result);
  }

  if (set_debug_name(context, beyond::bit_cast<uint64_t>(fence),
                     VK_OBJECT_TYPE_FENCE, create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return fence;
}

[[nodiscard]] auto create_semaphore(Context& context,
                                    const SemaphoreCreateInfo& create_info)
    -> Expected<VkSemaphore>
{
  const VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  VkSemaphore semaphore = {};
  if (auto result = vkCreateSemaphore(context, &semaphore_create_info, nullptr,
                                      &semaphore);
      result != VK_SUCCESS) {
    return beyond::unexpected(result);
  }

  if (set_debug_name(context, beyond::bit_cast<uint64_t>(semaphore),
                     VK_OBJECT_TYPE_SEMAPHORE, create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return semaphore;
}

} // namespace vkh
