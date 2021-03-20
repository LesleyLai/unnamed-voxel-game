#ifndef VOXEL_GAME_TERRAIN_MARCHING_CUBE_TABLES_HPP
#define VOXEL_GAME_TERRAIN_MARCHING_CUBE_TABLES_HPP

#include "../vulkan_helpers/buffer.hpp"

auto generate_edge_table_buffer(vkh::Context& context)
    -> vkh::Expected<vkh::Buffer>;

auto generate_triangle_table_buffer(vkh::Context& context)
    -> vkh::Expected<vkh::Buffer>;

#endif // VOXEL_GAME_TERRAIN_MARCHING_CUBE_TABLES_HPP
