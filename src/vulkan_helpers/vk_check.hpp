#ifndef VOXEL_GAME_VULKAN_VK_CHECK_HPP
#define VOXEL_GAME_VULKAN_VK_CHECK_HPP

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) { fmt::format("Vulkan error: {}\n", err); }                       \
  } while (0)

#endif // VOXEL_GAME_VULKAN_VK_CHECK_HPP
