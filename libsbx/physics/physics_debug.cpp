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

auto draw_convex_shape(render::debug_draw& debug_draw, const convex_shape& shape, const math::matrix4x4& matrix, const math::vector3& scale, const math::color& color) -> void {
  std::visit(utility::overload(
    [&](const sphere& shape) {
      // Non-uniform scale draws with scale.x as the radius; collision is still exact.
      debug_draw.add_wire_sphere(matrix, shape.radius * scale.x(), color);
    },
    [&](const cylinder& shape) {
      debug_draw.add_wire_cylinder(matrix, shape.radius * scale.x(), shape.half_height * scale.x(), color);
    },
    [&](const capsule& shape) {
      debug_draw.add_wire_capsule(matrix, shape.radius * scale.x(), shape.half_height * scale.x(), color);
    },
    [&](const box& shape) {
      debug_draw.add_wire_box(matrix, shape.half_extents * scale, color);
    },
    [&]([[maybe_unused]] const triangle& shape) {
      // Mesh collider candidates only; never reached.
    },
    [&](const convex_hull& shape) {
      // Draws the hull faces, or crosses at the points for a degenerate hull. Points are scaled before the matrix, consistent with the other shapes.
      if (shape.faces.empty()) {
        constexpr auto marker_size = 0.06f;

        for (const auto& point : shape.points) {
          debug_draw.add_cross(math::vector3{matrix * math::vector4{point * scale, 1.0f}}, marker_size, color);
        }

        return;
      }

      for (const auto& face : shape.faces) {
        const auto v0 = math::vector3{matrix * math::vector4{shape.points[face.indices[0]] * scale, 1.0f}};
        const auto v1 = math::vector3{matrix * math::vector4{shape.points[face.indices[1]] * scale, 1.0f}};
        const auto v2 = math::vector3{matrix * math::vector4{shape.points[face.indices[2]] * scale, 1.0f}};

        debug_draw.add_line(v0, v1, color);
        debug_draw.add_line(v1, v2, color);
        debug_draw.add_line(v2, v0, color);
      }
    }
  ), shape);
}

auto draw_navmesh(render::debug_draw& debug_draw, const navmesh& mesh, const math::color& color, const math::color& boundary_color) -> void {
  for (const auto& poly : mesh.polys) {
    const auto count = poly.verts.size();

    for (auto i = std::size_t{0}; i < count; ++i) {
      const auto& a = mesh.verts[poly.verts[i]];
      const auto& b = mesh.verts[poly.verts[(i + 1u) % count]];

      const auto is_boundary = poly.neighbors[i] == null_poly_reference;

      debug_draw.add_line(a, b, is_boundary ? boundary_color : color);
    }
  }
}

auto draw_nav_path(render::debug_draw& debug_draw, std::span<const straight_path_point> path, const math::color& color) -> void {
  constexpr auto corner_size = 0.1f;

  for (auto i = std::size_t{0}; i < path.size(); ++i) {
    debug_draw.add_cross(path[i].position, corner_size, color);

    if (i + 1 < path.size()) {
      debug_draw.add_line(path[i].position, path[i + 1].position, color);
    }
  }
}

auto draw_nav_agent(render::debug_draw& debug_draw, const nav_agent& agent, const math::vector3& position, const math::color& color) -> void {
  const auto matrix = math::matrix4x4::translated(math::matrix4x4::identity, position);

  debug_draw.add_wire_cylinder(matrix, agent.radius, agent.height * 0.5f, color);

  if (agent.velocity.length_squared() > 0.0001f) {
    debug_draw.add_line(position, position + agent.velocity, color);
  }
}

} // namespace sbx::physics
