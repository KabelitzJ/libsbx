// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_INSTANCE_POPULATE_HPP_
#define LIBSBX_SCENES_INSTANCE_POPULATE_HPP_

#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/instance_buffer.hpp>

namespace sbx::scenes {

/** @brief The axis of the source mesh that points away from the surface, like Godot's Mesh Up Axis. */
enum class populate_up_axis : std::uint8_t {
  x,
  y,
  z
}; // enum class populate_up_axis

/** @brief One triangle of a target surface, in the space the instances are placed in; normal is the side instances stand on. */
struct populate_triangle {
  math::vector3 a{};
  math::vector3 b{};
  math::vector3 c{};
  math::vector3 normal{0.0f, 1.0f, 0.0f};
}; // struct populate_triangle

/** @brief Godot's Populate MultiMesh options, plus a seed so a result can be reproduced. */
struct populate_options {
  std::uint32_t amount{128u};
  populate_up_axis mesh_up_axis{populate_up_axis::y};
  std::float_t random_rotation{0.0f}; // degrees either way around the up axis
  std::float_t random_tilt{0.0f}; // degrees either way off the surface normal
  std::float_t scale{1.0f};
  std::float_t random_scale{0.0f}; // fraction either way of scale
  std::uint32_t seed{0u};
}; // struct populate_options

/**
 * @brief Scatters instances over a surface: points spread uniformly by area, the source's up axis along the surface normal, then the random rotation, tilt and scale.
 *
 * @param triangles The target surface; degenerate triangles are ignored.
 * @param options The placement options.
 *
 * @return options.amount instances, or none for a surface without area.
 */
[[nodiscard]] auto populate_surface(std::span<const populate_triangle> triangles, const populate_options& options) -> std::vector<instance_data>;

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_INSTANCE_POPULATE_HPP_
