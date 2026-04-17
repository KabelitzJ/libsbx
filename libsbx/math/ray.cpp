// SPDX-License-Identifier: MIT
#include <libsbx/math/ray.hpp>

namespace sbx::math {

ray::ray()
: _origin{vector3::zero},
  _direction{vector3::forward} { }

ray::ray(const vector3& origin, const vector3& direction)
: _origin{origin},
  _direction{vector3::normalized(direction)} { }

auto ray::origin() const -> const vector3& {
  return _origin;
}

auto ray::direction() const -> const vector3& {
  return _direction;
}

auto ray::point_at(const std::float_t t) const -> vector3 {
  return _origin + _direction * t;
}

} // namespace sbx::math
