// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_CAMERA_HPP_
#define LIBSBX_SCENES_COMPONENTS_CAMERA_HPP_

#include <variant>

#include <array>
#include <cstdint>

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/sphere.hpp>
#include <libsbx/math/plane.hpp>
#include <libsbx/math/box.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/core/engine.hpp>

// #include <libsbx/devices/devices_module.hpp>
// #include <libsbx/devices/window.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::scenes {

class frustum : public math::box {

  inline static constexpr auto left_plane = std::size_t{0u};
  inline static constexpr auto right_plane = std::size_t{0u};
  inline static constexpr auto top_plane = std::size_t{1u};
  inline static constexpr auto bottom_plane = std::size_t{1u};
  inline static constexpr auto near_plane = std::size_t{2u};
  inline static constexpr auto far_plane = std::size_t{2u};

  inline static constexpr auto margin = std::float_t{0.5f};

public:

  frustum(const math::matrix4x4& view_projection)
  : math::box{_extract_planes(view_projection)} { }

  auto intersects(const math::volume& aabb, const math::matrix4x4& model) const noexcept -> bool {
    const auto world_volume = math::volume::transformed(aabb, model);

    for (const auto& plane : planes()) {
      const auto positive = math::vector3{
        (plane.normal().x() >= 0 ? world_volume.max().x() : world_volume.min().x()),
        (plane.normal().y() >= 0 ? world_volume.max().y() : world_volume.min().y()),
        (plane.normal().z() >= 0 ? world_volume.max().z() : world_volume.min().z())
      };

      const auto distance = plane.distance_to_point(positive);

      if (distance < -margin) {
        return false;
      }
    }

    return true;
  }

private:

  static auto _extract_planes(const math::matrix4x4& matrix) -> std::array<math::plane, 6u> {
    return std::array<math::plane, 6u>{
      _extract_plane<right_plane>(matrix),
      _extract_plane<left_plane>(matrix),
      _extract_plane<bottom_plane>(matrix),
      _extract_plane<top_plane>(matrix),
      _extract_plane<far_plane>(matrix),
      _extract_plane<near_plane>(matrix)
    };
  }

  template<std::size_t Side>
  static auto _extract_plane(const math::matrix4x4& matrix) -> math::plane {
    constexpr auto sign = (Side == right_plane || Side == top_plane || Side == far_plane) ? -1.0f : 1.0f;

    const auto plane = math::vector4{
      matrix[0][3] + sign * matrix[0][Side], 
      matrix[1][3] + sign * matrix[1][Side], 
      matrix[2][3] + sign * matrix[2][Side],
      matrix[3][3] + sign * matrix[3][Side]
    };

    return math::plane::normalized(math::plane{plane});
  }

}; // struct frustum

class camera {

public:

  camera(const math::angle& field_of_view, std::float_t near_plane, std::float_t far_plane)
  : _field_of_view{field_of_view},
    _near_plane{near_plane},
    _far_plane{far_plane} { }

  auto field_of_view() const noexcept -> const math::angle& {
    return _field_of_view;
  }

  auto set_field_of_view(const math::angle& field_of_view) noexcept -> void {
    _field_of_view = field_of_view;
  }

  auto near_plane() const noexcept -> std::float_t {
    return _near_plane;
  }

  auto far_plane() const noexcept -> std::float_t {
    return _far_plane;
  }

  auto projection(std::float_t aspect_ratio) const noexcept -> math::matrix4x4 {
    return math::matrix4x4::perspective(_field_of_view, aspect_ratio, _near_plane, _far_plane);
  }

  auto projection(std::float_t aspect_ratio, std::float_t near, std::float_t far) const noexcept -> math::matrix4x4 {
    return math::matrix4x4::perspective(_field_of_view, aspect_ratio, near, far);
  }

private:

  math::angle _field_of_view;
  std::float_t _near_plane;
  std::float_t _far_plane;

}; // class camera

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_CAMERA_HPP_
