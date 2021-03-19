#include "app.hpp"

#include <VkBootstrap.h>
#include <fmt/format.h>

#include <beyond/utils/byte_size.hpp>
#include <beyond/utils/size.hpp>
#include <beyond/utils/to_pointer.hpp>

#include <beyond/math/transform.hpp>

#include "vulkan_helpers/graphics_pipeline.hpp"
#include "vulkan_helpers/shader_module.hpp"
#include "vulkan_helpers/sync.hpp"

#include "vulkan_helpers/debug_utils.hpp"
#include "vulkan_helpers/vk_check.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

namespace {

// clang-format off
static constexpr std::uint32_t edge_table[256] = {
    0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0 };
static constexpr std::int32_t tri_table[256][16] =
    {{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
     {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
     {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
     {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
     {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
     {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
     {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
     {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
     {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
     {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
     {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
     {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
     {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
     {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
     {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
     {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
     {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
     {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
     {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
     {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
     {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
     {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
     {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
     {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
     {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
     {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
     {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
     {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
     {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
     {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
     {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
     {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
     {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
     {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
     {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
     {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
     {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
     {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
     {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
     {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
     {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
     {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
     {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
     {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
     {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
     {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
     {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
     {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
     {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
     {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
     {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
     {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
     {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
     {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
     {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
     {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
     {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
     {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
     {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
     {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
     {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
     {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
     {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
     {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
     {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
     {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
     {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
     {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
     {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
     {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
     {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
     {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
     {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
     {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
     {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
     {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
     {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
     {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
     {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
     {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
     {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
     {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
     {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
     {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
     {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
     {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
     {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
     {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
     {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
     {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
     {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
     {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
     {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
     {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
     {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
     {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
     {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
     {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
     {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
     {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
     {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
     {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
     {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
     {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
     {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
     {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
     {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
     {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
     {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
     {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
     {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
     {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
     {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
     {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
     {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
     {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
     {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
     {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
     {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
     {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
     {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
     {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
     {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
     {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
     {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
     {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
     {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
     {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
     {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
     {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
     {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
     {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
     {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
     {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
     {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
     {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
     {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
     {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
     {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
     {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
     {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
     {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
     {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
     {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
     {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
     {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
     {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
     {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
     {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
     {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
     {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
     {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
     {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
     {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
     {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
     {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
     {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
     {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
     {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
     {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
     {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
     {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
     {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
     {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
     {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
     {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
     {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
     {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
     {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
     {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
     {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
     {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
     {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
     {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
     {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
     {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
     {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
     {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
     {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
     {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
// clang-format on

static constexpr int chunk_dimension = 32;

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

  destroy_buffer(context_, indirect_buffer_);
  destroy_buffer(context_, terrain_mesh_.vertex_buffer_);

  vkDestroyPipeline(context_.device(), terrain_wireframe_pipeline_, nullptr);
  vkDestroyPipeline(context_.device(), terrain_graphics_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_.device(), terrain_graphics_pipeline_layout_,
                          nullptr);

  vkDestroyRenderPass(context_.device(), render_pass_, nullptr);

  for (auto& frame_data : frame_data_) {
    destroy_buffer(context_, frame_data.camera_buffer);

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
  upload_context_.fence =
      vkh::create_fence(context_, {.debug_name = "Upload Fence"}).value();
  for (auto i = 0u; i < frames_in_flight; ++i) {
    auto& frame_data = frame_data_[i];
    frame_data.render_semaphore =
        vkh::create_semaphore(
            context_,
            {.debug_name = fmt::format("Render Semaphore ({})", i).c_str()})
            .value();
    frame_data.present_semaphore =
        vkh::create_semaphore(
            context_,
            {.debug_name = fmt::format("present Fence ({})", i).c_str()})
            .value();
    frame_data.render_fence =
        vkh::create_fence(context_, {.debug_name = "Upload Fence"}).value();
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

  static constexpr VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}};

  static constexpr VkDescriptorPoolCreateInfo pool_info = {
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
    frame_data.camera_buffer =
        vkh::create_buffer(context_,
                           {
                               .size = sizeof(GPUCameraData),
                               .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                           })
            .value();

    // allocate one descriptor set for each frame
    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &global_descriptor_set_layout_,
    };

    VK_CHECK(vkAllocateDescriptorSets(context_.device(), &alloc_info,
                                      &frame_data.global_descriptor));

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
      vkh::load_shader_module_from_file(context_, "shaders/terrain.vert.spv",
                                        {.debug_name = "Terrain Vertex Shader"})
          .expect("Cannot load terrain.vert.spv");

  auto terrain_frag_shader_module =
      vkh::load_shader_module_from_file(
          context_, "shaders/terrain.frag.spv",
          {.debug_name = "Terrain Fragment Shader"})
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
          context_,
          vkh::GraphicsPipelineCreateInfo{
              .pipeline_layout = terrain_graphics_pipeline_layout_,
              .render_pass = render_pass_,
              .window_extend = window_extent_,
              .debug_name = "Terrain Graphics Pipeline",
              .shader_stages = terrain_shader_stages,
              .cull_mode = vkh::CullMode::back})
          .expect("Failed to create terrain graphics pipeline");

  vkDestroyShaderModule(context_.device(), terrain_vert_shader_module, nullptr);
  vkDestroyShaderModule(context_.device(), terrain_frag_shader_module, nullptr);

  auto wireframe_vert_shader_module =
      vkh::load_shader_module_from_file(
          context_, "shaders/wireframe.vert.spv",
          {.debug_name = "Wireframe Vertex Shader"})
          .expect("Cannot load wireframe.vert.spv");

  auto wireframe_frag_shader_module =
      vkh::load_shader_module_from_file(
          context_, "shaders/wireframe.frag.spv",
          {.debug_name = "Wireframe Fragment Shader"})
          .expect("Cannot load wireframe.vert.spv");
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
          context_,
          vkh::GraphicsPipelineCreateInfo{
              .pipeline_layout = terrain_graphics_pipeline_layout_,
              .render_pass = render_pass_,
              .window_extend = window_extent_,
              .debug_name = "Terrain Wireframe Pipeline",
              .shader_stages = wireframe_shader_stages,
              .polygon_mode = vkh::PolygonMode::line})
          .expect("Failed to create terrain wireframe graphics pipeline");

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
  void* data = context_.map(current_frame_data.camera_buffer).value();
  memcpy(data, &camera_data, sizeof(GPUCameraData));
  context_.unmap(current_frame_data.camera_buffer);

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
  auto triangle_table_buffer =
      vkh::create_buffer_from_data(
          context_,
          {
              .size = sizeof(tri_table),
              .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
              .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
          },
          beyond::to_pointer(tri_table))
          .value();

  auto edge_table_buffer = vkh::create_buffer_from_data(
                               context_,
                               {
                                   .size = sizeof(edge_table),
                                   .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                               },
                               beyond::to_pointer(edge_table))
                               .value();

  static constexpr VkDrawIndirectCommand indirect_command{
      .vertexCount = 0,
      .instanceCount = 1,
      .firstVertex = 0,
      .firstInstance = 0,
  };
  indirect_buffer_ = vkh::create_buffer_from_data(
                         context_,
                         {
                             .size = sizeof(VkDrawIndirectCommand),
                             .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                         },
                         indirect_command)
                         .value();

  constexpr size_t max_triangles_per_cell = 5;
  constexpr size_t vertices_per_triangle = 3;
  constexpr size_t vertex_buffer_size =
      sizeof(Vertex) * max_triangles_per_cell * vertices_per_triangle *
      chunk_dimension * chunk_dimension * chunk_dimension * 10;
  terrain_mesh_.vertex_buffer_ =
      vkh::create_buffer(context_,
                         {
                             .size = vertex_buffer_size,
                             .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
                         })
          .value();

  constexpr uint32_t input_data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  auto input_buffer = vkh::create_buffer_from_data(
                          context_,
                          {
                              .size = beyond::byte_size(input_data),
                              .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                          },
                          beyond::to_pointer(input_data))
                          .value();

  static constexpr VkDescriptorSetLayoutBinding
      descriptor_set_layout_bindings[] = {
          {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
           nullptr},
          {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
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

  VkShaderModule module = vkh::load_shader_module_from_file(
                              context_, "shaders/terrain_meshing.comp.spv",
                              {.debug_name = "Terrain Meshing Compute Shader"})
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
  VK_CHECK(vkCreateComputePipelines(context_.device(), {}, 1,
                                    &compute_pipeline_create_info, nullptr,
                                    &compute_pipeline));
  VK_CHECK(vkh::set_debug_name(
      context_, beyond::bit_cast<uint64_t>(compute_pipeline),
      VK_OBJECT_TYPE_PIPELINE, "Terrian Meshing Pipeline"));

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
  const VkDescriptorBufferInfo out_descriptor_buffer_info = {
      terrain_mesh_.vertex_buffer_, 0, VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo edge_table_descriptor_buffer_info = {
      edge_table_buffer, 0, VK_WHOLE_SIZE};
  const VkDescriptorBufferInfo tri_table_descriptor_buffer_info = {
      triangle_table_buffer, 0, VK_WHOLE_SIZE};

  const VkWriteDescriptorSet write_descriptor_set[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
       &indirect_descriptor_buffer_info, nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 1, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &in_descriptor_buffer_info,
       nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &out_descriptor_buffer_info,
       nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 3, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
       &edge_table_descriptor_buffer_info, nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 4, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
       &tri_table_descriptor_buffer_info, nullptr}};
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

  constexpr std::uint32_t local_size = 4;
  const auto f = chunk_dimension / local_size;
  vkCmdDispatch(command_buffer, f, f, f);
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

  vkWaitForFences(context_.device(), 1, &compute_fence, true, 1e9);
  vkResetFences(context_.device(), 1, &compute_fence);
  vkResetCommandPool(context_.device(), compute_command_pool, 0);

  vkDestroyFence(context_.device(), compute_fence, nullptr);
  vkDestroyCommandPool(context_.device(), compute_command_pool, nullptr);
  vkDestroyShaderModule(context_.device(), module, nullptr);
  vkDestroyPipeline(context_.device(), compute_pipeline, nullptr);

  vkDestroyPipelineLayout(context_.device(), pipeline_layout, nullptr);
  vkDestroyDescriptorSetLayout(context_.device(), descriptor_set_layout,
                               nullptr);
  destroy_buffer(context_, input_buffer);
  destroy_buffer(context_, edge_table_buffer);
  destroy_buffer(context_, triangle_table_buffer);
}

auto App::get_current_frame() -> FrameData&
{
  return frame_data_[frame_number_ % frames_in_flight];
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