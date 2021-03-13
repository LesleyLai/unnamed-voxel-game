#ifndef VOXEL_GAME_VULKAN_DELETION_QUEUE_HPP
#define VOXEL_GAME_VULKAN_DELETION_QUEUE_HPP

#include <vulkan/vulkan.h>

#include <functional>
#include <utility>

#include "context.hpp"

namespace vkh {

class DeletionQueue {
  std::vector<std::function<void(VkDevice)>> deleters_;
  VkDevice device_{};

public:
  DeletionQueue() = default;
  explicit DeletionQueue(Context& context) : device_{context.device()} {}

  ~DeletionQueue()
  {
    flush();
  }

  DeletionQueue(const DeletionQueue&) = delete;
  auto operator=(const DeletionQueue&) & -> DeletionQueue& = delete;
  DeletionQueue(DeletionQueue&& other) noexcept
      : deleters_(std::exchange(other.deleters_, {})),
        device_(std::exchange(other.device_, {}))
  {
  }
  auto operator=(DeletionQueue&& other) & noexcept -> DeletionQueue&
  {
    if (this != &other) {
      deleters_ = std::exchange(other.deleters_, {});
      device_ = std::exchange(other.device_, {});
    }
    return *this;
  }

  template <class Func> void push(Func&& function)
  {
    deleters_.push_back(std::forward<Func&&>(function));
  }

  void flush()
  {
    for (auto& deleter : deleters_) {
      deleter(device_);
    }
    deleters_.clear();
  }
};

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_DELETION_QUEUE_HPP
