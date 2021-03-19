#include "buffer.hpp"

#include "context.hpp"

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
  return allocated_buffer;
}

auto create_buffer_from_data(vkh::Context& context,
                             const BufferCreateInfo& buffer_create_info,
                             const void* data) -> Expected<Buffer>
{
  return create_buffer(context, buffer_create_info)
      .and_then([&](Buffer buffer) -> Expected<Buffer> {
        auto result = context.map(buffer);
        if (!result.has_value()) {
          return beyond::make_unexpected(result.error());
        }
        std::memcpy(*result, data, buffer_create_info.size);
        context.unmap(buffer);
        return buffer;
      });
}

void destroy_buffer(vkh::Context& context, Buffer buffer)
{
  vmaDestroyBuffer(context.allocator(), buffer.buffer, buffer.allocation);
}

} // namespace vkh