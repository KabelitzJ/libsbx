// SPDX-License-Identifier: MIT
#include <libsbx/math/box.hpp>

namespace sbx::math {

template<scalar Type>
basic_box<Type>::basic_box(const std::array<plane_type, 6u>& planes) noexcept
: _planes{planes} { }

template<scalar Type>
basic_box<Type>::basic_box(std::array<plane_type, 6u>&& planes) noexcept
: _planes{std::move(planes)} { }

template<scalar Type>
auto basic_box<Type>::intersects(const volume_type& volume) const -> bool {

  for (const auto& plane : planes()) {
    const auto vp = math::vector3{
      (plane.normal().x() >= 0 ? volume.max().x() : volume.min().x()),
      (plane.normal().y() >= 0 ? volume.max().y() : volume.min().y()),
      (plane.normal().z() >= 0 ? volume.max().z() : volume.min().z())
    };

    if (plane.distance_to_point(vp) < -0.5f) {
      return false;
    }
  }

  return true;
}

template<scalar Type>
auto basic_box<Type>::planes() const noexcept -> const std::array<plane_type, 6u>& {

  return _planes;
}

template<scalar Type>
auto basic_box<Type>::plane(const size_type index) const noexcept -> const plane_type& {

  return _planes[index];
}

} // namespace sbx::math
