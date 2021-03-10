#ifndef VOXEL_GAME_VULKAN_CONTEXT_HPP
#define VOXEL_GAME_VULKAN_CONTEXT_HPP

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <beyond/utils/force_inline.hpp>

#include <cstdint>

#include "../window_helpers/window.hpp"

namespace vkh {

class Context {
  VkInstance instance_{};
  VkDebugUtilsMessengerEXT debug_messenger_{};
  VkSurfaceKHR surface_{};
  VkPhysicalDevice physical_device_{};
  VkDevice device_{};
  VkQueue graphics_queue_{};
  std::uint32_t graphics_queue_family_index_ = 0;
  VkQueue present_queue_{};
  VmaAllocator allocator_{};

public:
  Context() = default;
  explicit Context(Window& window);
  ~Context();

  Context(const Context&) = delete;
  auto operator=(const Context&) -> Context& = delete;
  Context(Context&& other) noexcept;
  auto operator=(Context&& other) & noexcept -> Context&;

  BEYOND_FORCE_INLINE void wait_idle() noexcept
  {
    vkDeviceWaitIdle(device_);
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto instance() noexcept -> VkInstance
  {
    return instance_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto debug_messenger() noexcept
      -> VkDebugUtilsMessengerEXT
  {
    return debug_messenger_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto surface() noexcept -> VkSurfaceKHR
  {
    return surface_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto physical_device() noexcept
      -> VkPhysicalDevice
  {
    return physical_device_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto device() noexcept -> VkDevice
  {
    return device_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto graphics_queue() noexcept -> VkQueue
  {
    return graphics_queue_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto graphics_queue_family_index() noexcept
      -> std::uint32_t
  {
    return graphics_queue_family_index_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto present_queue() noexcept -> VkQueue
  {
    return present_queue_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto allocator() noexcept -> VmaAllocator
  {
    return allocator_;
  }

  BEYOND_FORCE_INLINE explicit operator bool()
  {
    return instance_;
  }
};

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_CONTEXT_HPP
