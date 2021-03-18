#include "debug_utils.hpp"

#include "context.hpp"

#include <fmt/format.h>

namespace vkh {

[[nodiscard]] auto set_debug_name(Context& context, uint64_t object_handle,
                                  VkObjectType object_type,
                                  const char* name) noexcept -> VkResult
{
  const VkDebugUtilsObjectNameInfoEXT name_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = object_type,
      .objectHandle = object_handle,
      .pObjectName = name};
  return context.functions().setDebugUtilsObjectNameEXT(context.device(),
                                                        &name_info);
}

void report_fail_to_set_debug_name(const char* name) noexcept
{
  fmt::print("Cannot create debug name for {}\n", name);
}

} // namespace vkh
