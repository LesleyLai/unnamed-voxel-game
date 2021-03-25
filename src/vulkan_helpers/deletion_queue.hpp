#ifndef VOXEL_GAME_VULKAN_DELETION_QUEUE_HPP
#define VOXEL_GAME_VULKAN_DELETION_QUEUE_HPP

#include <vulkan/vulkan.h>

#include <functional>
#include <utility>

#include "context.hpp"

namespace vkh {

class DeletionQueue {
  std::vector<std::function<void(Context&)>> deleters_;
  Context* context_ = nullptr;

public:
  DeletionQueue() = default;
  explicit DeletionQueue(Context& context) : context_{&context} {}

  ~DeletionQueue()
  {
    flush();
  }

  DeletionQueue(const DeletionQueue&) = delete;
  auto operator=(const DeletionQueue&) & -> DeletionQueue& = delete;
  DeletionQueue(DeletionQueue&& other) noexcept
      : deleters_(std::exchange(other.deleters_, {})),
        context_(std::exchange(other.context_, {}))
  {
  }
  auto operator=(DeletionQueue&& other) & noexcept -> DeletionQueue&
  {
    if (this != &other) {
      deleters_ = std::exchange(other.deleters_, {});
      context_ = std::exchange(other.context_, {});
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
      deleter(*context_);
    }
    deleters_.clear();
  }
};

} // namespace vkh

#endif // VOXEL_GAME_VULKAN_DELETION_QUEUE_HPP
