#ifndef VOXEL_GAME_FIRST_PERSON_CAMERA_HPP
#define VOXEL_GAME_FIRST_PERSON_CAMERA_HPP

#include <beyond/math/matrix.hpp>
#include <beyond/math/transform.hpp>
#include <beyond/math/vector.hpp>

#include <cmath>

// Default camera values
constexpr float default_yaw = -90.0f;
constexpr float default_pitch = 0.0f;
constexpr float default_speed = 2.5f;
constexpr float default_mouse_sensitivity = 0.1f;
constexpr float default_zoom = 45.0f;

// An abstract camera class that processes input and calculates the
// corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class FirstPersonCamera {
  // camera Attributes
  beyond::Point3 position_;
  beyond::Vec3 front_;
  beyond::Vec3 up_;
  beyond::Vec3 right_;
  beyond::Vec3 world_up_;
  // euler Angles
  float yaw_;
  float pitch_;
  // camera options
  float speed_;
  float mouse_sensitivity_;
  float zoom_;

public:
  enum class Movement { FORWARD, BACKWARD, LEFT, RIGHT };

  // constructor with vectors
  FirstPersonCamera(beyond::Vec3 position = beyond::Vec3(0.0f, 0.0f, 0.0f),
                    beyond::Vec3 up = beyond::Vec3(0.0f, 1.0f, 0.0f),
                    float yaw = default_yaw, float pitch = default_pitch)
      : position_{position}, front_{beyond::Vec3(0.0f, 0.0f, -1.0f)}, up_{up},
        yaw_{yaw}, pitch_{pitch}, speed_(default_speed),
        mouse_sensitivity_(default_mouse_sensitivity), zoom_(default_zoom)
  {
    update_camera_vectors();
  }
  // constructor with scalar values
  FirstPersonCamera(float posX, float posY, float posZ, float upX, float upY,
                    float upZ, float yaw, float pitch)
      : position_{beyond::Vec3(posX, posY, posZ)},
        front_(beyond::Vec3(0.0f, 0.0f, -1.0f)),
        up_{beyond::Vec3(upX, upY, upZ)}, yaw_{yaw}, pitch_{pitch},
        speed_(default_speed), mouse_sensitivity_(default_mouse_sensitivity),
        zoom_(default_zoom)
  {
    update_camera_vectors();
  }

  // returns the view matrix calculated using Euler Angles and the LookAt Matrix
  [[nodiscard]] auto get_view_matrix() const -> beyond::Mat4
  {
    return beyond::look_at(position_, position_ + front_, up_);
  }

  // processes input received from any keyboard-like input system. Accepts input
  // parameter in the form of camera defined ENUM (to abstract it from windowing
  // systems)
  void process_keyboard(Movement direction, float delta_time)
  {
    float velocity = speed_ * delta_time;
    switch (direction) {
    case Movement::FORWARD:
      position_ += front_ * velocity;
      break;
    case Movement::BACKWARD:
      position_ -= front_ * velocity;
      break;
    case Movement::LEFT:
      position_ -= right_ * velocity;
      break;
    case Movement::RIGHT:
      position_ += right_ * velocity;
      break;
    }
  }

  // processes input received from a mouse input system. Expects the offset
  // value in both the x and y direction.
  void process_mouse_movement(float xoffset, float yoffset,
                              bool constrain_pitch = true)
  {
    xoffset *= mouse_sensitivity_;
    yoffset *= mouse_sensitivity_;

    yaw_ += xoffset;
    pitch_ += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrain_pitch) {
      if (pitch_ > 89.0f) { pitch_ = 89.0f; }
      if (pitch_ < -89.0f) { pitch_ = -89.0f; }
    }

    // update front_, right_ and up_ Vectors using the updated Euler angles
    update_camera_vectors();
  }

  // processes input received from a mouse scroll-wheel event. Only requires
  // input on the vertical wheel-axis
  void process_mouse_scroll(float yoffset)
  {
    zoom_ -= yoffset;
    if (zoom_ < 1.0f) { zoom_ = 1.0f; }
    if (zoom_ > 45.0f) { zoom_ = 45.0f; }
  }

private:
  // calculates the front vector from the FirstPersonCamera's (updated) Euler
  // Angles
  void update_camera_vectors()
  {
    const auto degree_to_radians = [](float degree) {
      return degree / 180 * beyond::float_constants::pi;
    };

    // calculate the new front_ vector
    beyond::Vec3 front = {
        std::cos(degree_to_radians(yaw_)) * std::cos(degree_to_radians(pitch_)),
        std::sin(degree_to_radians(pitch_)),
        std::sin(degree_to_radians(yaw_)) * std::cos(degree_to_radians(pitch_)),
    };
    front_ = beyond::normalize(front);
    right_ = beyond::normalize(beyond::cross(front_, up_));
    up_ = beyond::normalize(beyond::cross(right_, front_));
  }
};

#endif // VOXEL_GAME_FIRST_PERSON_CAMERA_HPP
