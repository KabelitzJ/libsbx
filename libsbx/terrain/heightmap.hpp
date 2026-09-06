// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_TERRAIN_HEIGHTMAP_HPP_
#define LIBSBX_TERRAIN_HEIGHTMAP_HPP_

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::terrain {

/**
 * @brief A regular grid of height samples, cell_size apart, centered on the world origin. The
 * single source of truth for a terrain's shape -- chunk mesh generation (terrain_chunking.hpp),
 * the heightfield_collider raycast (physics/raycast.hpp), and elevation sampling exposed to C#
 * (Terrain.SampleHeight/SampleNormal) all read through this. Immutable once built; a future sculpt
 * tool would replace the whole heightmap (shared via std::shared_ptr) rather than mutate in place.
 */
class heightmap final {

public:

  heightmap() = default;

  heightmap(std::uint32_t width, std::uint32_t depth, std::float_t cell_size, std::vector<std::float_t> heights)
  : _width{width}, _depth{depth}, _cell_size{cell_size}, _heights{std::move(heights)} { }

  [[nodiscard]] auto width() const noexcept -> std::uint32_t {
    return _width;
  }

  [[nodiscard]] auto depth() const noexcept -> std::uint32_t {
    return _depth;
  }

  [[nodiscard]] auto cell_size() const noexcept -> std::float_t {
    return _cell_size;
  }

  /** @brief World-space half-extent along X/Z -- the grid spans [-half_extent, +half_extent] on each axis, centered at the origin. */
  [[nodiscard]] auto half_extent() const noexcept -> math::vector2 {
    if (_width == 0u || _depth == 0u) {
      return math::vector2{0.0f, 0.0f};
    }

    return math::vector2{
      static_cast<std::float_t>(_width - 1u) * _cell_size * 0.5f,
      static_cast<std::float_t>(_depth - 1u) * _cell_size * 0.5f
    };
  }

  /** @brief The min/max sampled height across the whole grid -- used to size a conservative world-space AABB for the broadphase. 0/0 if empty. */
  [[nodiscard]] auto height_range() const noexcept -> std::pair<std::float_t, std::float_t> {
    if (_heights.empty()) {
      return {0.0f, 0.0f};
    }

    const auto [min_it, max_it] = std::minmax_element(_heights.begin(), _heights.end());
    return {*min_it, *max_it};
  }

  [[nodiscard]] auto height_at(std::uint32_t x, std::uint32_t z) const noexcept -> std::float_t {
    return _heights[static_cast<std::size_t>(z) * _width + x];
  }

  /** @brief Bilinearly interpolated height at an arbitrary world-space (x, z); clamped to the grid's own edge outside its bounds. 0 for an empty heightmap. */
  [[nodiscard]] auto sample_bilinear(const math::vector2& world_xz) const noexcept -> std::float_t {
    if (_heights.empty() || _width == 0u || _depth == 0u) {
      return 0.0f;
    }

    const auto extent = half_extent();

    // World (x, z) -> continuous grid-cell coordinates, [0, width - 1] x [0, depth - 1].
    const auto gx = std::clamp((world_xz.x() + extent.x()) / _cell_size, 0.0f, static_cast<std::float_t>(_width - 1u));
    const auto gz = std::clamp((world_xz.y() + extent.y()) / _cell_size, 0.0f, static_cast<std::float_t>(_depth - 1u));

    const auto x0 = static_cast<std::uint32_t>(gx);
    const auto z0 = static_cast<std::uint32_t>(gz);
    const auto x1 = std::min(x0 + 1u, _width - 1u);
    const auto z1 = std::min(z0 + 1u, _depth - 1u);

    const auto tx = gx - static_cast<std::float_t>(x0);
    const auto tz = gz - static_cast<std::float_t>(z0);

    const auto h00 = height_at(x0, z0);
    const auto h10 = height_at(x1, z0);
    const auto h01 = height_at(x0, z1);
    const auto h11 = height_at(x1, z1);

    const auto h0 = h00 + (h10 - h00) * tx;
    const auto h1 = h01 + (h11 - h01) * tx;

    return h0 + (h1 - h0) * tz;
  }

  /** @brief Finite-difference surface normal at an arbitrary world-space (x, z), from samples one cell_size to either side. +Y for an empty heightmap. */
  [[nodiscard]] auto sample_normal(const math::vector2& world_xz) const noexcept -> math::vector3 {
    if (_heights.empty()) {
      return math::vector3{0.0f, 1.0f, 0.0f};
    }

    const auto h = _cell_size;
    const auto height_left = sample_bilinear(math::vector2{world_xz.x() - h, world_xz.y()});
    const auto height_right = sample_bilinear(math::vector2{world_xz.x() + h, world_xz.y()});
    const auto height_down = sample_bilinear(math::vector2{world_xz.x(), world_xz.y() - h});
    const auto height_up = sample_bilinear(math::vector2{world_xz.x(), world_xz.y() + h});

    const auto normal = math::vector3{height_left - height_right, 2.0f * h, height_down - height_up};

    return math::vector3::normalized(normal);
  }

private:

  std::uint32_t _width{0u};
  std::uint32_t _depth{0u};
  std::float_t _cell_size{1.0f};
  std::vector<std::float_t> _heights{};

}; // class heightmap

} // namespace sbx::terrain

#endif // LIBSBX_TERRAIN_HEIGHTMAP_HPP_
