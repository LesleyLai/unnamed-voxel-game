#include "commands.hpp"

#include "debug_utils.hpp"

#include <beyond/utils/bit_cast.hpp>

namespace vkh {

[[nodiscard]] auto allocate_command_buffer(Context& context,
                                           CommandBufferAllocInfo alloc_info)
    -> Expected<VkCommandBuffer>
{
  const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
      alloc_info.command_pool, alloc_info.level, 1};

  VkCommandBuffer command_buffer{};
  VKH_TRY(vkAllocateCommandBuffers(
      context.device(), &command_buffer_allocate_info, &command_buffer));
  VKH_TRY(vkh::set_debug_name(
      context, beyond::bit_cast<uint64_t>(command_buffer),
      VK_OBJECT_TYPE_COMMAND_BUFFER, alloc_info.debug_name));
  return command_buffer;
}

} // namespace vkh