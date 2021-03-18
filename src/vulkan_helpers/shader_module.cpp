#include "shader_module.hpp"

#include "context.hpp"
#include "debug_utils.hpp"

#include <beyond/utils/bit_cast.hpp>

#include <cstddef>
#include <fmt/format.h>
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

[[nodiscard]] auto
load_shader_module_from_file(Context& context, const std::string_view filename,
                             const ShaderModuleCreateInfo& create_info)
    -> beyond::expected<VkShaderModule, VkResult>
{
  const auto buffer = read_file(filename);

  const VkShaderModuleCreateInfo vk_create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = buffer.size(),
      .pCode = beyond::bit_cast<const uint32_t*>(buffer.data()),
  };

  VkShaderModule module{};
  if (VkResult result = vkCreateShaderModule(context.device(), &vk_create_info,
                                             nullptr, &module);
      result != VK_SUCCESS) {
    return beyond::unexpected(result);
  }

  if (set_debug_name(context, beyond::bit_cast<uint64_t>(module),
                     VK_OBJECT_TYPE_SHADER_MODULE, create_info.debug_name)) {
    fmt::print("Cannot create debug name for {}\n", create_info.debug_name);
  }
  return module;
}

} // namespace vkh
