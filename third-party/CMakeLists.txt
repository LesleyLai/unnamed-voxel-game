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