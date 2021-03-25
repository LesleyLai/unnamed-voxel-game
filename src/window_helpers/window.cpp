#include "window.hpp"

#include <beyond/utils/panic.hpp>

#include <GLFW/glfw3.h>

Window::Window(int width, int height, const char* title)
{
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
  if (!window_) { beyond::panic("Cannot create a glfw window"); }
}

Window::Window(Window&& other) noexcept
    : window_{std::exchange(other.window_, nullptr)}
{
}

Window::~Window()
{
  glfwDestroyWindow(window_);
}

auto Window::operator=(Window&& other) noexcept -> Window&
{
  if (this != &other) { window_ = std::exchange(other.window_, nullptr); }
  return *this;
}
void Window::swap_buffers() noexcept
{
  glfwSwapBuffers(window_);
}

auto Window::should_close() const noexcept -> bool
{
  return glfwWindowShouldClose(window_);
}
