// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/physics_debug.hpp>

#include <libsbx/math/vector4.hpp>

#include <libsbx/utility/overload.hpp>

#include <libsbx/render/debug/debug_draw.hpp>

namespace sbx::physics {

auto debug_color_for(body_type type, bool is_sleeping) -> math::color {
  const auto base = [&]() -> math::color {
    switch (type) {
      case body_type::dynamic_body: return math::color{0.2f, 0.9f, 0.2f, 1.0f};
      case body_type::kinematic: return math::color{0.2f, 0.5f, 1.0f, 1.0f};
      case body_type::static_body: return math::color{0.7f, 0.7f, 0.7f, 1.0f};
    }

    return math::color::white();
  }();

  return is_sleeping ? base * 0.5f : base;
}

auto draw_convex_shape(render::debug_draw& debug_draw, const convex_shape& shape, const math::matrix4x4& matrix, const math::color& color) -> void {
  std::visit(utility::overload(
    [&](const sphere& shape) {
      debug_draw.add_wire_sphere(matrix, shape.radius, color);
    },
    [&](const cylinder& shape) {
      debug_draw.add_wire_cylinder(matrix, shape.radius, shape.half_height, color);
    },
    [&](const capsule& shape) {
      debug_draw.add_wire_capsule(matrix, shape.radius, shape.half_height, color);
    },
    [&](const box& shape) {
      debug_draw.add_wire_box(matrix, shape.half_extents, color);
    },
    [&]([[maybe_unused]] const triangle& shape) {
      // Mesh-collider narrowphase candidate only -- never authored on a shape_collider, so never
      // reached here in practice.
    },
    [&](const convex_hull& shape) {
      // Never authored on a shape_collider either; physics_module's mesh_collider debug-draw loop
      // is the real caller. Draws the actual hull faces when quickhull.hpp produced any; falls back
      // to the bare point set (a degenerate source mesh -- see compute_convex_hull) as small crosses.
      if (shape.faces.is_empty()) {
        constexpr auto marker_size = 0.06f;

        for (const auto& point : shape.points) {
          debug_draw.add_cross(math::vector3{matrix * math::vector4{point, 1.0f}}, marker_size, color);
        }

        return;
      }

      for (const auto& face : shape.faces) {
        const auto v0 = math::vector3{matrix * math::vector4{shape.points[face.indices[0]], 1.0f}};
        const auto v1 = math::vector3{matrix * math::vector4{shape.points[face.indices[1]], 1.0f}};
        const auto v2 = math::vector3{matrix * math::vector4{shape.points[face.indices[2]], 1.0f}};

        debug_draw.add_line(v0, v1, color);
        debug_draw.add_line(v1, v2, color);
        debug_draw.add_line(v2, v0, color);
      }
    }
  ), shape);
}

} // namespace sbx::physics
