#include "app.hpp"

#include <VkBootstrap.h>
#include <fmt/format.h>

#include <beyond/utils/byte_size.hpp>
#include <beyond/utils/size.hpp>
#include <beyond/utils/to_pointer.hpp>

#include <beyond/math/transform.hpp>

#include "vulkan_helpers/graphics_pipeline.hpp"
#include "vulkan_helpers/shader_module.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

namespace {

void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action,
                  int /*mods*/)
{
  auto* app = beyond::bit_cast<App*>(glfwGetWindowUserPointer(window));

  switch (action) {
  case GLFW_REPEAT:
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
    default:
      break;
    }
    break;
  default:
    break;
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
  deletion_queue_ = vkh::DeletionQueue(context_);

  init_swapchain();
  init_command();
  init_render_pass();
  init_framebuffer();
  init_sync_strucures();
  init_imgui();
  init_descriptors();
  init_pipeline();
  generate_mesh();
}

App::~App()
{
  if (!context_) { return; }

  context_.wait_idle();

  vkDestroyFence(context_.device(), upload_context_.fence, nullptr);
  vkDestroyCommandPool(context_.device(), upload_context_.command_pool,
                       nullptr);

  destroy_allocated_buffer(context_, indirect_buffer_);
  destroy_allocated_buffer(context_, terrain_mesh_.vertex_buffer_);

  vkDestroyPipeline(context_.device(), terrain_wireframe_pipeline_, nullptr);
  vkDestroyPipeline(context_.device(), terrain_graphics_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_.device(), terrain_graphics_pipeline_layout_,
                          nullptr);

  vkDestroyRenderPass(context_.device(), render_pass_, nullptr);

  for (auto& frame_data : frame_data_) {
    destroy_allocated_buffer(context_, frame_data.camera_buffer);

    vkDestroyFence(context_.device(), frame_data.render_fence, nullptr);
    vkDestroySemaphore(context_.device(), frame_data.render_semaphore, nullptr);
    vkDestroySemaphore(context_.device(), frame_data.present_semaphore,
                       nullptr);
    vkDestroyCommandPool(context_.device(), frame_data.command_pool, nullptr);
  }

  ImGui_ImplVulkan_Shutdown();

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
  VK_CHECK(vmaCreateImage(context_.allocator(), &depth_image_create_info,
                          &depth_image_allocation_create_info,
                          &depth_image_.image, &depth_image_.allocation,
                          nullptr));

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
  VK_CHECK(vkCreateImageView(context_.device(), &depth_view_create_info,
                             nullptr, &depth_image_view_));
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

  const VkCommandPoolCreateInfo upload_command_pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context_.graphics_queue_family_index(),
  };
  // create pool for upload context
  VK_CHECK(vkCreateCommandPool(context_.device(),
                               &upload_command_pool_create_info, nullptr,
                               &upload_context_.command_pool));
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
  static constexpr VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  static constexpr VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  VK_CHECK(vkCreateFence(context_.device(), &fence_create_info, nullptr,
                         &upload_context_.fence));

  for (auto& frame_Data : frame_data_) {
    VK_CHECK(vkCreateSemaphore(context_.device(), &semaphore_create_info,
                               nullptr, &frame_Data.render_semaphore));
    VK_CHECK(vkCreateSemaphore(context_.device(), &semaphore_create_info,
                               nullptr, &frame_Data.present_semaphore));
    VK_CHECK(vkCreateFence(context_.device(), &fence_create_info, nullptr,
                           &frame_Data.render_fence));
  }
}

void App::init_imgui()
{
  static constexpr VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  static constexpr VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1000,
      .poolSizeCount = beyond::size(pool_sizes),
      .pPoolSizes = beyond::to_pointer(pool_sizes),
  };

  VkDescriptorPool imgui_pool{};
  VK_CHECK(vkCreateDescriptorPool(context_.device(), &pool_info, nullptr,
                                  &imgui_pool));
  deletion_queue_.push([=](VkDevice device) {
    vkDestroyDescriptorPool(device, imgui_pool, nullptr);
  });

  ImGui::CreateContext();

  // this initializes imgui for SDL
  ImGui_ImplGlfw_InitForVulkan(window_.glfw_window(), false);

  // this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = context_.instance(),
      .PhysicalDevice = context_.physical_device(),
      .Device = context_.device(),
      .Queue = context_.graphics_queue(),
      .DescriptorPool = imgui_pool,
      .MinImageCount = 3,
      .ImageCount = 3,
  };
  ImGui_ImplVulkan_Init(&init_info, render_pass_);

  // execute a gpu command to upload imgui font textures
  immediate_submit(
      [&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

  // clear font textures from cpu data
  ImGui_ImplVulkan_DestroyFontUploadObjects();
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
  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &global_descriptor_set_layout_,
      .pushConstantRangeCount = 0,
  };

  VK_CHECK(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info,
                                  nullptr, &terrain_graphics_pipeline_layout_));

  auto terrain_vert_shader_module =
      vkh::create_shader_module(context_.device(), "shaders/terrain.vert.spv")
          .expect("Cannot load terrain.vert.spv");
  auto terrain_frag_shader_module =
      vkh::create_shader_module(context_.device(), "shaders/terrain.frag.spv")
          .expect("Cannot load terrain.frag.spv");

  const VkPipelineShaderStageCreateInfo terrain_shader_stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = terrain_vert_shader_module,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = terrain_frag_shader_module,
       .pName = "main"}};

  terrain_graphics_pipeline_ =
      vkh::create_graphics_pipeline(
          context_.device(),
          vkh::GraphicsPipelineCreateInfo{
              .pipeline_layout = terrain_graphics_pipeline_layout_,
              .render_pass = render_pass_,
              .window_extend = window_extent_,
              .shader_stages = terrain_shader_stages,
              .cull_mode = vkh::CullMode::back})
          .expect("Failed to create terrain graphics pipeline");
  VK_CHECK(vkh::set_debug_name(context_, terrain_graphics_pipeline_,
                               "Terrain Graphics Pipeline"));

  vkDestroyShaderModule(context_.device(), terrain_vert_shader_module, nullptr);
  vkDestroyShaderModule(context_.device(), terrain_frag_shader_module, nullptr);

  auto wireframe_vert_ret = vkh::create_shader_module(
      context_.device(), "shaders/wireframe.vert.spv");
  auto wireframe_frag_ret = vkh::create_shader_module(
      context_.device(), "shaders/wireframe.frag.spv");
  auto wireframe_vert_shader_module =
      wireframe_vert_ret.expect("Cannot load wireframe.vert.spv");
  auto wireframe_frag_shader_module =
      wireframe_frag_ret.expect("Cannot load wireframe.vert.spv");
  const VkPipelineShaderStageCreateInfo wireframe_shader_stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = wireframe_vert_shader_module,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = wireframe_frag_shader_module,
       .pName = "main"}};

  terrain_wireframe_pipeline_ =
      vkh::create_graphics_pipeline(
          context_.device(),
          vkh::GraphicsPipelineCreateInfo{
              .pipeline_layout = terrain_graphics_pipeline_layout_,
              .render_pass = render_pass_,
              .window_extend = window_extent_,
              .shader_stages = wireframe_shader_stages,
              .polygon_mode = vkh::PolygonMode::line})
          .expect("Failed to create terrain wireframe graphics pipeline");
  VK_CHECK(vkh::set_debug_name(context_, terrain_wireframe_pipeline_,
                               "Terrain Wireframe Pipeline"));

  vkDestroyShaderModule(context_.device(), wireframe_vert_shader_module,
                        nullptr);
  vkDestroyShaderModule(context_.device(), wireframe_frag_shader_module,
                        nullptr);
}

void App::render_gui()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Options");

  ImGui::Text("Render Mode:");

  int render_mode_int = static_cast<int>(render_mode_);
  ImGui::RadioButton("Faces", &render_mode_int, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Wireframe", &render_mode_int, 1);
  render_mode_ = static_cast<RenderMode>(render_mode_int);

  ImGui::End();

  // imgui commands
  ImGui::Render();
}

void App::render()
{
  render_gui();
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
  void* data = current_frame_data.camera_buffer.map(context_);
  memcpy(data, &camera_data, sizeof(GPUCameraData));
  current_frame_data.camera_buffer.unmap(context_);

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

  switch (render_mode_) {
  case RenderMode::Fill:
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      terrain_graphics_pipeline_);
    break;
  case RenderMode::Wireframe:
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      terrain_wireframe_pipeline_);
    break;
  };
  static constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &terrain_mesh_.vertex_buffer_.buffer,
                         &offset);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          terrain_graphics_pipeline_layout_, 0, 1,
                          &current_frame_data.global_descriptor, 0, nullptr);
  vkCmdDrawIndirect(cmd, indirect_buffer_, 0, 1, 0);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

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

void App::generate_mesh()
{
  indirect_buffer_ = create_buffer({
      .size = sizeof(VkDrawIndirectCommand),
      .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
  });

  constexpr uint32_t input_data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  auto input_buffer = create_buffer_from_data(
      BufferCreateInfo{
          .size = beyond::byte_size(input_data),
          .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      },
      beyond::to_pointer(input_data));
  auto output_buffer = create_buffer({BufferCreateInfo{
      .size = beyond::byte_size(input_data),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .memory_usage = VMA_MEMORY_USAGE_GPU_TO_CPU,
  }});

  static constexpr VkDescriptorSetLayoutBinding
      descriptor_set_layout_bindings[] = {
          {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr}};

  static constexpr VkDescriptorSetLayoutCreateInfo
      descriptorSetLayoutCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .bindingCount = beyond::size(descriptor_set_layout_bindings),
          .pBindings = beyond::to_pointer(descriptor_set_layout_bindings)};

  VkDescriptorSetLayout descriptor_set_layout{};
  vkCreateDescriptorSetLayout(context_.device(), &descriptorSetLayoutCreateInfo,
                              nullptr, &descriptor_set_layout);

  const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout,
  };
  VkPipelineLayout pipeline_layout{};
  vkCreatePipelineLayout(context_.device(), &pipeline_layout_create_info,
                         nullptr, &pipeline_layout);

  VkShaderModule module =
      vkh::create_shader_module(context_.device(),
                                "shaders/terrain_meshing.comp.spv")
          .expect("Cannot load terrain_meshing.comp.spv");

  const VkComputePipelineCreateInfo compute_pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage =
          VkPipelineShaderStageCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = module,
              .pName = "main",
          },
      .layout = pipeline_layout,
  };
  VkPipeline compute_pipeline = {};
  vkCreateComputePipelines(context_.device(), {}, 1,
                           &compute_pipeline_create_info, nullptr,
                           &compute_pipeline);

  const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptor_pool_,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptor_set_layout,
  };
  VkDescriptorSet descriptor_set = {};
  vkAllocateDescriptorSets(context_.device(), &descriptor_set_allocate_info,
                           &descriptor_set);

  const VkDescriptorBufferInfo indirect_descriptor_buffer_info = {
      indirect_buffer_, 0, VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo in_descriptor_buffer_info = {input_buffer, 0,
                                                            VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo out_descriptor_buffer_info = {output_buffer, 0,
                                                             VK_WHOLE_SIZE};

  const VkWriteDescriptorSet write_descriptor_set[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
       &indirect_descriptor_buffer_info, nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 1, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &in_descriptor_buffer_info,
       nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &out_descriptor_buffer_info,
       nullptr}};
  vkUpdateDescriptorSets(context_.device(), beyond::size(write_descriptor_set),
                         beyond::to_pointer(write_descriptor_set), 0, nullptr);

  const VkCommandPoolCreateInfo compute_command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = context_.compute_queue_family_index()};
  VkCommandPool compute_command_pool = {};
  vkCreateCommandPool(context_.device(), &compute_command_pool_create_info,
                      nullptr, &compute_command_pool);

  const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
      compute_command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

  VkCommandBuffer command_buffer{};
  VK_CHECK(vkAllocateCommandBuffers(
      context_.device(), &command_buffer_allocate_info, &command_buffer));

  static constexpr VkCommandBufferBeginInfo command_buffer_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

  vkCmdDispatch(command_buffer, beyond::size(input_data), 1, 1);
  VK_CHECK(vkEndCommandBuffer(command_buffer));

  const VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
  };

  static constexpr VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VkFence compute_fence = {};
  vkCreateFence(context_.device(), &fence_create_info, nullptr, &compute_fence);

  VK_CHECK(
      vkQueueSubmit(context_.compute_queue(), 1, &submit_info, compute_fence));

  vkWaitForFences(context_.device(), 1, &compute_fence, true, 9999999999);
  vkResetFences(context_.device(), 1, &compute_fence);
  vkResetCommandPool(context_.device(), compute_command_pool, 0);

  vkDestroyFence(context_.device(), compute_fence, nullptr);
  vkDestroyCommandPool(context_.device(), compute_command_pool, nullptr);
  vkDestroyShaderModule(context_.device(), module, nullptr);
  vkDestroyPipeline(context_.device(), compute_pipeline, nullptr);

  const auto* output_ptr = output_buffer.map<uint32_t>(context_);
  if (!std::equal(beyond::to_pointer(input_data),
                  beyond::to_pointer(input_data) + beyond::size(input_data),
                  output_ptr)) {
    fmt::print(stderr, "Compute shader does not work!\n");
    std::fflush(stderr);
  }
  output_buffer.unmap(context_);

  vkDestroyPipelineLayout(context_.device(), pipeline_layout, nullptr);
  vkDestroyDescriptorSetLayout(context_.device(), descriptor_set_layout,
                               nullptr);
  destroy_allocated_buffer(context_, input_buffer);
  destroy_allocated_buffer(context_, output_buffer);

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
                                  const void* data) -> AllocatedBuffer
{
  AllocatedBuffer buffer = create_buffer(buffer_create_info);
  void* mapped_ptr = buffer.map(context_);
  std::memcpy(mapped_ptr, data, buffer_create_info.size);
  buffer.unmap(context_);
  return buffer;
}

void App::immediate_submit(
    beyond::function_ref<void(VkCommandBuffer cmd)> function)
{
  const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = upload_context_.command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer cmd = {};
  VK_CHECK(vkAllocateCommandBuffers(context_.device(),
                                    &command_buffer_allocate_info, &cmd));

  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  // execute the function
  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  const VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                 .commandBufferCount = 1,
                                 .pCommandBuffers = &cmd};

  // submit command buffer to the queue and execute it.
  // _uploadFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit(context_.graphics_queue(), 1, &submit_info,
                         upload_context_.fence));

  vkWaitForFences(context_.device(), 1, &upload_context_.fence, true,
                  9999999999);
  vkResetFences(context_.device(), 1, &upload_context_.fence);

  // clear the command pool. This will free the command buffer too
  vkResetCommandPool(context_.device(), upload_context_.command_pool, 0);
}