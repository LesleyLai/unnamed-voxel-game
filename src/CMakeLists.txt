find_package(Vulkan)

add_library(common
        app.hpp
        app.cpp)
target_link_libraries(common
        PUBLIC
        CONAN_PKG::fmt
        CONAN_PKG::glfw
        Vulkan::Vulkan
        PRIVATE
        compiler_options
        vk-bootstrap::vk-bootstrap)
target_include_directories(common PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_executable(app "main.cpp")
target_link_libraries(app
        PRIVATE common compiler_options)
add_clangformat(app)
