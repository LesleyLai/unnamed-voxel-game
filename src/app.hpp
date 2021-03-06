#ifndef VOXEL_GAME_APP_HPP
#define VOXEL_GAME_APP_HPP

#include <GLFW/glfw3.h>

class App {
  GLFWwindow* window_ = nullptr;

public:
  App();
  ~App();

  void exec();

  App(const App&) = delete;
  auto operator=(const App&) -> App& = delete;
  App(App&&) noexcept = delete;
  auto operator=(App&&) noexcept -> App& = delete;
};

#endif // VOXEL_GAME_APP_HPP
