#ifndef VOXEL_GAME_WINDOW_MANAGER_HPP
#define VOXEL_GAME_WINDOW_MANAGER_HPP

class WindowManager {
public:
  [[nodiscard]] static auto instance() -> WindowManager&
  {
    static WindowManager s;
    return s;
  }

  ~WindowManager();
  WindowManager(const WindowManager&) = delete;
  auto operator=(const WindowManager&) -> WindowManager& = delete;
  WindowManager(WindowManager&&) = delete;
  auto operator=(WindowManager&&) -> WindowManager& = delete;

  void pull_events();

private:
  WindowManager();
};

#endif // VOXEL_GAME_WINDOW_MANAGER_HPP
