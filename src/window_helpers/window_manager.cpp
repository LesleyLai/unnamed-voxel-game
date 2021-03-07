#include "window_manager.hpp"

#include <GLFW/glfw3.h>

#include <beyond/utils/panic.hpp>

WindowManager::WindowManager()
{
  if (!glfwInit()) { beyond::panic("Failed to initialize GLFW\n"); }
}

WindowManager::~WindowManager()
{
  glfwTerminate();
}

void WindowManager::pull_events()
{
  glfwPollEvents();
}
