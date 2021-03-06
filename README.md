# Unnamed Voxel Game

### Unit test

This boilerplate uses [Catch2](https://github.com/catchorg/Catch2) as the Unit Test Framework. The CMake
option `VOXEL_GAME_BUILD_TESTS` (`ON` by default) enables the building of unit test.

The CMake option `VOXEL_GAME_BUILD_TESTS_COVERAGE` (`OFF` by default) enables the test coverage with `gcov` and `lcov`.
To make the coverage web report working, you need a [codecov](https://codecov.io/) account. And you also need to
substitute the `CODECOV_TOKEN` in `.travis.yml` to your own.

### Compiler warning and sanitizers

Every time you add a new target, you need to enable the warnings and sanitizers on that target. Please write

```cmake
target_link_libraries(${TEST_TARGET_NAME} PRIVATE compiler_options)
```

This project enables a reasonable amount of warnings across compilers GCC, Clang, and MSVC. The
option `VOXEL_GAME_WARNING_AS_ERROR` treats warnings as errors. You can modify `cmake/compiler.cmake` to customize
warning settings.

We can optionally enable compiler [sanitizers](https://github.com/google/sanitizers) for the project. Sanitizers are
run-time checks that catch common bugs. Not all compilers support all the sanitizers, and enabling non-supported
sanitizer triggers either a compile-time error or warning. The option `VOXEL_GAME_USE_ASAN` enables the address
sanitizer;
`VOXEL_GAME_USE_TSAN` enables the thread sanitizer;
`VOXEL_GAME_USE_MSAN` enables the memory sanitizer;
`VOXEL_GAME_USE_UBSAN` enables the undefined behavior sanitizer.

## All the CMake Options

- `BUILD_TESTING` (`ON` by default) enables the building of unit test
- `VOXEL_GAME_BUILD_TESTS_COVERAGE` (`OFF` by default) enables the test coverage with `gcov` and `lcov`
- `VOXEL_GAME_WARNING_AS_ERROR` (`ON` by default) treats warnings as errors
- `VOXEL_GAME_USE_ASAN` (`OFF` by default) enables the address sanitizer
- `VOXEL_GAME_USE_TSAN` (`OFF` by default) enables the thread sanitizer
- `VOXEL_GAME_USE_MSAN` (`OFF` by default) enables the memory sanitizer
- `VOXEL_GAME_USE_UBSAN` (`OFF` by default) enables the undefined behavior sanitizer
- `VOXEL_GAME_ENABLE_IPO`  (`OFF` by default) enables Interprocedural optimization, aka Link Time Optimization
- `VOXEL_GAME_ENABLE_CPPCHECK` (`OFF` by default) Enable static analysis with cppcheck
- `VOXEL_GAME_ENABLE_CLANG_TIDY` (`OFF` by default) Enable static analysis with clang-tidy
