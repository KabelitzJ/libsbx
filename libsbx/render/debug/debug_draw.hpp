// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/render/debug/debug_draw.hpp
 *
 * @brief A generic immediate-mode line accumulator, owned by scene_renderer_module and drawn once
 * per frame by debug_draw_pass (see libsbx/render/passes/debug_draw_pass.hpp).
 *
 * @ingroup libsbx-render
 */

#ifndef LIBSBX_RENDER_DEBUG_DEBUG_DRAW_HPP_
#define LIBSBX_RENDER_DEBUG_DEBUG_DRAW_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/color.hpp>

namespace sbx::render {

/**
 * @brief One vertex of the vertex-pulled buffer debug_draw_pass draws (matches shaders/passes/debug_draw.slang's `debug_vertex`).
 */
struct debug_vertex {
  math::vector4 position;
  math::color color;
}; // struct debug_vertex

/**
 * @brief Immediate-mode line accumulator. Anything that wants debug geometry on screen -- physics
 * colliders today (see physics::physics_module::late_update()), script-driven gizmos later -- calls
 * add_*() every frame it wants that geometry visible. debug_draw_pass uploads and draws whatever's
 * accumulated each frame, then clear()s it, so nothing needs to be told to stop drawing -- a caller
 * just stops calling add_*().
 *
 * Fetch the shared instance via `core::engine::get_module<render::scene_renderer_module>().debug_draw()`.
 */
class debug_draw final {

public:

  auto add_line(const math::vector3& start, const math::vector3& end, const math::color& color) -> void;

  /**
   * @brief 12-edge wireframe box. `matrix` carries the box's world translation/rotation (and scale,
   * if any); `half_extents` is in that matrix's local space.
   */
  auto add_wire_box(const math::matrix4x4& matrix, const math::vector3& half_extents, const math::color& color) -> void;

  /** @brief 12-edge wireframe box directly from a world-space AABB -- no transform involved. */
  auto add_wire_aabb(const math::volume& volume, const math::color& color) -> void;

  /** @brief Three orthogonal world-axis-aligned rings -- a sphere reads the same from any rotation, so `center` is all that's needed. */
  auto add_wire_sphere(const math::vector3& center, std::float_t radius, const math::color& color, std::uint32_t segments = 20u) -> void;

  /** @brief Capsule axis along `matrix`'s local +Y, matching physics::capsule's convention. `half_height` measures the cylindrical segment only, not the caps. */
  auto add_wire_capsule(const math::matrix4x4& matrix, std::float_t radius, std::float_t half_height, const math::color& color, std::uint32_t segments = 20u) -> void;

  /** @brief Cylinder axis along `matrix`'s local +Y, matching physics::cylinder's convention. */
  auto add_wire_cylinder(const math::matrix4x4& matrix, std::float_t radius, std::float_t half_height, const math::color& color, std::uint32_t segments = 20u) -> void;

  /** @brief A small 3-axis cross, e.g. to mark a contact point. */
  auto add_cross(const math::vector3& point, std::float_t size, const math::color& color) -> void;

  [[nodiscard]] auto vertices() const noexcept -> const std::vector<debug_vertex>& {
    return _vertices;
  }

  auto clear() noexcept -> void {
    _vertices.clear();
  }

private:

  /** @brief One arc from `start_angle` to `end_angle` (radians) around `center`, in the plane spanned by `axis_a`/`axis_b`. A full ring is `start_angle = 0`, `end_angle = 2*pi`. */
  auto _add_arc(const math::vector3& center, const math::vector3& axis_a, const math::vector3& axis_b, std::float_t radius, std::float_t start_angle, std::float_t end_angle, const math::color& color, std::uint32_t segments) -> void;

  std::vector<debug_vertex> _vertices{};

}; // class debug_draw

} // namespace sbx::render

#endif // LIBSBX_RENDER_DEBUG_DEBUG_DRAW_HPP_
