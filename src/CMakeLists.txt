find_package(Vulkan)

include(../cmake/CompileShader.cmake)
compile_shader(terrainVertShader
        SOURCE ${CMAKE_SOURCE_DIR}/shaders/terrain.vert.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/terrain.vert.spv
        )

compile_shader(terrainFragShader
        SOURCE ${CMAKE_SOURCE_DIR}/shaders/terrain.frag.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/terrain.frag.spv
        )

compile_shader(wireframeVertShader
        SOURCE ${CMAKE_SOURCE_DIR}/shaders/wireframe.vert.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/wireframe.vert.spv
        )

compile_shader(wireframeFragShader
        SOURCE ${CMAKE_SOURCE_DIR}/shaders/wireframe.frag.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/wireframe.frag.spv
        )

compile_shader(terrainMeshingShader
        SOURCE ${CMAKE_SOURCE_DIR}/shaders/terrain_meshing.comp.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/terrain_meshing.comp.spv
        )

add_library(common
        app.hpp
        app.cpp
        vulkan_helpers/shader_module.hpp
        vulkan_helpers/shader_module.cpp
        vulkan_helpers/vma_impl.cpp
        first_person_camera.hpp
        vertex.hpp
        window_helpers/window_manager.cpp
        window_helpers/window_manager.hpp
        window_helpers/window.cpp
        window_helpers/window.hpp
        vulkan_helpers/context.cpp
        vulkan_helpers/context.hpp
        vulkan_helpers/vk_check.hpp
        vulkan_helpers/graphics_pipeline.cpp
        vulkan_helpers/graphics_pipeline.hpp vulkan_helpers/deletion_queue.hpp vulkan_helpers/debug_utils.cpp vulkan_helpers/debug_utils.hpp)
target_link_libraries(common
        PUBLIC
        CONAN_PKG::fmt
        CONAN_PKG::glfw
        beyond::core
        Vulkan::Vulkan
        third_party::vma
        third_party::imgui
        PRIVATE
        compiler_options
        vk-bootstrap::vk-bootstrap)
target_include_directories(common PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_dependencies(common terrainVertShader)
add_dependencies(common terrainFragShader)
add_dependencies(common wireframeVertShader)
add_dependencies(common wireframeFragShader)
add_dependencies(common terrainMeshingShader)

add_executable(app "main.cpp")
target_link_libraries(app
        PRIVATE common compiler_options)
add_clangformat(app)
