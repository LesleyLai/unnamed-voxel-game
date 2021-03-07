#include "app.hpp"

#include <VkBootstrap.h>
#include <fmt/format.h>

#include <beyond/utils/panic.hpp>
#include <beyond/utils/size.hpp>
#include <beyond/utils/to_pointer.hpp>

#include "vulkan_helpers/shader_module.hpp"

#define CHECK_GLFW(glfw_call)                                                  \
  if (!(glfw_call)) { beyond::panic("GLFW fatal error\n"); }

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) { fmt::format("Vulkan error: {}\n", err); }                       \
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
  init_framebuffer();
  init_sync_strucures();
  init_pipeline();
  load_mesh();
}

App::~App()
{
  if (!device_) { return; }

  vkDeviceWaitIdle(device_);

  vmaDestroyBuffer(allocator_, terrain_mesh_.index_buffer_.buffer_,
                   terrain_mesh_.index_buffer_.allocation_);
  vmaDestroyBuffer(allocator_, terrain_mesh_.vertex_buffer_.buffer_,
                   terrain_mesh_.vertex_buffer_.allocation_);

  vkDestroyPipeline(device_, terrain_graphics_pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, terrain_graphics_pipeline_layout_, nullptr);

  vkDestroyRenderPass(device_, render_pass_, nullptr);

  for (auto& frame_data : frame_data_) {
    vkDestroyFence(device_, frame_data.render_fence_, nullptr);
    vkDestroySemaphore(device_, frame_data.render_semaphore_, nullptr);
    vkDestroySemaphore(device_, frame_data.present_semaphore_, nullptr);
    vkDestroyCommandPool(device_, frame_data.command_pool_, nullptr);
  }

  for (auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  for (auto& image_view : swapchain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  vkDestroyImageView(device_, depth_image_view_, nullptr);
  vmaDestroyImage(allocator_, depth_image_.image_, depth_image_.allocation_);
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);

  vmaDestroyAllocator(allocator_);

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
    render();

    glfwSwapBuffers(window_);
    glfwPollEvents();
  }
}

void App::init_vk_device()
{

  auto instance_ret = vkb::InstanceBuilder{}
                          .require_api_version(1, 2, 0)
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

  const VmaAllocatorCreateInfo allocator_create_info{
      .physicalDevice = physical_device_,
      .device = device_,
      .instance = instance_,
  };
  VK_CHECK(vmaCreateAllocator(&allocator_create_info, &allocator_));
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

  depth_image_format_ = VK_FORMAT_D32_SFLOAT;

  const VkExtent3D depth_extent = {window_extent_.width, window_extent_.height,
                                   1};
  const VkImageCreateInfo depth_image_create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depth_image_format_,
      .extent = depth_extent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
  };
  constexpr VmaAllocationCreateInfo depth_image_allocation_create_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags =
          VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };
  vmaCreateImage(allocator_, &depth_image_create_info,
                 &depth_image_allocation_create_info, &depth_image_.image_,
                 &depth_image_.allocation_, nullptr);

  const VkImageViewCreateInfo depth_view_create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .image = depth_image_.image_,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth_image_format_,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }};
  vkCreateImageView(device_, &depth_view_create_info, nullptr,
                    &depth_image_view_);
}

void App::init_command()
{
  const VkCommandPoolCreateInfo command_pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphics_queue_family_index_,
  };

  for (auto& frame_data : frame_data_) {
    VK_CHECK(vkCreateCommandPool(device_, &command_pool_create_info, nullptr,
                                 &frame_data.command_pool_));

    const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = frame_data.command_pool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(device_, &command_buffer_allocate_info,
                                      &frame_data.main_command_buffer_));
  }
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

  const VkAttachmentDescription depth_attachment = {
      .flags = 0,
      .format = depth_image_format_,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  static constexpr VkAttachmentReference depth_attachment_ref = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  static constexpr VkSubpassDescription subpass = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = nullptr,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
      .pResolveAttachments = nullptr,
      .pDepthStencilAttachment = &depth_attachment_ref,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = nullptr};

  const VkAttachmentDescription attachments[] = {color_attachment,
                                                 depth_attachment};

  const VkRenderPassCreateInfo render_pass_create_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .attachmentCount = beyond::size(attachments),
      .pAttachments = beyond::to_pointer(attachments),
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 0,
      .pDependencies = nullptr,
  };

  VK_CHECK(vkCreateRenderPass(device_, &render_pass_create_info, nullptr,
                              &render_pass_));
}

void App::init_framebuffer()
{
  // create the framebuffers for the swapchain images. This will connect the
  // render-pass to the images for rendering
  VkFramebufferCreateInfo framebuffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .renderPass = render_pass_,
      .attachmentCount = 1,
      .width = window_extent_.width,
      .height = window_extent_.height,
      .layers = 1,
  };

  // grab how many images we have in the swapchain
  const auto swapchain_imagecount =
      static_cast<std::uint32_t>(swapchain_images_.size());
  framebuffers_ = std::vector<VkFramebuffer>(swapchain_imagecount);

  // create framebuffers for each of the swapchain image views
  for (std::uint32_t i = 0; i < swapchain_imagecount; ++i) {
    const VkImageView attachments[] = {swapchain_image_views_[i],
                                       depth_image_view_};
    framebuffer_create_info.pAttachments = beyond::to_pointer(attachments);
    framebuffer_create_info.attachmentCount = beyond::size(attachments);
    VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr,
                                 &framebuffers_[i]));
  }
}

void App::init_sync_strucures()
{
  const VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  const VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  for (auto& frame_Data : frame_data_) {
    VK_CHECK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                               &frame_Data.render_semaphore_));
    VK_CHECK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                               &frame_Data.present_semaphore_));
    VK_CHECK(vkCreateFence(device_, &fence_create_info, nullptr,
                           &frame_Data.render_fence_));
  }
}

void App::init_pipeline()
{
  auto terrain_vert_ret =
      vkh::create_shader_module(device_, "shaders/terrain.vert.spv");
  auto terrain_frag_ret =
      vkh::create_shader_module(device_, "shaders/terrain.frag.spv");
  auto terrain_vert_shader_module = terrain_vert_ret.value();
  auto terrain_frag_shader_module = terrain_frag_ret.value();

  const VkPipelineShaderStageCreateInfo shader_stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = terrain_vert_shader_module,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = terrain_frag_shader_module,
       .pName = "main"}};

  static constexpr auto vertex_binding_description =
      Vertex::binding_description();
  static constexpr auto vertex_attribute_descriptions =
      Vertex::attributes_descriptions();

  static constexpr VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertex_binding_description,
      .vertexAttributeDescriptionCount =
          static_cast<std::uint32_t>(vertex_attribute_descriptions.size()),
      .pVertexAttributeDescriptions = vertex_attribute_descriptions.data()};

  static constexpr VkPipelineInputAssemblyStateCreateInfo input_assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  const VkViewport viewport{
      .x = 0.0f,
      .y = static_cast<float>(window_extent_.height),
      .width = static_cast<float>(window_extent_.width),
      .height = -static_cast<float>(window_extent_.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  const VkRect2D scissor{.offset = {0, 0}, .extent = window_extent_};

  const VkPipelineViewportStateCreateInfo viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  static constexpr VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  static constexpr VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  static constexpr VkPipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  static constexpr VkPipelineColorBlendStateCreateInfo color_blending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pushConstantRangeCount = 0,
  };

  if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr,
                             &terrain_graphics_pipeline_layout_) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  const VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .minDepthBounds = 0.0f, // Optional
      .maxDepthBounds = 1.0f, // Optional
  };

  const VkGraphicsPipelineCreateInfo pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = beyond::size(shader_stages),
      .pStages = beyond::to_pointer(shader_stages),
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depth_stencil_state,
      .pColorBlendState = &color_blending,
      .layout = terrain_graphics_pipeline_layout_,
      .renderPass = render_pass_,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };

  VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1,
                                     &pipeline_create_info, nullptr,
                                     &terrain_graphics_pipeline_));

  vkDestroyShaderModule(device_, terrain_vert_shader_module, nullptr);
  vkDestroyShaderModule(device_, terrain_frag_shader_module, nullptr);
}

void App::render()
{
  auto current_frame_data = get_current_frame();
  static constexpr std::uint64_t time_out = 1e9;
  VK_CHECK(vkWaitForFences(device_, 1, &current_frame_data.render_fence_, true,
                           time_out));
  VK_CHECK(vkResetFences(device_, 1, &current_frame_data.render_fence_));

  uint32_t swapchain_image_index = 0;
  VK_CHECK(vkAcquireNextImageKHR(device_, swapchain_, time_out,
                                 current_frame_data.present_semaphore_, nullptr,
                                 &swapchain_image_index));
  VK_CHECK(vkResetCommandBuffer(current_frame_data.main_command_buffer_, 0));

  VkCommandBuffer cmd = current_frame_data.main_command_buffer_;
  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  const float flash =
      std::abs(std::sin(static_cast<float>(frame_number_) / 120.f));
  const VkClearValue clear_value = {.color = {{0.0f, 0.0f, flash, 1.0f}}};
  const VkClearValue depth_clear_value = {.depthStencil = {.depth = 1.f}};

  const VkClearValue clear_values[] = {clear_value, depth_clear_value};

  const VkRenderPassBeginInfo render_pass_begin_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = render_pass_,
      .framebuffer = framebuffers_[swapchain_image_index],
      .renderArea = {.offset = {.x = 0, .y = 0}, .extent = window_extent_},
      .clearValueCount = beyond::size(clear_values),
      .pClearValues = beyond::to_pointer(clear_values),
  };

  vkCmdBeginRenderPass(cmd, &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    terrain_graphics_pipeline_);
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &terrain_mesh_.vertex_buffer_.buffer_,
                         &offset);
  vkCmdBindIndexBuffer(cmd, terrain_mesh_.index_buffer_.buffer_, offset,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd,
                   static_cast<std::uint32_t>(terrain_mesh_.indices_.size()), 1,
                   0, 0, 0);

  vkCmdEndRenderPass(cmd);
  vkEndCommandBuffer(cmd);

  static constexpr VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  const VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &current_frame_data.present_semaphore_,
      .pWaitDstStageMask = &wait_stage,
      .commandBufferCount = 1,
      .pCommandBuffers = &current_frame_data.main_command_buffer_,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &current_frame_data.render_semaphore_,
  };
  VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &submit_info,
                         current_frame_data.render_fence_));

  const VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &current_frame_data.render_semaphore_,
      .swapchainCount = 1,
      .pSwapchains = &swapchain_,
      .pImageIndices = &swapchain_image_index,
  };

  VK_CHECK(vkQueuePresentKHR(present_queue_, &present_info));

  ++frame_number_;
}

void App::load_mesh()
{
  terrain_mesh_.vertices_.resize(3);

  terrain_mesh_.vertices_[0].position = {1.f, 1.f, 0.0f};
  terrain_mesh_.vertices_[1].position = {-1.f, 1.f, 0.0f};
  terrain_mesh_.vertices_[2].position = {0.f, -1.f, 0.0f};

  terrain_mesh_.vertices_[0].color = {1.f, 0.f, 0.0f};
  terrain_mesh_.vertices_[1].color = {0.f, 1.f, 0.0f};
  terrain_mesh_.vertices_[2].color = {0.f, 1.f, 1.0f};

  terrain_mesh_.indices_ = {0, 1, 2};

  upload_mesh(terrain_mesh_);
}
void App::upload_mesh(Mesh& mesh)
{
  // vertex buffer
  {
    const VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = mesh.vertices_.size() * sizeof(Vertex),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };

    const VmaAllocationCreateInfo vma_alloc_info{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU};

    VK_CHECK(vmaCreateBuffer(allocator_, &buffer_create_info, &vma_alloc_info,
                             &mesh.vertex_buffer_.buffer_,
                             &mesh.vertex_buffer_.allocation_, nullptr));

    void* data = nullptr;
    vmaMapMemory(allocator_, mesh.vertex_buffer_.allocation_, &data);
    std::memcpy(data, terrain_mesh_.vertices_.data(),
                terrain_mesh_.vertices_.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator_, mesh.vertex_buffer_.allocation_);
  }

  // index buffer
  {
    const VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = mesh.indices_.size() * sizeof(std::uint32_t),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    };

    const VmaAllocationCreateInfo vma_alloc_info{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU};

    VK_CHECK(vmaCreateBuffer(allocator_, &buffer_create_info, &vma_alloc_info,
                             &mesh.index_buffer_.buffer_,
                             &mesh.index_buffer_.allocation_, nullptr));

    void* data = nullptr;
    vmaMapMemory(allocator_, mesh.index_buffer_.allocation_, &data);
    std::memcpy(data, terrain_mesh_.indices_.data(),
                terrain_mesh_.indices_.size() * sizeof(std::uint32_t));
    vmaUnmapMemory(allocator_, mesh.index_buffer_.allocation_);
  }
}
auto App::get_current_frame() -> FrameData&
{
  return frame_data_[frame_number_ % frames_in_flight];
}
