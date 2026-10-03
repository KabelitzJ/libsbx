// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/render/debug/debug_draw.hpp
 *
 * @brief An immediate-mode line accumulator, owned by scene_renderer_module and drawn by debug_draw_pass.
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

/** @brief One vertex of debug_draw_pass's vertex-pulled buffer; matches debug_draw.slang's `debug_vertex`. */
struct debug_vertex {
  math::vector4 position;
  math::color color;
}; // struct debug_vertex

/** @brief Immediate-mode line accumulator: call add_*() every frame the geometry should show; debug_draw_pass draws and clears it. */
class debug_draw final {

public:

  auto add_line(const math::vector3& start, const math::vector3& end, const math::color& color) -> void;

  /**
   * @brief A wireframe box; @p half_extents is in @p matrix's local space.
   *
   * @param matrix The box's world transform.
   * @param half_extents The local half extents.
   * @param color The line color.
   */
  auto add_wire_box(const math::matrix4x4& matrix, const math::vector3& half_extents, const math::color& color) -> void;

  /**
   * @brief A wireframe box from a world-space AABB.
   *
   * @param volume The AABB.
   * @param color The line color.
   */
  auto add_wire_aabb(const math::volume& volume, const math::color& color) -> void;

  /**
   * @brief Three world-axis rings; prefer the matrix overload when there is a rotation to show.
   *
   * @param center The sphere's center.
   * @param radius The sphere's radius.
   * @param color The line color.
   * @param segments Segments per ring.
   */
  auto add_wire_sphere(const math::vector3& center, std::float_t radius, const math::color& color, std::uint32_t segments = 20u) -> void;

  /**
   * @brief Three rings along @p matrix's local axes, so rotation is visible.
   *
   * @param matrix The sphere's world transform.
   * @param radius The sphere's radius.
   * @param color The line color.
   * @param segments Segments per ring.
   */
  auto add_wire_sphere(const math::matrix4x4& matrix, std::float_t radius, const math::color& color, std::uint32_t segments = 20u) -> void;

  /** @brief A capsule along @p matrix's local +Y, like physics::capsule; `half_height` covers the cylinder only, not the caps. */
  auto add_wire_capsule(const math::matrix4x4& matrix, std::float_t radius, std::float_t half_height, const math::color& color, std::uint32_t segments = 20u) -> void;

  /** @brief A cylinder along @p matrix's local +Y, like physics::cylinder. */
  auto add_wire_cylinder(const math::matrix4x4& matrix, std::float_t radius, std::float_t half_height, const math::color& color, std::uint32_t segments = 20u) -> void;

  /**
   * @brief A small 3-axis cross, e.g. for a contact point.
   *
   * @param point The cross's center.
   * @param size The arm length.
   * @param color The line color.
   */
  auto add_cross(const math::vector3& point, std::float_t size, const math::color& color) -> void;

  [[nodiscard]] auto vertices() const noexcept -> const std::vector<debug_vertex>& {
    return _vertices;
  }

  auto clear() noexcept -> void {
    _vertices.clear();
  }

private:

  /** @brief An arc from `start_angle` to `end_angle` (radians) around `center` in the plane of `axis_a`/`axis_b`. */
  auto _add_arc(const math::vector3& center, const math::vector3& axis_a, const math::vector3& axis_b, std::float_t radius, std::float_t start_angle, std::float_t end_angle, const math::color& color, std::uint32_t segments) -> void;

  std::vector<debug_vertex> _vertices{};

}; // class debug_draw

} // namespace sbx::render

#endif // LIBSBX_RENDER_DEBUG_DEBUG_DRAW_HPP_
