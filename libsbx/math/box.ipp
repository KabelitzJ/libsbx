// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
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

    // -0.5f here (not the standard positive-vertex test's 0.0f) is unexplained: no comment, no
    // test, and no commit message justifies it anywhere in this file's history. Git archaeology
    // (see box.ipp's history around 2025-06-30) shows the function used to test all 8 corners
    // with a tolerance of `-volume.diagonal_length() * 0.5f`, which at least scaled with the
    // volume's own size; when it was rewritten to the single-positive-vertex test, the
    // `volume.diagonal_length() *` factor was dropped but the bare `* 0.5f` was left behind,
    // strongly suggesting this is a leftover fragment rather than a deliberately chosen constant.
    // Left unchanged pending confirmation — flip to `< 0.0f` (or `< value_type{0}`) for the
    // textbook positive-vertex test if that fragment theory is confirmed.
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
