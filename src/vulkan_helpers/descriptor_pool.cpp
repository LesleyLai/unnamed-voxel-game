#include "descriptor_pool.hpp"
#include "context.hpp"
#include "debug_utils.hpp"

#include <beyond/utils/bit_cast.hpp>

namespace vkh {

auto create_descriptor_pool(Context& context,
                            const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>
{

  const VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = create_info.flags,
      .maxSets = create_info.max_sets,
      .poolSizeCount =
          static_cast<std::uint32_t>(create_info.pool_sizes.size()),
      .pPoolSizes = create_info.pool_sizes.data(),
  };

  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VKH_TRY(vkCreateDescriptorPool(context.device(), &pool_info, nullptr,
                                 &descriptor_pool));

  if (create_info.debug_name != nullptr &&
      set_debug_name(context, beyond::bit_cast<uint64_t>(descriptor_pool),
                     VK_OBJECT_TYPE_DESCRIPTOR_POOL, create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return descriptor_pool;
}

} // namespace vkh