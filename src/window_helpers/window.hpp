#ifndef VOXEL_GAME_WINDOW_HPP
#define VOXEL_GAME_WINDOW_HPP

#include <utility>

#include <beyond/utils/force_inline.hpp>

struct GLFWwindow;

class Window {
public:
  Window() = default;
  Window(int width, int height, const char* title);

  Window(const Window&) = delete;
  auto operator=(const Window&) -> Window& = delete;
  Window(Window&& other) noexcept;
  auto operator=(Window&& other) noexcept -> Window&;

  ~Window();

  void swap_buffers() noexcept;

  [[nodiscard]] auto should_close() const noexcept -> bool;

  [[nodiscard]] BEYOND_FORCE_INLINE auto glfw_window() noexcept -> GLFWwindow*
  {
    return window_;
  }

private:
  GLFWwindow* window_ = nullptr;
};

#endif // VOXEL_GAME_WINDOW_HPP
