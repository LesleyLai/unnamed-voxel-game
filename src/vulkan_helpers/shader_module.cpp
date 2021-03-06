#include "shader_module.hpp"

#include <beyond/utils/bit_cast.hpp>

#include <cstddef>
#include <fstream>

namespace {

[[nodiscard]] auto read_file(const std::string_view filename)
    -> std::vector<char>
{

  std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    // TODO(lesley): error handling
    beyond::panic("Failed to open file " + std::string(filename));
  }

  size_t file_size = static_cast<size_t>(file.tellg());
  std::vector<char> buffer;
  buffer.resize(file_size);

  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(file_size));

  return buffer;
}

} // anonymous namespace

namespace vkh {

[[nodiscard]] auto create_shader_module(VkDevice device,
                                        const std::string_view filename)
    -> beyond::expected<VkShaderModule, VkResult>
{
  const auto buffer = read_file(filename);

  const VkShaderModuleCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = buffer.size(),
      .pCode = beyond::bit_cast<const uint32_t*>(buffer.data()),
  };

  VkShaderModule module = nullptr;
  if (VkResult result =
          vkCreateShaderModule(device, &create_info, nullptr, &module);
      result != VK_SUCCESS) {
    return beyond::unexpected(result);
  } else {
    return module;
  }
}

} // namespace vkh
