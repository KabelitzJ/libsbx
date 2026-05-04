// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_DIRECTIONAL_LIGHT_HPP_
#define LIBSBX_SCENES_COMPONENTS_DIRECTIONAL_LIGHT_HPP_

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

namespace sbx::scenes {

class directional_light {

public:

  directional_light(const math::color& color, const math::vector3& direction)
  : _color{color},
    _direction{direction} { }

  ~directional_light() = default;

  [[nodiscard]] auto color() const noexcept -> const math::color& {
    return _color;
  }

  [[nodiscard]] auto color() noexcept -> math::color& {
    return _color;
  }

  [[nodiscard]] auto direction() const noexcept -> const math::vector3& {
    return _direction;
  }

  [[nodiscard]] auto direction() noexcept -> math::vector3& {
    return _direction;
  }

  auto set_direction(const math::vector3& direction) noexcept -> void {
    _direction = math::vector3::normalized(direction);
  }

private:

  math::color _color;
  math::vector3 _direction;

}; // class directional_light

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_DIRECTIONAL_LIGHT_HPP_
