#include "app.hpp"

#include <fmt/format.h>

#define CHECK_GLFW(glfw_call)                                                  \
  if (!(glfw_call)) std::exit(1);

App::App()
{
  CHECK_GLFW(glfwInit());

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window_ = glfwCreateWindow(1400, 900, "Voxel Game", nullptr, nullptr);
  if (!window_) { std::exit(1); }
  glfwMakeContextCurrent(window_);
}

App::~App()
{
  glfwDestroyWindow(window_);
  glfwTerminate();
}

void App::exec()
{
  while (!glfwWindowShouldClose(window_)) {
    glfwSwapBuffers(window_);
    glfwPollEvents();
  }
}