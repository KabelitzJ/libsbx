// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/physics_debug.hpp>

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
      debug_draw.add_wire_sphere(math::vector3{matrix[3]}, shape.radius, color);
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
    }
  ), shape);
}

} // namespace sbx::physics
