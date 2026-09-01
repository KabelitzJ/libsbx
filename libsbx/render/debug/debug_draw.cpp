// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/debug/debug_draw.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace sbx::render {

auto debug_draw::add_line(const math::vector3& start, const math::vector3& end, const math::color& color) -> void {
  _vertices.push_back(debug_vertex{math::vector4{start, 1.0f}, color});
  _vertices.push_back(debug_vertex{math::vector4{end, 1.0f}, color});
}

auto debug_draw::add_wire_box(const math::matrix4x4& matrix, const math::vector3& half_extents, const math::color& color) -> void {
  const auto local_corners = math::volume{-half_extents, half_extents}.corners();

  auto corners = std::array<math::vector3, 8u>{};

  for (auto i = std::size_t{0u}; i < corners.size(); ++i) {
    corners[i] = math::vector3{matrix * math::vector4{local_corners[i], 1.0f}};
  }

  add_line(corners[0], corners[1], color); add_line(corners[2], corners[3], color);
  add_line(corners[4], corners[5], color); add_line(corners[6], corners[7], color);
  add_line(corners[0], corners[2], color); add_line(corners[1], corners[3], color);
  add_line(corners[4], corners[6], color); add_line(corners[5], corners[7], color);
  add_line(corners[0], corners[4], color); add_line(corners[1], corners[5], color);
  add_line(corners[2], corners[6], color); add_line(corners[3], corners[7], color);
}

auto debug_draw::add_wire_aabb(const math::volume& volume, const math::color& color) -> void {
  const auto corners = volume.corners();

  add_line(corners[0], corners[1], color); add_line(corners[2], corners[3], color);
  add_line(corners[4], corners[5], color); add_line(corners[6], corners[7], color);
  add_line(corners[0], corners[2], color); add_line(corners[1], corners[3], color);
  add_line(corners[4], corners[6], color); add_line(corners[5], corners[7], color);
  add_line(corners[0], corners[4], color); add_line(corners[1], corners[5], color);
  add_line(corners[2], corners[6], color); add_line(corners[3], corners[7], color);
}

auto debug_draw::_add_arc(const math::vector3& center, const math::vector3& axis_a, const math::vector3& axis_b, std::float_t radius, std::float_t start_angle, std::float_t end_angle, const math::color& color, std::uint32_t segments) -> void {
  segments = std::max(segments, 1u);

  auto previous = center + (axis_a * std::cos(start_angle) + axis_b * std::sin(start_angle)) * radius;

  for (auto i = std::uint32_t{1u}; i <= segments; ++i) {
    const auto t = start_angle + (end_angle - start_angle) * (static_cast<std::float_t>(i) / static_cast<std::float_t>(segments));
    const auto current = center + (axis_a * std::cos(t) + axis_b * std::sin(t)) * radius;

    add_line(previous, current, color);

    previous = current;
  }
}

auto debug_draw::add_wire_sphere(const math::vector3& center, std::float_t radius, const math::color& color, std::uint32_t segments) -> void {
  constexpr auto tau = 2.0f * std::numbers::pi_v<std::float_t>;

  // Three orthogonal world-axis-aligned great circles -- a sphere reads the same under any
  // rotation, so there's no need to orient these to the collider's actual transform.
  _add_arc(center, math::vector3::right, math::vector3::forward, radius, 0.0f, tau, color, segments);
  _add_arc(center, math::vector3::right, math::vector3::up, radius, 0.0f, tau, color, segments);
  _add_arc(center, math::vector3::forward, math::vector3::up, radius, 0.0f, tau, color, segments);
}

auto debug_draw::add_wire_cylinder(const math::matrix4x4& matrix, std::float_t radius, std::float_t half_height, const math::color& color, std::uint32_t segments) -> void {
  constexpr auto tau = 2.0f * std::numbers::pi_v<std::float_t>;

  const auto center = math::vector3{matrix[3]};
  const auto axis_x = math::vector3::normalized(math::vector3{matrix[0]});
  const auto axis_y = math::vector3::normalized(math::vector3{matrix[1]});
  const auto axis_z = math::vector3::normalized(math::vector3{matrix[2]});

  const auto top = center + axis_y * half_height;
  const auto bottom = center - axis_y * half_height;

  _add_arc(top, axis_x, axis_z, radius, 0.0f, tau, color, segments);
  _add_arc(bottom, axis_x, axis_z, radius, 0.0f, tau, color, segments);

  constexpr auto vertical_count = 4u;

  for (auto i = std::uint32_t{0u}; i < vertical_count; ++i) {
    const auto angle = tau * (static_cast<std::float_t>(i) / static_cast<std::float_t>(vertical_count));
    const auto offset = (axis_x * std::cos(angle) + axis_z * std::sin(angle)) * radius;

    add_line(top + offset, bottom + offset, color);
  }
}

auto debug_draw::add_wire_capsule(const math::matrix4x4& matrix, std::float_t radius, std::float_t half_height, const math::color& color, std::uint32_t segments) -> void {
  constexpr auto tau = 2.0f * std::numbers::pi_v<std::float_t>;
  constexpr auto pi = std::numbers::pi_v<std::float_t>;

  const auto center = math::vector3{matrix[3]};
  const auto axis_x = math::vector3::normalized(math::vector3{matrix[0]});
  const auto axis_y = math::vector3::normalized(math::vector3{matrix[1]});
  const auto axis_z = math::vector3::normalized(math::vector3{matrix[2]});

  const auto top = center + axis_y * half_height;
  const auto bottom = center - axis_y * half_height;

  _add_arc(top, axis_x, axis_z, radius, 0.0f, tau, color, segments);
  _add_arc(bottom, axis_x, axis_z, radius, 0.0f, tau, color, segments);

  constexpr auto vertical_count = 4u;

  for (auto i = std::uint32_t{0u}; i < vertical_count; ++i) {
    const auto angle = tau * (static_cast<std::float_t>(i) / static_cast<std::float_t>(vertical_count));
    const auto offset = (axis_x * std::cos(angle) + axis_z * std::sin(angle)) * radius;

    add_line(top + offset, bottom + offset, color);
  }

  // Hemisphere caps -- two half-circle silhouettes per cap, in the axis_x/axis_y and axis_z/axis_y
  // planes, each running from the ring's equator point, through the pole, to the diametrically
  // opposite equator point (matches the ring + verticals + cap-arcs gizmo most physics
  // debug-drawers use for capsules).
  const auto cap_segments = std::max(segments / 2u, 4u);

  _add_arc(top, axis_x, axis_y, radius, 0.0f, pi, color, cap_segments);
  _add_arc(top, axis_z, axis_y, radius, 0.0f, pi, color, cap_segments);
  _add_arc(bottom, axis_x, -axis_y, radius, 0.0f, pi, color, cap_segments);
  _add_arc(bottom, axis_z, -axis_y, radius, 0.0f, pi, color, cap_segments);
}

auto debug_draw::add_cross(const math::vector3& point, std::float_t size, const math::color& color) -> void {
  const auto half = size * 0.5f;

  add_line(point - math::vector3::right * half, point + math::vector3::right * half, color);
  add_line(point - math::vector3::up * half, point + math::vector3::up * half, color);
  add_line(point - math::vector3::forward * half, point + math::vector3::forward * half, color);
}

} // namespace sbx::render
