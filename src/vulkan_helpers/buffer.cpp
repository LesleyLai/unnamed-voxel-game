#include "buffer.hpp"

#include "context.hpp"
#include "debug_utils.hpp"
#include "error_handling.hpp"

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
  VKH_TRY(vmaCreateBuffer(context.allocator(), &vk_buffer_create_info,
                          &vma_alloc_info, &allocated_buffer.buffer,
                          &allocated_buffer.allocation, nullptr));

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
  return create_buffer(context, buffer_create_info)
      .and_then([&](Buffer buffer) -> Expected<Buffer> {
        BEYOND_EXPECTED_ASSIGN(void*, buffer_ptr, context.map(buffer));
        std::memcpy(buffer_ptr, data, buffer_create_info.size);
        context.unmap(buffer);
        return buffer;
      });
}

void destroy_buffer(vkh::Context& context, Buffer buffer)
{
  vmaDestroyBuffer(context.allocator(), buffer.buffer, buffer.allocation);
}

} // namespace vkh