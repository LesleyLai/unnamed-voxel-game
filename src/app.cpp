#include "app.hpp"

#include <VkBootstrap.h>
#include <fmt/format.h>

#define CHECK_GLFW(glfw_call)                                                  \
  if (!(glfw_call)) { std::exit(1); }

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      fmt::print("Vulkan error: {}\n", err);                                   \
      exit(-1);                                                                \
    }                                                                          \
  } while (0)

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

App::App()
{
  CHECK_GLFW(glfwInit());

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  static constexpr int window_width = 1400;
  static constexpr int window_height = 900;
  window_ = glfwCreateWindow(window_width, window_height, "Voxel Game", nullptr,
                             nullptr);
  if (!window_) { std::exit(1); }
  window_extent_ = VkExtent2D{static_cast<std::uint32_t>(window_width),
                              static_cast<std::uint32_t>(window_height)};

  glfwMakeContextCurrent(window_);

  init_vk_device();
  init_swapchain();
}

App::~App()
{
  if (!device_) { return; }

  for (auto& image_view : swapchain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);

  vkDestroyDevice(device_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkb::destroy_debug_utils_messenger(instance_, debug_messenger_, nullptr);
  vkDestroyInstance(instance_, nullptr);

  glfwDestroyWindow(window_);
  glfwTerminate();
}

void App::exec()
{
  while (!glfwWindowShouldClose(window_)) {
    glfwSwapBuffers(window_);
    glfwPollEvents();
  }
}

void App::init_vk_device()
{

  auto instance_ret = vkb::InstanceBuilder{}
                          .use_default_debug_messenger()
                          .request_validation_layers()
                          .build();
  if (!instance_ret) {
    fmt::print("{}\n", instance_ret.error().message());
    std::exit(-1);
  }
  instance_ = instance_ret->instance;
  debug_messenger_ = instance_ret->debug_messenger;
  surface_ = create_surface_glfw(instance_, window_);

  vkb::PhysicalDeviceSelector phys_device_selector(instance_ret.value());
  auto phys_device_ret = phys_device_selector.set_surface(surface_).select();
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
  device_ = device_ret->device;
}

void App::init_swapchain()
{
  vkb::SwapchainBuilder swapchain_builder{physical_device_, device_, surface_};

  vkb::Swapchain vkb_swapchain =
      swapchain_builder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(window_extent_.width, window_extent_.height)
          .build()
          .value();

  swapchain_ = vkb_swapchain.swapchain;
  swapchain_images_ = vkb_swapchain.get_images().value();
  swapchain_image_views_ = vkb_swapchain.get_image_views().value();
  swapchain_image_format_ = vkb_swapchain.image_format;
}