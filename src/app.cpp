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
  init_command();
  init_render_pass();
}

App::~App()
{
  if (!device_) { return; }

  vkDestroyRenderPass(device_, render_pass_, nullptr);
  vkDestroyCommandPool(device_, command_pool_, nullptr);

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
  auto vkb_device = device_ret.value();
  device_ = vkb_device.device;

  graphics_queue_ = vkb_device.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family_index_ =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();
  present_queue_ = vkb_device.get_queue(vkb::QueueType::present).value();
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

void App::init_command()
{
  const VkCommandPoolCreateInfo command_pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphics_queue_family_index_,
  };

  VK_CHECK(vkCreateCommandPool(device_, &command_pool_create_info, nullptr,
                               &command_pool_));

  const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = command_pool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VK_CHECK(vkAllocateCommandBuffers(device_, &command_buffer_allocate_info,
                                    &main_command_buffer_));
}
void App::init_render_pass()
{
  // the renderpass will use this color attachment.
  const VkAttachmentDescription color_attachment = {
      .flags = 0,
      .format = swapchain_image_format_,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  static constexpr VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  static constexpr VkSubpassDescription subpass = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = nullptr,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
      .pResolveAttachments = nullptr,
      .pDepthStencilAttachment = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = nullptr};

  const VkRenderPassCreateInfo render_pass_create_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 0,
      .pDependencies = nullptr,
  };

  VK_CHECK(vkCreateRenderPass(device_, &render_pass_create_info, nullptr,
                              &render_pass_));
}
