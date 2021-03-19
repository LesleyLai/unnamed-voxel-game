#ifndef VOXEL_GAME_VULKAN_BUFFER_HPP
#define VOXEL_GAME_VULKAN_BUFFER_HPP

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <beyond/utils/assert.hpp>

#include "expected.hpp"

namespace vkh {

class Context;

struct BufferCreateInfo {
  size_t size = 0;
  VkBufferUsageFlags usage = 0;
  VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_UNKNOWN;
  const char* debug_name = nullptr;
};

struct [[nodiscard]] Buffer {
  VkBuffer buffer{};
  VmaAllocation allocation{};

  explicit(false) operator VkBuffer()
  {
    return buffer;
  }
};

auto create_buffer(vkh::Context& context,
                   const BufferCreateInfo& buffer_create_info)
    -> Expected<Buffer>;

auto create_buffer_from_data(vkh::Context& context,
                             const BufferCreateInfo& buffer_create_info,
                             const void* data) -> Expected<Buffer>;

template <typename T>
auto create_buffer_from_data(vkh::Context& context,
                             const BufferCreateInfo& buffer_create_info,
                             const T* data) -> Expected<Buffer>
{
  BEYOND_ENSURE(sizeof(T) <= buffer_create_info.size);
  return create_buffer_from_data(context, buffer_create_info,
                                 static_cast<const void*>(data));
}

template <typename T>
auto create_buffer_from_data(vkh::Context& context,
                             const BufferCreateInfo& buffer_create_info,
                             const T& data) -> Expected<Buffer>
{
  BEYOND_ENSURE(sizeof(T) <= buffer_create_info.size);
  return create_buffer_from_data(context, buffer_create_info,
                                 static_cast<const void*>(&data));
}

void destroy_buffer(vkh::Context& context, Buffer buffer);

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_BUFFER_HPP
