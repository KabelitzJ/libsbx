// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_ANIMATION_HPP_
#define LIBSBX_ANIMATIONS_ANIMATION_HPP_

#include <string>
#include <vector>
#include <filesystem>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>

#include <libsbx/animations/spline.hpp>

namespace sbx::animations {

class animation {

public:

  struct bone_track {
    spline<math::vector3> position_spline;
    spline<math::quaternion> rotation_spline;
    spline<math::vector3> scale_spline;
  }; // struct bone_track

  animation(const std::filesystem::path& path, const std::string& name);

  auto track_map() const noexcept -> const std::unordered_map<utility::hashed_string, bone_track>&;

  auto duration() const noexcept -> std::float_t;

private:

  std::string _name;
  std::float_t _duration = 0.0f;
  std::float_t _ticks_per_second = 25.0f;

  std::unordered_map<utility::hashed_string, bone_track> _track_map;

}; // class animation

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_ANIMATION_HPP_
