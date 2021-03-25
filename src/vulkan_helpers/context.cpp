#include "context.hpp"

#include "buffer.hpp"
#include "error_handling.hpp"

#include <VkBootstrap.h>
#include <fmt/format.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <beyond/utils/bit_cast.hpp>

namespace {

auto create_surface_glfw(VkInstance instance, GLFWwindow* window)
    -> VkSurfaceKHR
{
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
  if (err) {
    const char* error_msg = nullptr;
    int ret = glfwGetError(&error_msg);
    if (ret != 0) {
      fmt::print("{} ", ret);
      if (error_msg != nullptr) { fmt::print("{}", error_msg); }
      fmt::print("\n");
    }
    surface = VK_NULL_HANDLE;
  }
  return surface;
}
} // namespace

namespace vkh {

Context::Context(Window& window)
{
  auto instance_ret = vkb::InstanceBuilder{}
                          .require_api_version(1, 2, 0)
                          .use_default_debug_messenger()
                          .request_validation_layers()
                          .add_validation_feature_enable(
                              VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
                          .enable_extension("VK_EXT_debug_utils")
                          .build();
  if (!instance_ret) {
    fmt::print("{}\n", instance_ret.error().message());
    std::exit(-1);
  }
  instance_ = instance_ret->instance;
  debug_messenger_ = instance_ret->debug_messenger;
  surface_ = create_surface_glfw(instance_, window.glfw_window());

  vkb::PhysicalDeviceSelector phys_device_selector(instance_ret.value());
  auto phys_device_ret = phys_device_selector.set_surface(surface_)
                             .add_required_extension(
                                 VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)
                             .set_required_features({
                                 .fillModeNonSolid = true,
                             })
                             .select();
  if (!phys_device_ret) {
    fmt::print("{}\n", phys_device_ret.error().message());
    std::exit(-1);
  }
  vkb::PhysicalDevice vkb_physical_device = phys_device_ret.value();
  physical_device_ = vkb_physical_device.physical_device;

  vkb::DeviceBuilder device_builder{vkb_physical_device};
  auto device_ret = device_builder.build();
  if (!device_ret) {
    fmt::print("{}\n", device_ret.error().message());
    std::exit(-1);
  }
  auto vkb_device = device_ret.value();
  device_ = vkb_device.device;

  graphics_queue_ = vkb_device.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family_index_ =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();
  compute_queue_ = vkb_device.get_queue(vkb::QueueType::compute).value();
  compute_queue_family_index_ =
      vkb_device.get_queue_index(vkb::QueueType::compute).value();
  transfer_queue_ = vkb_device.get_queue(vkb::QueueType::transfer).value();
  transfer_queue_family_index_ =
      vkb_device.get_queue_index(vkb::QueueType::transfer).value();

  present_queue_ = vkb_device.get_queue(vkb::QueueType::present).value();

  functions_ = {
      .setDebugUtilsObjectNameEXT =
          beyond::bit_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
              vkGetDeviceProcAddr(device_, "vkSetDebugUtilsObjectNameEXT")),
  };

  const VmaAllocatorCreateInfo allocator_create_info{
      .physicalDevice = physical_device_,
      .device = device_,
      .instance = instance_,
  };
  VK_CHECK(vmaCreateAllocator(&allocator_create_info, &allocator_));
} // namespace vkh

Context::~Context()
{
  if (!instance_) return;

  vmaDestroyAllocator(allocator_);

  vkDestroyDevice(device_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkb::destroy_debug_utils_messenger(instance_, debug_messenger_, nullptr);
  vkDestroyInstance(instance_, nullptr);
}

Context::Context(vkh::Context&& other) noexcept
    : instance_{std::exchange(other.instance_, {})},
      debug_messenger_{std::exchange(other.debug_messenger_, {})},
      surface_{std::exchange(other.surface_, {})},
      physical_device_{std::exchange(other.physical_device_, {})},
      device_{std::exchange(other.device_, {})},
      graphics_queue_{std::exchange(other.graphics_queue_, {})},
      compute_queue_{std::exchange(other.compute_queue_, {})},
      transfer_queue_{std::exchange(other.transfer_queue_, {})},
      present_queue_{std::exchange(other.present_queue_, {})},
      graphics_queue_family_index_{
          std::exchange(other.graphics_queue_family_index_, {})},
      compute_queue_family_index_{
          std::exchange(other.compute_queue_family_index_, {})},
      transfer_queue_family_index_{
          std::exchange(other.transfer_queue_family_index_, {})},
      functions_{std::exchange(other.functions_, {})},
      allocator_{std::exchange(other.allocator_, {})}
{
}

auto Context::operator=(Context&& other) & noexcept -> Context&
{
  if (this != &other) {
    this->~Context();
    instance_ = std::exchange(other.instance_, {});
    debug_messenger_ = std::exchange(other.debug_messenger_, {});
    surface_ = std::exchange(other.surface_, {});
    physical_device_ = std::exchange(other.physical_device_, {});
    device_ = std::exchange(other.device_, {});
    graphics_queue_ = std::exchange(other.graphics_queue_, {});
    compute_queue_ = std::exchange(other.compute_queue_, {});
    transfer_queue_ = std::exchange(other.transfer_queue_, {});
    present_queue_ = std::exchange(other.present_queue_, {});
    graphics_queue_family_index_ =
        std::exchange(other.graphics_queue_family_index_, {});
    compute_queue_family_index_ =
        std::exchange(other.compute_queue_family_index_, {});
    transfer_queue_family_index_ =
        std::exchange(other.transfer_queue_family_index_, {});
    functions_ = std::exchange(other.functions_, {});
    allocator_ = std::exchange(other.allocator_, {});
  }
  return *this;
}

auto Context::map_impl(const Buffer& buffer) -> Expected<void*>
{
  void* ptr = nullptr;
  VKH_TRY(vmaMapMemory(allocator_, buffer.allocation, &ptr));
  return ptr;
}

void Context::unmap(const Buffer& buffer)
{
  vmaUnmapMemory(allocator_, buffer.allocation);
}

} // namespace vkh