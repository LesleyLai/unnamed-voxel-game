#ifndef VOXEL_GAME_VULKAN_UNIQUE_RESOURCE_HPP
#define VOXEL_GAME_VULKAN_UNIQUE_RESOURCE_HPP

#include <utility>
#include <vulkan/vulkan_core.h>

namespace vkh {

template <typename T,
          void (*deleter)(VkDevice, T, const VkAllocationCallbacks*)>
class UniqueResource {
public:
  UniqueResource() noexcept = default;
  UniqueResource(VkDevice device, T resource) noexcept
      : device_{device}, resource_{resource}
  {
  }

  ~UniqueResource() noexcept
  {
    delete_without_reset();
  }

  UniqueResource(const UniqueResource& other) noexcept = delete;
  auto operator=(const UniqueResource& other) & noexcept
      -> UniqueResource& = delete;

  UniqueResource(UniqueResource&& other) noexcept
      : device_{std::exchange(other.device_, nullptr)}, resource_{std::exchange(
                                                            other.resource_,
                                                            nullptr)}
  {
  }

  auto operator=(UniqueResource&& other) & noexcept -> UniqueResource&
  {
    if (resource_ != other.resource_) {
      delete_without_reset();
      device_ = std::exchange(other.device_, nullptr);
      resource_ = std::exchange(other.resource_, nullptr);
    }
    return *this;
  }

  auto reset() noexcept -> void
  {
    delete_without_reset();
    device_ = nullptr;
    resource_ = nullptr;
  }

  auto get() const noexcept -> T
  {
    return resource_;
  }

  explicit(false) operator T() const noexcept
  {
    return resource_;
  }

  auto swap(UniqueResource& rhs) noexcept -> void
  {
    std::swap(device_, rhs.device_);
    std::swap(resource_, rhs.resource_);
  }

  friend auto swap(UniqueResource& lhs, UniqueResource& rhs) noexcept -> void
  {
    lhs.swap(rhs);
  }

private:
  auto delete_without_reset() noexcept -> void
  {
    if (resource_ != nullptr) { deleter(device_, resource_, nullptr); }
  }

  VkDevice device_ = nullptr;
  T resource_ = {};
};

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_UNIQUE_RESOURCE_HPP
