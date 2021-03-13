cmake_minimum_required(VERSION 3.14)

project(voxel_game LANGUAGES CXX)

include("cmake/StandardProjectSettings.cmake")
include("CMakeDependentOption")

include("cmake/compiler.cmake")
include("cmake/StaticAnalyzers.cmake")

include("cmake/conan.cmake")
run_conan()

add_subdirectory(third-party)
add_subdirectory(src)


#if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
#    include(CTest)
#endif ()
#
#if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
#    add_subdirectory(test)
#endif ()