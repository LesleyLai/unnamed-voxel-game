#ifndef VOXEL_GAME_APP_HPP
#define VOXEL_GAME_APP_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

#include <beyond/math/matrix.hpp>
#include <beyond/math/point.hpp>
#include <beyond/math/vector.hpp>
#include <beyond/utils/function_ref.hpp>

#include "window_helpers/window.hpp"
#include "window_helpers/window_manager.hpp"

#include "vulkan_helpers/context.hpp"
#include "vulkan_helpers/deletion_queue.hpp"
#include "vulkan_helpers/vk_check.hpp"

#include "first_person_camera.hpp"
#include "marching_cubes.hpp"

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 viewproj;
};

struct AllocatedBuffer {
  VkBuffer buffer{};
  VmaAllocation allocation{};

  template <typename T = void> auto map(vkh::Context& context) -> T*
  {
    void* ptr = nullptr;
    VK_CHECK(vmaMapMemory(context.allocator(), allocation, &ptr));
    return static_cast<T*>(ptr);
  }

  void unmap(vkh::Context& context)
  {
    vmaUnmapMemory(context.allocator(), allocation);
  }
};

inline void destroy_allocated_buffer(vkh::Context& context,
                                     AllocatedBuffer buffer)
{
  vmaDestroyBuffer(context.allocator(), buffer.buffer, buffer.allocation);
}

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

struct Mesh {
  std::vector<Vertex> vertices_;
  std::vector<std::uint32_t> indices_;
  AllocatedBuffer vertex_buffer_;
  AllocatedBuffer index_buffer_;
};

enum class MouseDraggingState { No, Start, Dragging };

enum class RenderMode { Fill, Wireframe };

struct UploadContext {
  VkFence fence = {};
  VkCommandPool command_pool = {};
};

class App {
  WindowManager* window_manager_ = nullptr;
  Window window_;

  VkExtent2D window_extent_{};

  vkh::Context context_;
  vkh::DeletionQueue deletion_queue_;

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
  VkPipeline terrain_wireframe_graphics_pipeline_{};

  Mesh terrain_mesh_{};

  FirstPersonCamera camera_{beyond::Vec3(0.0f, 0.0f, 5.0f)};
  MouseDraggingState dragging_ = MouseDraggingState::No;
  float last_mouse_x_{};
  float last_mouse_y_{};
  RenderMode render_mode_ = RenderMode::Fill;

  UploadContext upload_context_;

public:
  App();
  ~App();

  void exec();

  App(const App&) = delete;
  auto operator=(const App&) -> App& = delete;
  App(App&&) noexcept = delete;
  auto operator=(App&&) noexcept -> App& = delete;

  void move_camera(FirstPersonCamera::Movement movement);
  void mouse_dragging(bool is_dragging);
  [[nodiscard]] auto dragging_status() const
  {
    return dragging_;
  }
  void mouse_move(float x, float y);

  [[nodiscard]] auto render_mode() const noexcept -> RenderMode
  {
    return render_mode_;
  }

  void set_render_mode(RenderMode render_mode) noexcept
  {
    render_mode_ = render_mode;
  }

private:
  void init_swapchain();
  void init_command();
  void init_render_pass();
  void init_framebuffer();
  void init_sync_strucures();
  void init_imgui();
  void init_descriptors();
  void init_pipeline();

  [[nodiscard]] auto get_current_frame() -> FrameData&;

  [[nodiscard]] auto create_buffer(const BufferCreateInfo& buffer_create_info)
      -> AllocatedBuffer;

  [[nodiscard]] auto
  create_buffer_from_data(const BufferCreateInfo& buffer_create_info,
                          const void* data) -> AllocatedBuffer;

  void render();
  void render_gui();
  void generate_mesh();
  void upload_mesh(Mesh& mesh);

  void
  immediate_submit(beyond::function_ref<void(VkCommandBuffer cmd)> function);
};

#endif // VOXEL_GAME_APP_HPP
