// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_POINT_LIGHT_HPP_
#define LIBSBX_SCENES_COMPONENTS_POINT_LIGHT_HPP_

#include <libsbx/math/color.hpp>

namespace sbx::scenes {

class point_light {

public:

  point_light(math::color color, std::float_t radius)
  : _color{color},
    _radius{radius} { }

  auto color() const noexcept -> const math::color& {
    return _color;
  }

  auto radius() const noexcept -> std::float_t {
    return _radius;
  }

private:

  math::color _color;
  std::float_t _radius;

}; // class point_light

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_POINT_LIGHT_HPP_
