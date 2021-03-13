add_subdirectory(beyond-core)

add_subdirectory(vk-bootstrap)

# vulkan memory allocator
add_library(vma INTERFACE)
target_compile_definitions(vma INTERFACE
        VMA_STATIC_VULKAN_FUNCTIONS=1)
target_include_directories(vma SYSTEM
        INTERFACE
        $<INSTALL_INTERFACE:VulkanMemoryAllocator/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/src>
        )
add_library(third_party::vma ALIAS vma)

# Imgui
add_library(imgui
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_glfw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_vulkan.cpp")
target_include_directories(imgui SYSTEM
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
        )

target_link_libraries(imgui CONAN_PKG::glfw)
add_library(third_party::imgui ALIAS imgui)