// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/primitive_meshes.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <utility>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/color.hpp>

#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/mesh.hpp>

namespace sbx::assets {

static constexpr auto primitive_mesh_kind_uuids = std::array<math::uuid::value_type, reflection::enum_count<primitive_mesh_kind>()>{
  0x5342583100000001ull,
  0x5342583100000002ull,
  0x5342583100000003ull,
  0x5342583100000004ull,
  0x5342583100000005ull
};

static constexpr auto primitive_mesh_kind_names = std::array<std::string_view, reflection::enum_count<primitive_mesh_kind>()>{
  "Cube",
  "Sphere",
  "Plane",
  "Capsule",
  "Cylinder"
};

[[nodiscard]] auto primitive_mesh_uuid(const primitive_mesh_kind kind) -> math::uuid {
  return math::uuid::from_value(primitive_mesh_kind_uuids[std::to_underlying(kind)]);
}

[[nodiscard]] auto primitive_mesh_name(const primitive_mesh_kind kind) -> std::string_view {
  return primitive_mesh_kind_names[std::to_underlying(kind)];
}

[[nodiscard]] auto primitive_mesh_kind_of(const math::uuid& id) -> std::optional<primitive_mesh_kind> {
  for (const auto kind : reflection::enum_values<primitive_mesh_kind>()) {
    if (primitive_mesh_uuid(kind) == id) {
      return kind;
    }
  }

  return std::nullopt;
}

static auto add_quad(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, const math::vector3& center, const math::vector3& normal, const math::vector3& right, const math::vector3& up, std::float_t half_size) -> void {
  const auto base = static_cast<std::uint32_t>(vertices.size());

  const auto corners = std::array<math::vector3, 4>{
    center - right * half_size - up * half_size,
    center + right * half_size - up * half_size,
    center + right * half_size + up * half_size,
    center - right * half_size + up * half_size
  };

  const auto uvs = std::array<math::vector2, 4>{
    math::vector2{0.0f, 0.0f}, math::vector2{1.0f, 0.0f}, math::vector2{1.0f, 1.0f}, math::vector2{0.0f, 1.0f}
  };

  const auto tangent = math::vector4{right.x(), right.y(), right.z(), 1.0f};

  for (auto i = std::size_t{0}; i < 4u; ++i) {
    vertices.push_back(vertex{corners[i], normal, uvs[i], tangent});
  }

  indices.push_back(base + 0u);
  indices.push_back(base + 1u);
  indices.push_back(base + 2u);
  indices.push_back(base + 0u);
  indices.push_back(base + 2u);
  indices.push_back(base + 3u);
}

[[nodiscard]] static auto generate_cube() -> std::pair<std::vector<vertex>, std::vector<std::uint32_t>> {
  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};

  constexpr auto half_extent = 0.5f;

  struct face { math::vector3 normal, right, up; };

  constexpr auto faces = std::array<face, 6>{{
    face{math::vector3{0.0f, 0.0f, 1.0f}, math::vector3{1.0f, 0.0f, 0.0f}, math::vector3{0.0f, 1.0f, 0.0f}},
    face{math::vector3{0.0f, 0.0f, -1.0f}, math::vector3{-1.0f, 0.0f, 0.0f}, math::vector3{0.0f, 1.0f, 0.0f}},
    face{math::vector3{1.0f, 0.0f, 0.0f}, math::vector3{0.0f, 0.0f, -1.0f}, math::vector3{0.0f, 1.0f, 0.0f}},
    face{math::vector3{-1.0f, 0.0f, 0.0f}, math::vector3{0.0f, 0.0f, 1.0f}, math::vector3{0.0f, 1.0f, 0.0f}},
    face{math::vector3{0.0f, 1.0f, 0.0f}, math::vector3{1.0f, 0.0f, 0.0f}, math::vector3{0.0f, 0.0f, -1.0f}},
    face{math::vector3{0.0f, -1.0f, 0.0f}, math::vector3{1.0f, 0.0f, 0.0f}, math::vector3{0.0f, 0.0f, 1.0f}}
  }};

  for (const auto& f : faces) {
    add_quad(vertices, indices, f.normal * half_extent, f.normal, f.right, f.up, half_extent);
  }

  return {std::move(vertices), std::move(indices)};
}

[[nodiscard]] static auto generate_plane() -> std::pair<std::vector<vertex>, std::vector<std::uint32_t>> {
  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};

  add_quad(vertices, indices, math::vector3::zero, math::vector3{0.0f, 1.0f, 0.0f}, math::vector3{1.0f, 0.0f, 0.0f}, math::vector3{0.0f, 0.0f, -1.0f}, 0.5f);

  return {std::move(vertices), std::move(indices)};
}

[[nodiscard]] static auto generate_uv_sphere(std::float_t radius, std::int32_t rings, std::int32_t segments) -> std::pair<std::vector<vertex>, std::vector<std::uint32_t>> {
  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};

  const auto stride = segments + 1;

  for (auto ring = 0; ring <= rings; ++ring) {
    const auto phi = std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(ring) / static_cast<std::float_t>(rings);
    const auto y = radius * std::cos(phi);
    const auto ring_radius = radius * std::sin(phi);

    for (auto seg = 0; seg <= segments; ++seg) {
      const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);
      const auto x = ring_radius * std::cos(theta);
      const auto z = ring_radius * std::sin(theta);

      const auto position = math::vector3{x, y, z};
      const auto normal = math::vector3::normalized(position);
      const auto uv = math::vector2{static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments), static_cast<std::float_t>(ring) / static_cast<std::float_t>(rings)};
      const auto tangent_dir = math::vector3{-std::sin(theta), 0.0f, std::cos(theta)};

      vertices.push_back(vertex{position, normal, uv, math::vector4{tangent_dir.x(), tangent_dir.y(), tangent_dir.z(), 1.0f}});
    }
  }

  for (auto ring = 0; ring < rings; ++ring) {
    for (auto seg = 0; seg < segments; ++seg) {
      const auto i0 = static_cast<std::uint32_t>(ring * stride + seg);
      const auto i1 = static_cast<std::uint32_t>(ring * stride + seg + 1);
      const auto i2 = static_cast<std::uint32_t>((ring + 1) * stride + seg + 1);
      const auto i3 = static_cast<std::uint32_t>((ring + 1) * stride + seg);

      indices.push_back(i0);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i0);
      indices.push_back(i2);
      indices.push_back(i3);
    }
  }

  return {std::move(vertices), std::move(indices)};
}

// Appends a cylindrical band (no caps) of `segments` quads between y = bottom_y and y = top_y,
// vertex_v0/vertex_v1 giving the V-coordinate each ring's row of vertices should carry (so a
// capsule can chain this between its two hemisphere caps with a continuous V range).
static auto add_cylinder_band(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::float_t radius, std::float_t bottom_y, std::float_t top_y, std::float_t v0, std::float_t v1, std::int32_t segments) -> void {
  const auto stride = segments + 1;
  const auto base = static_cast<std::uint32_t>(vertices.size());

  for (auto row = 0; row < 2; ++row) {
    const auto y = (row == 0) ? bottom_y : top_y;
    const auto v = (row == 0) ? v0 : v1;

    for (auto seg = 0; seg <= segments; ++seg) {
      const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);
      const auto x = radius * std::cos(theta);
      const auto z = radius * std::sin(theta);

      const auto normal = math::vector3::normalized(math::vector3{x, 0.0f, z});
      const auto uv = math::vector2{static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments), v};
      const auto tangent_dir = math::vector3{-std::sin(theta), 0.0f, std::cos(theta)};

      vertices.push_back(vertex{math::vector3{x, y, z}, normal, uv, math::vector4{tangent_dir.x(), tangent_dir.y(), tangent_dir.z(), 1.0f}});
    }
  }

  for (auto seg = 0; seg < segments; ++seg) {
    const auto i0 = base + static_cast<std::uint32_t>(seg);
    const auto i1 = base + static_cast<std::uint32_t>(seg + 1);
    const auto i2 = base + static_cast<std::uint32_t>(stride + seg + 1);
    const auto i3 = base + static_cast<std::uint32_t>(stride + seg);

    // Unlike generate_uv_sphere's rings (where ring+1 sits at a *lower* y), row 1 here sits at a
    // *higher* y than row 0 -- the vertical edge's sign flips, so the same (i0,i1,i2)/(i0,i2,i3)
    // order that's outward-facing there is inward-facing here. Reversed to compensate.
    indices.push_back(i0);
    indices.push_back(i2);
    indices.push_back(i1);
    indices.push_back(i0);
    indices.push_back(i3);
    indices.push_back(i2);
  }
}

static auto add_disc_cap(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::float_t radius, std::float_t y, bool facing_up, std::int32_t segments) -> void {
  const auto normal = math::vector3{0.0f, facing_up ? 1.0f : -1.0f, 0.0f};
  const auto center_index = static_cast<std::uint32_t>(vertices.size());

  vertices.push_back(vertex{math::vector3{0.0f, y, 0.0f}, normal, math::vector2{0.5f, 0.5f}, math::vector4{1.0f, 0.0f, 0.0f, 1.0f}});

  const auto rim_base = static_cast<std::uint32_t>(vertices.size());

  for (auto seg = 0; seg <= segments; ++seg) {
    const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);
    const auto x = radius * std::cos(theta);
    const auto z = radius * std::sin(theta);

    const auto uv = math::vector2{0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)};

    vertices.push_back(vertex{math::vector3{x, y, z}, normal, uv, math::vector4{1.0f, 0.0f, 0.0f, 1.0f}});
  }

  for (auto seg = 0; seg < segments; ++seg) {
    const auto a = rim_base + static_cast<std::uint32_t>(seg);
    const auto b = rim_base + static_cast<std::uint32_t>(seg + 1);

    // theta increases from seg to seg+1 the same way generate_uv_sphere's does, so the correct
    // (center, a, b) vs (center, b, a) order per facing is the sphere's convention flipped, not
    // matched -- a flat disc has no ring-to-ring y change to flip the sign back the other way.
    if (facing_up) {
      indices.push_back(center_index);
      indices.push_back(b);
      indices.push_back(a);
    } else {
      indices.push_back(center_index);
      indices.push_back(a);
      indices.push_back(b);
    }
  }
}

[[nodiscard]] static auto generate_cylinder(std::float_t radius, std::float_t half_height, std::int32_t segments) -> std::pair<std::vector<vertex>, std::vector<std::uint32_t>> {
  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};

  add_cylinder_band(vertices, indices, radius, -half_height, half_height, 0.0f, 1.0f, segments);
  add_disc_cap(vertices, indices, radius, half_height, true, segments);
  add_disc_cap(vertices, indices, radius, -half_height, false, segments);

  return {std::move(vertices), std::move(indices)};
}

[[nodiscard]] static auto generate_capsule(std::float_t radius, std::float_t cylinder_half_height, std::int32_t segments, std::int32_t hemisphere_rings) -> std::pair<std::vector<vertex>, std::vector<std::uint32_t>> {
  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};

  const auto stride = segments + 1;

  // Top hemisphere: rings from the pole (ring 0) down to the equator (ring hemisphere_rings),
  // offset up by cylinder_half_height so it caps the cylindrical body.
  const auto top_base = static_cast<std::uint32_t>(vertices.size());

  for (auto ring = 0; ring <= hemisphere_rings; ++ring) {
    const auto phi = (std::numbers::pi_v<std::float_t> * 0.5f) * static_cast<std::float_t>(ring) / static_cast<std::float_t>(hemisphere_rings);
    const auto y = radius * std::cos(phi);
    const auto ring_radius = radius * std::sin(phi);

    for (auto seg = 0; seg <= segments; ++seg) {
      const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);
      const auto x = ring_radius * std::cos(theta);
      const auto z = ring_radius * std::sin(theta);

      const auto local_normal = math::vector3::normalized(math::vector3{x, y, z});
      const auto position = math::vector3{x, y + cylinder_half_height, z};
      const auto uv = math::vector2{static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments), static_cast<std::float_t>(ring) / static_cast<std::float_t>(hemisphere_rings) * 0.25f};
      const auto tangent_dir = math::vector3{-std::sin(theta), 0.0f, std::cos(theta)};

      vertices.push_back(vertex{position, local_normal, uv, math::vector4{tangent_dir.x(), tangent_dir.y(), tangent_dir.z(), 1.0f}});
    }
  }

  for (auto ring = 0; ring < hemisphere_rings; ++ring) {
    for (auto seg = 0; seg < segments; ++seg) {
      const auto i0 = top_base + static_cast<std::uint32_t>(ring * stride + seg);
      const auto i1 = top_base + static_cast<std::uint32_t>(ring * stride + seg + 1);
      const auto i2 = top_base + static_cast<std::uint32_t>((ring + 1) * stride + seg + 1);
      const auto i3 = top_base + static_cast<std::uint32_t>((ring + 1) * stride + seg);

      indices.push_back(i0);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i0);
      indices.push_back(i2);
      indices.push_back(i3);
    }
  }

  // Cylindrical body between the two hemispheres' equators.
  add_cylinder_band(vertices, indices, radius, -cylinder_half_height, cylinder_half_height, 0.375f, 0.625f, segments);

  // Bottom hemisphere: mirror of the top, rings from the equator (ring 0) down to the pole
  // (ring hemisphere_rings), offset down by cylinder_half_height.
  const auto bottom_base = static_cast<std::uint32_t>(vertices.size());

  for (auto ring = 0; ring <= hemisphere_rings; ++ring) {
    const auto phi = (std::numbers::pi_v<std::float_t> * 0.5f) * static_cast<std::float_t>(ring) / static_cast<std::float_t>(hemisphere_rings);
    const auto y = -radius * std::sin(phi);
    const auto ring_radius = radius * std::cos(phi);

    for (auto seg = 0; seg <= segments; ++seg) {
      const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);
      const auto x = ring_radius * std::cos(theta);
      const auto z = ring_radius * std::sin(theta);

      const auto local_normal = math::vector3::normalized(math::vector3{x, y, z});
      const auto position = math::vector3{x, y - cylinder_half_height, z};
      const auto uv = math::vector2{static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments), 0.75f + static_cast<std::float_t>(ring) / static_cast<std::float_t>(hemisphere_rings) * 0.25f};
      const auto tangent_dir = math::vector3{-std::sin(theta), 0.0f, std::cos(theta)};

      vertices.push_back(vertex{position, local_normal, uv, math::vector4{tangent_dir.x(), tangent_dir.y(), tangent_dir.z(), 1.0f}});
    }
  }

  for (auto ring = 0; ring < hemisphere_rings; ++ring) {
    for (auto seg = 0; seg < segments; ++seg) {
      const auto i0 = bottom_base + static_cast<std::uint32_t>(ring * stride + seg);
      const auto i1 = bottom_base + static_cast<std::uint32_t>(ring * stride + seg + 1);
      const auto i2 = bottom_base + static_cast<std::uint32_t>((ring + 1) * stride + seg + 1);
      const auto i3 = bottom_base + static_cast<std::uint32_t>((ring + 1) * stride + seg);

      indices.push_back(i0);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i0);
      indices.push_back(i2);
      indices.push_back(i3);
    }
  }

  return {std::move(vertices), std::move(indices)};
}

auto ensure_primitive_mesh_cooked(primitive_mesh_kind kind) -> void {
  ensure_default_material_cooked();

  const auto cooked = asset_cooker::cooked_path(primitive_mesh_uuid(kind), ".sbxmsh");

  if (std::filesystem::exists(cooked)) {
    return;
  }

  auto [vertices, indices] = [&]() -> std::pair<std::vector<vertex>, std::vector<std::uint32_t>> {
    switch (kind) {
      case primitive_mesh_kind::cube: return generate_cube();
      case primitive_mesh_kind::sphere: return generate_uv_sphere(0.5f, 24, 32);
      case primitive_mesh_kind::plane: return generate_plane();
      case primitive_mesh_kind::capsule: return generate_capsule(0.5f, 0.5f, 16, 6);
      case primitive_mesh_kind::cylinder: return generate_cylinder(0.5f, 0.5f, 16);
    }

    return {};
  }();

  auto bounds = math::volume{};

  for (const auto& v : vertices) {
    bounds.include(v.position);
  }

  const auto submeshes = std::vector<cooked_submesh>{
    cooked_submesh{0u, static_cast<std::uint32_t>(indices.size()), bounds, default_material_uuid(), {}}
  };

  if (!asset_cooker::write_cooked_mesh(cooked, vertices, indices, submeshes, bounds)) {
    utility::logger<"assets">::warn("Failed to write built-in mesh '{}'", primitive_mesh_name(kind));
  }
}

auto default_material_uuid() -> math::uuid {
  return math::uuid::from_value(0x5342583200000001ull);
}

auto ensure_default_material_cooked() -> void {
  const auto id = default_material_uuid();
  const auto cooked = asset_cooker::cooked_path(id, ".sbxmat");

  if (std::filesystem::exists(cooked)) {
    return;
  }

  auto description = material_description{};
  description.name = "Default";
  description.base_color_factor = math::color{0.8f, 0.8f, 0.8f, 1.0f};
  description.metallic_factor = 0.0f;
  description.roughness_factor = 0.5f;

  if (!asset_cooker::write_cooked_material(id, description)) {
    utility::logger<"assets">::warn("Failed to write built-in default material");
  }
}

} // namespace sbx::assets
