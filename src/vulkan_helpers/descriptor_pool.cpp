#include "descriptor_pool.hpp"
#include "context.hpp"
#include "debug_utils.hpp"

#include <algorithm>
#include <beyond/container/static_vector.hpp>
#include <iterator>

namespace {

constexpr auto to_vkDescriptorPoolSize(vkh::PoolSize size, std::uint32_t count)
{
  return VkDescriptorPoolSize{.type = size.type,
                              .descriptorCount = static_cast<std::uint32_t>(
                                  size.multiplier * static_cast<float>(count))};
}

} // namespace

namespace vkh {

auto create_descriptor_pool(Context& context,
                            const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>
{
  beyond::StaticVector<VkDescriptorPoolSize, 24> sizes;
  BEYOND_ENSURE(sizes.capacity() >= create_info.pool_sizes.size());
  std::ranges::transform(
      create_info.pool_sizes, std::back_inserter(sizes), [&](PoolSize size) {
        return to_vkDescriptorPoolSize(size, create_info.count);
      });

  const VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = create_info.flags,
      .maxSets = create_info.count,
      .poolSizeCount = sizes.size(),
      .pPoolSizes = sizes.data(),
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