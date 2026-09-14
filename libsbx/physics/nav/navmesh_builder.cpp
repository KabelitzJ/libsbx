// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/nav/navmesh_builder.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <variant>
#include <vector>

#include <libsbx/utility/overload.hpp>

#include <libsbx/ecs/entity.hpp>

#include <libsbx/scenes/node.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/narrowphase.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>
#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/nav/bake_arena.hpp>
#include <libsbx/physics/nav/compact_heightfield.hpp>
#include <libsbx/physics/nav/contour.hpp>
#include <libsbx/physics/nav/poly_mesh.hpp>
#include <libsbx/physics/nav/poly_mesh_detail.hpp>

namespace sbx::physics {

[[nodiscard]] auto transform_point(const transform& pose, const math::vector3& local) -> math::vector3 {
  const auto scaled = math::vector3{local.x() * pose.scale.x(), local.y() * pose.scale.y(), local.z() * pose.scale.z()};
  return pose.position + (pose.rotation * scaled);
}

auto append_triangle_mesh(const std::vector<math::vector3>& local_vertices, const std::vector<std::uint32_t>& local_indices, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  const auto base = static_cast<std::uint32_t>(vertices.size());

  for (const auto& local_vertex : local_vertices) {
    vertices.push_back(transform_point(pose, local_vertex));
  }

  for (const auto index : local_indices) {
    indices.push_back(base + index);
  }
}

auto triangulate_box(const box& shape, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  const auto& e = shape.half_extents;

  const auto corners = std::array<math::vector3, 8>{
    math::vector3{-e.x(), -e.y(), -e.z()}, math::vector3{e.x(), -e.y(), -e.z()},
    math::vector3{e.x(), -e.y(), e.z()}, math::vector3{-e.x(), -e.y(), e.z()},
    math::vector3{-e.x(), e.y(), -e.z()}, math::vector3{e.x(), e.y(), -e.z()},
    math::vector3{e.x(), e.y(), e.z()}, math::vector3{-e.x(), e.y(), e.z()}
  };

  constexpr auto faces = std::array<std::array<std::uint32_t, 4>, 6>{{
    {0u, 1u, 2u, 3u},
    {4u, 7u, 6u, 5u},
    {0u, 4u, 5u, 1u},
    {3u, 2u, 6u, 7u},
    {0u, 3u, 7u, 4u},
    {1u, 5u, 6u, 2u}
  }};

  const auto base = static_cast<std::uint32_t>(vertices.size());

  for (const auto& corner : corners) {
    vertices.push_back(transform_point(pose, corner));
  }

  for (const auto& face : faces) {
    indices.push_back(base + face[0]);
    indices.push_back(base + face[1]);
    indices.push_back(base + face[2]);

    indices.push_back(base + face[0]);
    indices.push_back(base + face[2]);
    indices.push_back(base + face[3]);
  }
}

auto triangulate_convex_hull(const convex_hull& shape, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  const auto base = static_cast<std::uint32_t>(vertices.size());

  for (const auto& point : shape.points) {
    vertices.push_back(transform_point(pose, point));
  }

  for (const auto& face : shape.faces) {
    indices.push_back(base + face.indices[0]);
    indices.push_back(base + face.indices[1]);
    indices.push_back(base + face.indices[2]);
  }
}

auto triangulate_cylinder_band(std::float_t radius, std::float_t bottom_y, std::float_t top_y, std::int32_t segments, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  const auto base = static_cast<std::uint32_t>(vertices.size());

  for (auto row = 0; row < 2; ++row) {
    const auto y = (row == 0) ? bottom_y : top_y;

    for (auto seg = 0; seg < segments; ++seg) {
      const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);

      vertices.push_back(transform_point(pose, math::vector3{radius * std::cos(theta), y, radius * std::sin(theta)}));
    }
  }

  const auto bottom_base = base;
  const auto top_base = base + static_cast<std::uint32_t>(segments);

  for (auto seg = std::int32_t{0}; seg < segments; ++seg) {
    const auto next = (seg + 1) % segments;

    const auto i0 = bottom_base + static_cast<std::uint32_t>(seg);
    const auto i1 = bottom_base + static_cast<std::uint32_t>(next);
    const auto i2 = top_base + static_cast<std::uint32_t>(next);
    const auto i3 = top_base + static_cast<std::uint32_t>(seg);

    indices.push_back(i0);
    indices.push_back(i2);
    indices.push_back(i1);
    indices.push_back(i0);
    indices.push_back(i3);
    indices.push_back(i2);
  }
}

auto triangulate_disc_cap(std::float_t radius, std::float_t y, bool facing_up, std::int32_t segments, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  const auto center_index = static_cast<std::uint32_t>(vertices.size());

  vertices.push_back(transform_point(pose, math::vector3{0.0f, y, 0.0f}));

  const auto rim_base = static_cast<std::uint32_t>(vertices.size());

  for (auto seg = std::int32_t{0}; seg < segments; ++seg) {
    const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);

    vertices.push_back(transform_point(pose, math::vector3{radius * std::cos(theta), y, radius * std::sin(theta)}));
  }

  for (auto seg = std::int32_t{0}; seg < segments; ++seg) {
    const auto a = rim_base + static_cast<std::uint32_t>(seg);
    const auto b = rim_base + static_cast<std::uint32_t>((seg + 1) % segments);

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

auto triangulate_hemisphere_rings(std::float_t radius, std::float_t y_offset, bool top, std::int32_t segments, std::int32_t rings, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  const auto base = static_cast<std::uint32_t>(vertices.size());

  for (auto ring = std::int32_t{0}; ring <= rings; ++ring) {
    const auto phi = (std::numbers::pi_v<std::float_t> * 0.5f) * static_cast<std::float_t>(ring) / static_cast<std::float_t>(rings);
    const auto y = top ? radius * std::cos(phi) : -radius * std::sin(phi);
    const auto ring_radius = top ? radius * std::sin(phi) : radius * std::cos(phi);

    for (auto seg = std::int32_t{0}; seg < segments; ++seg) {
      const auto theta = 2.0f * std::numbers::pi_v<std::float_t> * static_cast<std::float_t>(seg) / static_cast<std::float_t>(segments);

      vertices.push_back(transform_point(pose, math::vector3{ring_radius * std::cos(theta), y + y_offset, ring_radius * std::sin(theta)}));
    }
  }

  for (auto ring = std::int32_t{0}; ring < rings; ++ring) {
    for (auto seg = std::int32_t{0}; seg < segments; ++seg) {
      const auto next = (seg + 1) % segments;

      const auto i0 = base + static_cast<std::uint32_t>(ring * segments + seg);
      const auto i1 = base + static_cast<std::uint32_t>(ring * segments + next);
      const auto i2 = base + static_cast<std::uint32_t>((ring + 1) * segments + next);
      const auto i3 = base + static_cast<std::uint32_t>((ring + 1) * segments + seg);

      indices.push_back(i0);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i0);
      indices.push_back(i2);
      indices.push_back(i3);
    }
  }
}

auto triangulate_cylinder(const cylinder& shape, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  constexpr auto segments = std::int32_t{16};

  triangulate_cylinder_band(shape.radius, -shape.half_height, shape.half_height, segments, pose, vertices, indices);
  triangulate_disc_cap(shape.radius, shape.half_height, true, segments, pose, vertices, indices);
  triangulate_disc_cap(shape.radius, -shape.half_height, false, segments, pose, vertices, indices);
}

auto triangulate_capsule(const capsule& shape, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  constexpr auto segments = std::int32_t{16};
  constexpr auto rings = std::int32_t{6};

  triangulate_hemisphere_rings(shape.radius, shape.half_height, true, segments, rings, pose, vertices, indices);
  triangulate_cylinder_band(shape.radius, -shape.half_height, shape.half_height, segments, pose, vertices, indices);
  triangulate_hemisphere_rings(shape.radius, -shape.half_height, false, segments, rings, pose, vertices, indices);
}

auto triangulate_sphere(const sphere& shape, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  constexpr auto segments = std::int32_t{16};
  constexpr auto rings = std::int32_t{6};

  triangulate_hemisphere_rings(shape.radius, 0.0f, true, segments, rings, pose, vertices, indices);
  triangulate_hemisphere_rings(shape.radius, 0.0f, false, segments, rings, pose, vertices, indices);
}

auto gather_convex_shape(const convex_shape& shape, const transform& pose, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  std::visit(utility::overload(
    [&](const box& value) { triangulate_box(value, pose, vertices, indices); },
    [&](const convex_hull& value) { triangulate_convex_hull(value, pose, vertices, indices); },
    [&](const cylinder& value) { triangulate_cylinder(value, pose, vertices, indices); },
    [&](const capsule& value) { triangulate_capsule(value, pose, vertices, indices); },
    [&](const sphere& value) { triangulate_sphere(value, pose, vertices, indices); },
    [&](const auto&) { }
  ), shape);
}

auto gather_walkable_triangles(scenes::scene& scene, mesh_collision_cache& mesh_cache, convex_hull_cache& hull_cache, assets::assets_module& assets_module, pose_cache& cache, std::pmr::vector<math::vector3>& vertices, std::pmr::vector<std::uint32_t>& indices) -> void {
  for (auto&& [entity, collider] : scene.query<mesh_collider>(ecs::exclude<rigidbody, scenes::inactive>).each()) {
    auto node = scene.node_of(entity);

    if (find_owning_rigidbody(scene, node) || !collider.mesh.is_valid()) {
      continue;
    }

    const auto pose = compose_pose(compose_world_pose(scene, node, cache), collider.offset, collider.rotation);
    const auto& data = mesh_cache.get_or_build(assets_module, collider.mesh->id());

    append_triangle_mesh(data.vertices, data.indices, pose, vertices, indices);
  }

  for (auto&& [entity, body, collider] : scene.query<rigidbody, mesh_collider>(ecs::exclude<scenes::inactive>).each()) {
    if (body.type != body_type::static_body || !collider.mesh.is_valid()) {
      continue;
    }

    auto node = scene.node_of(entity);

    const auto pose = compose_pose(compose_world_pose(scene, node, cache), collider.offset, collider.rotation);
    const auto& data = mesh_cache.get_or_build(assets_module, collider.mesh->id());

    append_triangle_mesh(data.vertices, data.indices, pose, vertices, indices);
  }

  for (auto&& [entity, collider] : scene.query<shape_collider>(ecs::exclude<rigidbody, scenes::inactive>).each()) {
    static_cast<void>(collider);

    auto node = scene.node_of(entity);

    if (find_owning_rigidbody(scene, node)) {
      continue;
    }

    if (auto resolved = resolve_convex(scene, node, hull_cache, assets_module, cache)) {
      gather_convex_shape(resolved->shape, resolved->pose, vertices, indices);
    }
  }

  for (auto&& [entity, body] : scene.query<rigidbody>(ecs::exclude<mesh_collider, scenes::inactive>).each()) {
    if (body.type != body_type::static_body) {
      continue;
    }

    auto node = scene.node_of(entity);

    for (const auto& shape : resolve_body_shapes(scene, node, hull_cache, assets_module, cache)) {
      gather_convex_shape(shape.shape, shape.pose, vertices, indices);
    }
  }
}

[[nodiscard]] auto build_navmesh(const config& cfg, scenes::scene& scene) -> navmesh_build_result {
  auto& physics = core::engine::get_module<physics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto arena = bake_arena{};
  auto cache = pose_cache{};

  auto vertices = std::pmr::vector<math::vector3>{arena.temp()};
  auto indices = std::pmr::vector<std::uint32_t>{arena.temp()};

  gather_walkable_triangles(scene, physics.mesh_cache(), physics.hull_cache(), assets_module, cache, vertices, indices);

  if (vertices.empty() || indices.empty()) {
    return navmesh_build_result{};
  }

  auto bounds = math::volume::construct(vertices);

  const auto padding = math::vector3{cfg.walkable_radius * cfg.cell_size, cfg.cell_height, cfg.walkable_radius * cfg.cell_size};
  bounds = math::volume{bounds.min() - padding, bounds.max() + padding};

  auto effective_cfg = cfg;
  effective_cfg.bounds = bounds;

  const auto [width, height] = calc_grid_size(bounds, cfg.cell_size);
  effective_cfg.width = width;
  effective_cfg.height = height;

  auto hf = create_heightfield(effective_cfg.width, effective_cfg.height, effective_cfg.bounds, effective_cfg.cell_size, effective_cfg.cell_height, arena.permanent());

  rasterize_triangles(hf, vertices, indices, cfg.walkable_slope_angle);
  filter_low_hanging_walkable_obstacles(hf, cfg.walkable_climb);
  filter_ledge_spans(hf, cfg.walkable_height, cfg.walkable_climb);
  filter_walkable_low_height_spans(hf, cfg.walkable_height);

  arena.reset_temp();

  auto chf = build_compact_heightfield(cfg.walkable_height, cfg.walkable_climb, hf, arena);
  erode_walkable_area(cfg.walkable_radius, chf, arena);

  arena.reset_temp();

  if (!build_regions_monotone(chf, 0, cfg.min_region_area, cfg.merge_region_area, arena)) {
    return navmesh_build_result{};
  }

  arena.reset_temp();

  const auto contours = build_contours(chf, cfg.max_simplification_error, cfg.max_edge_length, arena);

  if (contours.contours.empty()) {
    return navmesh_build_result{};
  }

  arena.reset_temp();

  const auto pmesh = build_poly_mesh(contours, cfg.max_verts_per_poly, arena);

  if (pmesh.num_polys == 0) {
    return navmesh_build_result{};
  }

  arena.reset_temp();

  const auto dmesh = build_poly_mesh_detail(pmesh, chf, cfg.detail_sample_distance, cfg.detail_sample_max_error, arena);

  return navmesh_build_result{build_runtime_navmesh(pmesh, dmesh, static_cast<std::float_t>(cfg.walkable_climb) * cfg.cell_height), true};
}

[[nodiscard]] auto build_navmesh(const nav_settings& settings, scenes::scene& scene) -> navmesh_build_result {
  return build_navmesh(to_config(settings), scene);
}

} // namespace sbx::physics
