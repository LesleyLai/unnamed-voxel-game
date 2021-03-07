find_package(Vulkan)

include(../cmake/CompileShader.cmake)
compile_shader(vertShader
        SOURCE ${CMAKE_SOURCE_DIR}/shaders/terrain.vert.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/terrain.vert.spv
        )

compile_shader(fragShader
        SOURCE ${CMAKE_SOURCE_DIR}/shaders/terrain.frag.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/terrain.frag.spv
        )

add_library(common
        app.hpp
        app.cpp
        vulkan_helpers/shader_module.hpp
        vulkan_helpers/shader_module.cpp
        vulkan_helpers/vma_impl.cpp
        first_person_camera.hpp)
target_link_libraries(common
        PUBLIC
        CONAN_PKG::fmt
        CONAN_PKG::glfw
        beyond::core
        Vulkan::Vulkan
        third_party::vma
        PRIVATE
        compiler_options
        vk-bootstrap::vk-bootstrap)
target_include_directories(common PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_dependencies(common vertShader)
add_dependencies(common fragShader)

add_executable(app "main.cpp")
target_link_libraries(app
        PRIVATE common compiler_options)
add_clangformat(app)
