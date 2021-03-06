#ifndef VOXEL_GAME_APP_HPP
#define VOXEL_GAME_APP_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct FrameData {
  VkSemaphore render_semaphore_{};
  VkSemaphore present_semaphore_{};
  VkFence render_fence_{};
};

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
  std::vector<VkFramebuffer> framebuffers_{};

  std::uint32_t frame_number_ = 0;
  FrameData frame_data_{};

  VkPipelineLayout terrain_graphics_pipeline_layout_{};
  VkPipeline terrain_graphics_pipeline_{};

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
  void init_framebuffer();
  void init_sync_strucures();
  void init_pipeline();

  void render();
};

#endif // VOXEL_GAME_APP_HPP
