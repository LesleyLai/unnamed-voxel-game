#include "app.hpp"

#include <VkBootstrap.h>
#include <fmt/format.h>

#include <beyond/utils/byte_size.hpp>
#include <beyond/utils/panic.hpp>
#include <beyond/utils/size.hpp>
#include <beyond/utils/to_pointer.hpp>

#include <beyond/math/transform.hpp>

#include "vulkan_helpers/shader_module.hpp"
#include "vulkan_helpers/vk_check.hpp"

namespace {

void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action,
                  int /*mods*/)
{
  auto* app = beyond::bit_cast<App*>(glfwGetWindowUserPointer(window));

  if (action == GLFW_REPEAT) {
    switch (key) {
    case GLFW_KEY_W:
      app->move_camera(FirstPersonCamera::Movement::FORWARD);
      break;
    case GLFW_KEY_A:
      app->move_camera(FirstPersonCamera::Movement::LEFT);
      break;
    case GLFW_KEY_S:
      app->move_camera(FirstPersonCamera::Movement::BACKWARD);
      break;
    case GLFW_KEY_D:
      app->move_camera(FirstPersonCamera::Movement::RIGHT);
      break;
    }
  }
}

static void cursor_position_callback(GLFWwindow* window, double xpos,
                                     double ypos)
{
  auto* app = beyond::bit_cast<App*>(glfwGetWindowUserPointer(window));
  if (app->dragging_status() != MouseDraggingState::No) {
    app->mouse_move(static_cast<float>(xpos), static_cast<float>(ypos));
  }
}

void mouse_button_callback(GLFWwindow* window, int button, int action,
                           int /*mods*/)
{
  auto* app = beyond::bit_cast<App*>(glfwGetWindowUserPointer(window));
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    if (action == GLFW_PRESS) {
      app->mouse_dragging(true);
    } else if (action == GLFW_RELEASE) {
      app->mouse_dragging(false);
    }
  }
}

} // namespace

App::App() : window_manager_{&WindowManager::instance()}
{
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  static constexpr int window_width = 1400;
  static constexpr int window_height = 900;
  window_ = Window(window_width, window_height, "Voxel Game");
  window_extent_ = VkExtent2D{static_cast<std::uint32_t>(window_width),
                              static_cast<std::uint32_t>(window_height)};

  glfwMakeContextCurrent(window_.glfw_window());

  glfwSetKeyCallback(window_.glfw_window(), key_callback);
  glfwSetWindowUserPointer(window_.glfw_window(), this);
  glfwSetCursorPosCallback(window_.glfw_window(), cursor_position_callback);
  glfwSetMouseButtonCallback(window_.glfw_window(), mouse_button_callback);

  context_ = vkh::Context(window_);

  init_swapchain();
  init_command();
  init_render_pass();
  init_framebuffer();
  init_sync_strucures();
  init_descriptors();
  init_pipeline();
  load_mesh();
}

App::~App()
{
  if (!context_) { return; }

  context_.wait_idle();

  //  vmaDestroyBuffer(context_.allocator(), terrain_mesh_.index_buffer_.buffer,
  //                   terrain_mesh_.index_buffer_.allocation);
  vmaDestroyBuffer(context_.allocator(), terrain_mesh_.vertex_buffer_.buffer,
                   terrain_mesh_.vertex_buffer_.allocation);

  vkDestroyPipeline(context_.device(), terrain_graphics_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_.device(), terrain_graphics_pipeline_layout_,
                          nullptr);

  vkDestroyRenderPass(context_.device(), render_pass_, nullptr);

  for (auto& frame_data : frame_data_) {

    vmaDestroyBuffer(context_.allocator(), frame_data.camera_buffer.buffer,
                     frame_data.camera_buffer.allocation);

    vkDestroyFence(context_.device(), frame_data.render_fence, nullptr);
    vkDestroySemaphore(context_.device(), frame_data.render_semaphore, nullptr);
    vkDestroySemaphore(context_.device(), frame_data.present_semaphore,
                       nullptr);
    vkDestroyCommandPool(context_.device(), frame_data.command_pool, nullptr);
  }

  vkDestroyDescriptorPool(context_.device(), descriptor_pool_, nullptr);
  vkDestroyDescriptorSetLayout(context_.device(), global_descriptor_set_layout_,
                               nullptr);

  for (auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(context_.device(), framebuffer, nullptr);
  }
  for (auto& image_view : swapchain_image_views_) {
    vkDestroyImageView(context_.device(), image_view, nullptr);
  }
  vkDestroyImageView(context_.device(), depth_image_view_, nullptr);
  vmaDestroyImage(context_.allocator(), depth_image_.image,
                  depth_image_.allocation);
  vkDestroySwapchainKHR(context_.device(), swapchain_, nullptr);
}

void App::move_camera(FirstPersonCamera::Movement movement)
{
  camera_.process_keyboard(movement, 0.1f);
}

void App::mouse_dragging(bool is_dragging)
{
  dragging_ = is_dragging ? MouseDraggingState::Start : MouseDraggingState::No;
}

void App::mouse_move(float x, float y)
{
  if (dragging_ == MouseDraggingState::Start) {
    last_mouse_x_ = x;
    last_mouse_y_ = y;
    dragging_ = MouseDraggingState::Dragging;
  }

  camera_.process_mouse_movement(x - last_mouse_x_, y - last_mouse_y_);
  last_mouse_x_ = x;
  last_mouse_y_ = y;
}

void App::exec()
{
  while (!window_.should_close()) {
    render();

    window_.swap_buffers();
    window_manager_->pull_events();
  }
}

void App::init_swapchain()
{
  vkb::SwapchainBuilder swapchain_builder{
      context_.physical_device(), context_.device(), context_.surface()};

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
  vmaCreateImage(context_.allocator(), &depth_image_create_info,
                 &depth_image_allocation_create_info, &depth_image_.image,
                 &depth_image_.allocation, nullptr);

  const VkImageViewCreateInfo depth_view_create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .image = depth_image_.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth_image_format_,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }};
  vkCreateImageView(context_.device(), &depth_view_create_info, nullptr,
                    &depth_image_view_);
}

void App::init_command()
{
  const VkCommandPoolCreateInfo command_pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context_.graphics_queue_family_index(),
  };

  for (auto& frame_data : frame_data_) {
    VK_CHECK(vkCreateCommandPool(context_.device(), &command_pool_create_info,
                                 nullptr, &frame_data.command_pool));

    const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = frame_data.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(context_.device(),
                                      &command_buffer_allocate_info,
                                      &frame_data.main_command_buffer));
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

  VK_CHECK(vkCreateRenderPass(context_.device(), &render_pass_create_info,
                              nullptr, &render_pass_));
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
    VK_CHECK(vkCreateFramebuffer(context_.device(), &framebuffer_create_info,
                                 nullptr, &framebuffers_[i]));
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
    VK_CHECK(vkCreateSemaphore(context_.device(), &semaphore_create_info,
                               nullptr, &frame_Data.render_semaphore));
    VK_CHECK(vkCreateSemaphore(context_.device(), &semaphore_create_info,
                               nullptr, &frame_Data.present_semaphore));
    VK_CHECK(vkCreateFence(context_.device(), &fence_create_info, nullptr,
                           &frame_Data.render_fence));
  }
}

void App::init_descriptors()
{
  static constexpr VkDescriptorSetLayoutBinding camera_buffer_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };

  static constexpr VkDescriptorSetLayoutCreateInfo set_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = 1,
      .pBindings = &camera_buffer_binding,
  };

  constexpr VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}};

  const VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = 0,
      .maxSets = 10,
      .poolSizeCount = beyond::size(pool_sizes),
      .pPoolSizes = beyond::to_pointer(pool_sizes),
  };

  vkCreateDescriptorPool(context_.device(), &pool_info, nullptr,
                         &descriptor_pool_);

  VK_CHECK(vkCreateDescriptorSetLayout(context_.device(),
                                       &set_layout_create_info, nullptr,
                                       &global_descriptor_set_layout_));

  for (auto& frame_data : frame_data_) {
    frame_data.camera_buffer = create_buffer(BufferCreateInfo{
        .size = sizeof(GPUCameraData),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });

    // allocate one descriptor set for each frame
    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &global_descriptor_set_layout_,
    };

    vkAllocateDescriptorSets(context_.device(), &alloc_info,
                             &frame_data.global_descriptor);

    const VkDescriptorBufferInfo buffer_info = {
        .buffer = frame_data.camera_buffer.buffer,
        .offset = 0,
        .range = sizeof(GPUCameraData),
    };

    const VkWriteDescriptorSet write_set = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = frame_data.global_descriptor,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buffer_info,
    };

    vkUpdateDescriptorSets(context_.device(), 1, &write_set, 0, nullptr);
  }
}

void App::init_pipeline()
{
  auto terrain_vert_ret =
      vkh::create_shader_module(context_.device(), "shaders/terrain.vert.spv");
  auto terrain_frag_ret =
      vkh::create_shader_module(context_.device(), "shaders/terrain.frag.spv");
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

  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &global_descriptor_set_layout_,
      .pushConstantRangeCount = 0,
  };

  if (vkCreatePipelineLayout(context_.device(), &pipeline_layout_info, nullptr,
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

  VK_CHECK(vkCreateGraphicsPipelines(context_.device(), VK_NULL_HANDLE, 1,
                                     &pipeline_create_info, nullptr,
                                     &terrain_graphics_pipeline_));

  vkDestroyShaderModule(context_.device(), terrain_vert_shader_module, nullptr);
  vkDestroyShaderModule(context_.device(), terrain_frag_shader_module, nullptr);
}

void App::render()
{
  auto current_frame_data = get_current_frame();

  const float aspect_ratio = static_cast<float>(window_extent_.width) /
                             static_cast<float>(window_extent_.height);
  const beyond::Mat4 view = camera_.get_view_matrix();
  beyond::Mat4 projection =
      beyond::perspective(beyond::Degree(60.f), aspect_ratio, 0.1f, 200.0f);
  projection[1][1] *= -1;

  // fill a GPU camera data struct
  const GPUCameraData camera_data = {
      .view = view,
      .proj = projection,
      .viewproj = projection * view,
  };

  // and copy it to the buffer
  void* data = nullptr;
  vmaMapMemory(context_.allocator(),
               current_frame_data.camera_buffer.allocation, &data);
  memcpy(data, &camera_data, sizeof(GPUCameraData));
  vmaUnmapMemory(context_.allocator(),
                 current_frame_data.camera_buffer.allocation);

  static constexpr std::uint64_t time_out = 1e9;
  VK_CHECK(vkWaitForFences(context_.device(), 1,
                           &current_frame_data.render_fence, true, time_out));
  VK_CHECK(
      vkResetFences(context_.device(), 1, &current_frame_data.render_fence));

  uint32_t swapchain_image_index = 0;
  VK_CHECK(vkAcquireNextImageKHR(context_.device(), swapchain_, time_out,
                                 current_frame_data.present_semaphore, nullptr,
                                 &swapchain_image_index));
  VK_CHECK(vkResetCommandBuffer(current_frame_data.main_command_buffer, 0));

  VkCommandBuffer cmd = current_frame_data.main_command_buffer;
  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  static constexpr VkClearValue clear_value = {
      .color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
  static constexpr VkClearValue depth_clear_value = {
      .depthStencil = {.depth = 1.f}};

  static constexpr VkClearValue clear_values[] = {clear_value,
                                                  depth_clear_value};

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
  static constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &terrain_mesh_.vertex_buffer_.buffer,
                         &offset);
  //  vkCmdBindIndexBuffer(cmd, terrain_mesh_.index_buffer_.buffer, offset,
  //                       VK_INDEX_TYPE_UINT32);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          terrain_graphics_pipeline_layout_, 0, 1,
                          &current_frame_data.global_descriptor, 0, nullptr);
  vkCmdDraw(cmd, static_cast<std::uint32_t>(terrain_mesh_.vertices_.size()), 1,
            0, 0);

  vkCmdEndRenderPass(cmd);
  vkEndCommandBuffer(cmd);

  static constexpr VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  const VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &current_frame_data.present_semaphore,
      .pWaitDstStageMask = &wait_stage,
      .commandBufferCount = 1,
      .pCommandBuffers = &current_frame_data.main_command_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &current_frame_data.render_semaphore,
  };
  VK_CHECK(vkQueueSubmit(context_.graphics_queue(), 1, &submit_info,
                         current_frame_data.render_fence));

  const VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &current_frame_data.render_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain_,
      .pImageIndices = &swapchain_image_index,
  };

  VK_CHECK(vkQueuePresentKHR(context_.present_queue(), &present_info));

  ++frame_number_;
}

void App::load_mesh()
{
  terrain_mesh_.vertices_ = generate_chunk_mesh();
  upload_mesh(terrain_mesh_);
}

void App::upload_mesh(Mesh& mesh)
{
  mesh.vertex_buffer_ = create_buffer_from_data(
      BufferCreateInfo{
          .size = beyond::byte_size(mesh.vertices_),
          .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      },
      mesh.vertices_.data());

  //  mesh.index_buffer_ = create_buffer_from_data(
  //      BufferCreateInfo{
  //          .size = beyond::byte_size(mesh.indices_),
  //          .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
  //          .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
  //      },
  //      mesh.indices_.data());
}

auto App::get_current_frame() -> FrameData&
{
  return frame_data_[frame_number_ % frames_in_flight];
}
auto App::create_buffer(const BufferCreateInfo& buffer_create_info)
    -> AllocatedBuffer
{
  const VkBufferCreateInfo vk_buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = buffer_create_info.size,
      .usage = buffer_create_info.usage,
  };

  const VmaAllocationCreateInfo vma_alloc_info{
      .usage = buffer_create_info.memory_usage};

  AllocatedBuffer allocated_buffer;
  VK_CHECK(vmaCreateBuffer(context_.allocator(), &vk_buffer_create_info,
                           &vma_alloc_info, &allocated_buffer.buffer,
                           &allocated_buffer.allocation, nullptr));
  return allocated_buffer;
}

auto App::create_buffer_from_data(const BufferCreateInfo& buffer_create_info,
                                  void* data) -> AllocatedBuffer
{
  AllocatedBuffer buffer = create_buffer(buffer_create_info);
  void* mapped_ptr = nullptr;
  vmaMapMemory(context_.allocator(), buffer.allocation, &mapped_ptr);
  std::memcpy(mapped_ptr, data, buffer_create_info.size);
  vmaUnmapMemory(context_.allocator(), buffer.allocation);
  return buffer;
}
