#include "swapchain.hpp"
#include "context.hpp"

#include <VkBootstrap.h>

namespace vkh {

Swapchain::Swapchain(Context& context, const SwapchainCreateInfo& create_info)
    : device_{context.device()}
{
  vkb::SwapchainBuilder swapchain_builder{context.physical_device(),
                                          context.device(), context.surface()};

  vkb::Swapchain vkb_swapchain =
      swapchain_builder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(create_info.extent.width,
                              create_info.extent.height)
          .build()
          .value();

  swapchain_ = vkb_swapchain.swapchain;
  images_ = vkb_swapchain.get_images().value();
  image_views_ = vkb_swapchain.get_image_views().value();
  image_format_ = vkb_swapchain.image_format;
}

Swapchain::~Swapchain()
{
  if (device_) {
    for (VkImageView image_view : image_views_) {
      vkDestroyImageView(device_, image_view, nullptr);
    }
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  }
}

Swapchain::Swapchain(Swapchain&& other) noexcept
    : device_{std::exchange(other.device_, {})},
      swapchain_{std::exchange(other.swapchain_, {})}, images_{std::exchange(
                                                           other.images_, {})},
      image_views_{std::exchange(other.image_views_, {})},
      image_format_{std::exchange(other.image_format_, {})}
{
}

auto Swapchain::operator=(Swapchain&& other) & noexcept -> Swapchain&
{
  if (this != &other) {
    this->~Swapchain();
    device_ = std::exchange(other.device_, {});
    swapchain_ = std::exchange(other.swapchain_, {});
    images_ = std::exchange(other.images_, {});
    image_views_ = std::exchange(other.image_views_, {});
    image_format_ = std::exchange(other.image_format_, {});
  }
  return *this;
}

} // namespace vkh
