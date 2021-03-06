#ifndef VOXEL_GAME_APP_HPP
#define VOXEL_GAME_APP_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>

class App {
  GLFWwindow* window_ = nullptr;
  VkExtent2D window_extent_{};

  VkInstance instance_{};
  VkDebugUtilsMessengerEXT debug_messenger_{};
  VkSurfaceKHR surface_{};
  VkPhysicalDevice physical_device_{};
  VkDevice device_{};
  VkQueue graphics_queue_{};
  uint32_t graphics_queue_family_index_ = 0;
  VkQueue present_queue_{};

  VkSwapchainKHR swapchain_{};
  std::vector<VkImage> swapchain_images_{};
  std::vector<VkImageView> swapchain_image_views_{};
  VkFormat swapchain_image_format_{};

  VkCommandPool command_pool_{};
  VkCommandBuffer main_command_buffer_{};

  VkRenderPass render_pass_{};

public:
  App();
  ~App();

  void exec();

  App(const App&) = delete;
  auto operator=(const App&) -> App& = delete;
  App(App&&) noexcept = delete;
  auto operator=(App&&) noexcept -> App& = delete;

private:
  void init_vk_device();
  void init_swapchain();
  void init_command();
  void init_render_pass();
};

#endif // VOXEL_GAME_APP_HPP
