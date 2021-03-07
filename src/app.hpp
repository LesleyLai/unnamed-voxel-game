#ifndef VOXEL_GAME_APP_HPP
#define VOXEL_GAME_APP_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include <array>
#include <cstdint>
#include <vector>

#include <beyond/math/matrix.hpp>
#include <beyond/math/point.hpp>
#include <beyond/math/vector.hpp>

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 viewproj;
};

struct AllocatedBuffer {
  VkBuffer buffer{};
  VmaAllocation allocation{};
};

struct FrameData {
  VkSemaphore render_semaphore{};
  VkSemaphore present_semaphore{};
  VkFence render_fence{};

  VkCommandPool command_pool{};
  VkCommandBuffer main_command_buffer{};

  AllocatedBuffer camera_buffer{};
  VkDescriptorSet global_descriptor{};
};
constexpr std::uint32_t frames_in_flight = 2;

struct BufferCreateInfo {
  size_t size = 0;
  VkBufferUsageFlags usage = 0;
  VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_UNKNOWN;
};

struct AllocatedImage {
  VkImage image{};
  VmaAllocation allocation{};
};

struct Vertex {
  beyond::Point3 position;
  beyond::Vec3 normal;
  beyond::Vec3 color;

  [[nodiscard]] static constexpr auto binding_description()
  {
    return VkVertexInputBindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
  }

  [[nodiscard]] static constexpr auto attributes_descriptions()
  {
    return std::to_array<VkVertexInputAttributeDescription>(
        {{.location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, position)},
         {.location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, position)},
         {.location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, position)}});
  }
};

struct Mesh {
  std::vector<Vertex> vertices_;
  std::vector<std::uint32_t> indices_;
  AllocatedBuffer vertex_buffer_;
  AllocatedBuffer index_buffer_;
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
  VmaAllocator allocator_{};

  VkSwapchainKHR swapchain_{};
  std::vector<VkImage> swapchain_images_{};
  std::vector<VkImageView> swapchain_image_views_{};
  VkFormat swapchain_image_format_{};

  VkImageView depth_image_view_{};
  AllocatedImage depth_image_{};
  VkFormat depth_image_format_{};

  VkRenderPass render_pass_{};
  std::vector<VkFramebuffer> framebuffers_{};

  VkDescriptorSetLayout global_descriptor_set_layout_{};
  VkDescriptorPool descriptor_pool_{};

  std::uint32_t frame_number_ = 0;
  FrameData frame_data_[frames_in_flight]{};

  VkPipelineLayout terrain_graphics_pipeline_layout_{};
  VkPipeline terrain_graphics_pipeline_{};

  Mesh terrain_mesh_{};

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
  void init_descriptors();
  void init_pipeline();

  [[nodiscard]] auto get_current_frame() -> FrameData&;

  [[nodiscard]] auto create_buffer(const BufferCreateInfo& buffer_create_info)
      -> AllocatedBuffer;

  [[nodiscard]] auto
  create_buffer_from_data(const BufferCreateInfo& buffer_create_info,
                          void* data) -> AllocatedBuffer;

  void render();
  void load_mesh();
  void upload_mesh(Mesh& mesh);
};

#endif // VOXEL_GAME_APP_HPP
