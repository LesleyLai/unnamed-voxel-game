#include "buffer.hpp"

#include "context.hpp"
#include "debug_utils.hpp"

#include <beyond/utils/bit_cast.hpp>

namespace vkh {

auto create_buffer(vkh::Context& context,
                   const BufferCreateInfo& buffer_create_info)
    -> Expected<Buffer>
{
  const VkBufferCreateInfo vk_buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = buffer_create_info.size,
      .usage = buffer_create_info.usage,
  };

  const VmaAllocationCreateInfo vma_alloc_info{
      .usage = buffer_create_info.memory_usage};

  Buffer allocated_buffer;
  if (VkResult result = vmaCreateBuffer(
          context.allocator(), &vk_buffer_create_info, &vma_alloc_info,
          &allocated_buffer.buffer, &allocated_buffer.allocation, nullptr);
      result != VK_SUCCESS) {
    return beyond::make_unexpected(result);
  }

  if (buffer_create_info.debug_name != nullptr &&
      set_debug_name(context,
                     beyond::bit_cast<uint64_t>(allocated_buffer.buffer),
                     VK_OBJECT_TYPE_BUFFER, buffer_create_info.debug_name)) {
    report_fail_to_set_debug_name(buffer_create_info.debug_name);
  }

  return allocated_buffer;
}

auto create_buffer_from_data(vkh::Context& context,
                             const BufferCreateInfo& buffer_create_info,
                             const void* data) -> Expected<Buffer>
{
  auto buffer_ret = create_buffer(context, buffer_create_info);
  if (!buffer_ret.has_value()) { return buffer_ret; }

  Buffer buffer = *buffer_ret;
  auto map_ret = context.map(buffer);
  if (!map_ret.has_value()) { return beyond::make_unexpected(map_ret.error()); }
  std::memcpy(*map_ret, data, buffer_create_info.size);
  context.unmap(buffer);
  return buffer;
}

void destroy_buffer(vkh::Context& context, Buffer buffer)
{
  vmaDestroyBuffer(context.allocator(), buffer.buffer, buffer.allocation);
}

} // namespace vkh