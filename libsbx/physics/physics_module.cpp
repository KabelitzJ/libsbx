// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/physics_module.hpp>

#include <algorithm>
#include <span>
#include <tuple>
#include <unordered_set>
#include <utility>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/scenes/node.hpp>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/debug/debug_draw.hpp>

#include <libsbx/physics/narrowphase.hpp>
#include <libsbx/physics/epa.hpp>
#include <libsbx/physics/solver.hpp>
#include <libsbx/physics/physics_debug.hpp>
#include <libsbx/physics/raycast.hpp>

namespace sbx::physics {

auto prune_stale_leaves(containers::dynamic_tree<scenes::node>& tree, containers::dense_map<scenes::node, containers::dynamic_tree<scenes::node>::id>& leaves, const containers::dense_map<scenes::node, bool>& touched) -> void {
  auto stale = std::vector<scenes::node>{};

  for (const auto& [node, id] : leaves) {
    if (!touched.contains(node)) {
      stale.push_back(node);
    }
  }

  for (const auto& node : stale) {
    tree.remove(leaves.at(node));
    leaves.erase(node);
  }
}

[[nodiscard]] auto world_pose_matrix(const math::vector3& position, const math::quaternion& rotation) -> math::matrix4x4 {
  return math::matrix4x4::translated(math::matrix4x4::identity, position) * math::matrix_cast<math::matrix4x4>(rotation);
}

// A pose's local bounds, scaled and transformed into a world AABB for the broadphase.
[[nodiscard]] auto world_bounds_aabb(const transform& pose, const math::volume& local_bounds) -> math::volume {
  const auto scaled = math::volume{local_bounds.min() * pose.scale, local_bounds.max() * pose.scale};
  return math::volume::transformed(scaled, world_pose_matrix(pose.position, pose.rotation));
}

[[nodiscard]] auto world_shape_aabb(const body_shape& shape) -> math::volume {
  return world_bounds_aabb(shape.pose, local_aabb(shape.shape));
}

// Checks only the node's own collider, not a compound body's subtree (a v1 simplification).
[[nodiscard]] auto node_has_trigger_collider(const scenes::node& node) -> bool {
  if (auto shape = node.try_get_component<shape_collider>()) {
    return shape->is_trigger;
  }

  if (auto mesh = node.try_get_component<mesh_collider>()) {
    return mesh->is_trigger;
  }

  return false;
}

physics_module::physics_module() { }

physics_module::~physics_module() { }

auto physics_module::_sync_broadphase(scenes::scene& scene) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto touched_dynamic = containers::dense_map<scenes::node, bool>{};
  auto touched_static = containers::dense_map<scenes::node, bool>{};

  // Static bodies go into _static_tree once; dynamic and kinematic ones are refit in _dynamic_tree every step.
  const auto route = [&](const scenes::node& node, body_type type, const math::volume& world_box) {
    if (type == body_type::static_body) {
      touched_static.emplace(node, true);

      if (!_static_leaves.contains(node)) {
        _static_leaves.emplace(node, _static_tree.insert(node, world_box));
      }
    } else {
      touched_dynamic.emplace(node, true);

      if (const auto entry = _dynamic_leaves.find(node); entry != _dynamic_leaves.end()) {
        [[maybe_unused]] auto exists = _dynamic_tree.update(entry->second, world_box);
      } else {
        _dynamic_leaves.emplace(node, _dynamic_tree.insert(node, world_box));
      }
    }
  };

  // Rigidbodies: every shape in the subtree, unioned into one leaf. Bodies with their own mesh_collider are handled below; a body without colliders is skipped.
  for (auto&& [entity, body] : scene.query<rigidbody>(ecs::exclude<mesh_collider, scenes::inactive>).each()) {
    auto node = scene.node_of(entity);

    const auto shapes = resolve_body_shapes(scene, node, _hull_cache, assets_module, _pose_cache);

    if (shapes.empty()) {
      continue;
    }

    auto world_box = math::volume{};

    for (const auto& shape : shapes) {
      world_box.include(world_shape_aabb(shape));
    }

    route(node, body.type, world_box);
  }

  // A non-convex mesh_collider has no support mapping, so dynamic bodies carrying one are excluded; convex ones are ordinary shapes.
  for (auto&& [entity, body, collider] : scene.query<rigidbody, mesh_collider>(ecs::exclude<scenes::inactive>).each()) {
    if (!collider.mesh.is_valid()) {
      continue;
    }

    if (body.type == body_type::dynamic_body && !collider.is_convex) {
      continue;
    }

    auto node = scene.node_of(entity);

    const auto pose = compose_pose(compose_world_pose(scene, node, _pose_cache), collider.offset, collider.rotation);

    const auto local_bounds = collider.is_convex
      ? _hull_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds
      : _mesh_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds;

    route(node, body.type, world_bounds_aabb(pose, local_bounds));
  }

  // A collider without a rigidbody ancestor is its own static body, like Unity; compound children of an ancestor's body were collected above.
  for (auto&& [entity, collider] : scene.query<shape_collider>(ecs::exclude<rigidbody, scenes::inactive>).each()) {
    auto node = scene.node_of(entity);

    if (find_owning_rigidbody(scene, node)) {
      continue;
    }

    if (auto resolved = resolve_convex(scene, node, _hull_cache, assets_module, _pose_cache)) {
      route(node, body_type::static_body, world_shape_aabb(*resolved));
    }
  }

  for (auto&& [entity, collider] : scene.query<mesh_collider>(ecs::exclude<rigidbody, scenes::inactive>).each()) {
    if (!collider.mesh.is_valid()) {
      continue;
    }

    auto node = scene.node_of(entity);

    if (find_owning_rigidbody(scene, node)) {
      continue;
    }

    const auto pose = compose_pose(compose_world_pose(scene, node, _pose_cache), collider.offset, collider.rotation);

    const auto local_bounds = collider.is_convex
      ? _hull_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds
      : _mesh_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds;

    route(node, body_type::static_body, world_bounds_aabb(pose, local_bounds));
  }

  // Heightfields get their own list: only raycast() uses them, and their huge AABB would match nearly every body as a candidate pair.
  _heightfield_nodes.clear();

  for (auto&& [entity, collider] : scene.query<heightfield_collider>(ecs::exclude<scenes::inactive>).each()) {
    if (collider.data) {
      _heightfield_nodes.push_back(scene.node_of(entity));
    }
  }

  prune_stale_leaves(_dynamic_tree, _dynamic_leaves, touched_dynamic);
  prune_stale_leaves(_static_tree, _static_leaves, touched_static);
}

auto physics_module::_generate_candidate_pairs() -> void {
  _candidate_pairs.clear();

  // The layer collision matrix: non-colliding pairs never become candidates.
  auto& project = core::engine::project();

  const auto layers_collide = [&project](const scenes::node& a, const scenes::node& b) {
    return project.layers_collide(a.get_component<scenes::layer>().index, b.get_component<scenes::layer>().index);
  };

  _dynamic_tree.for_each_leaf([this, &layers_collide](broadphase_tree_type::id leaf_id, const scenes::node& node, const math::volume& fat_aabb) {
    _dynamic_tree.query(fat_aabb, [this, leaf_id, &node, &layers_collide](const scenes::node& other) {
      if (other == node) {
        return;
      }

      if (_dynamic_leaves.at(other) <= leaf_id) {
        return;
      }

      if (!layers_collide(node, other)) {
        return;
      }

      _candidate_pairs.emplace_back(node, other);
    });

    _static_tree.query(fat_aabb, [this, &node, &layers_collide](const scenes::node& other) {
      if (!layers_collide(node, other)) {
        return;
      }

      _candidate_pairs.emplace_back(node, other);
    });
  });
}

auto physics_module::_warm_start_manifolds() -> void {
  // Points this close to last step's point of the same pair count as the same contact for impulse carry-over.
  constexpr auto match_distance_squared = 0.05f * 0.05f;

  for (auto& manifold : _manifolds) {
    const auto cached = _manifold_cache.find(make_manifold_key(manifold.node_a, manifold.node_b));

    if (cached == _manifold_cache.end()) {
      continue;
    }

    const auto& cached_points = cached->second.points;

    for (auto& point : manifold.points) {
      auto best_index = std::optional<std::size_t>{};
      auto best_distance_squared = match_distance_squared;

      for (auto index = std::size_t{0}; index < cached_points.size(); ++index) {
        const auto distance_squared = math::vector3::distance_squared(point.point, cached_points[index].point);

        if (distance_squared <= best_distance_squared) {
          best_distance_squared = distance_squared;
          best_index = index;
        }
      }

      if (!best_index) {
        continue; // no match in range: starts cold
      }

      const auto& matched = cached_points[*best_index];
      point.normal_impulse = matched.normal_impulse;
      point.tangent_impulse_1 = matched.tangent_impulse_1;
      point.tangent_impulse_2 = matched.tangent_impulse_2;
    }
  }
}

auto physics_module::_dispatch_contact_events() -> void {
  auto seen_this_step = std::unordered_set<manifold_key>{};

  for (const auto& manifold : _manifolds) {
    const auto key = make_manifold_key(manifold.node_a, manifold.node_b);
    seen_this_step.insert(key);

    if (_manifold_cache.contains(key)) {
      continue; // already touching last step
    }

    const auto event = collision_event{
      manifold.node_a,
      manifold.node_b,
      manifold.normal,
      manifold.points.is_empty() ? math::vector3::zero : manifold.points.front().point,
      manifold.is_trigger
    };

    _on_contact_began(event);
  }

  for (const auto& [key, cached_manifold] : _manifold_cache) {
    if (seen_this_step.contains(key)) {
      continue; // still touching this step
    }

    // Either node may have been destroyed since last step.
    if (!cached_manifold.node_a.is_valid() || !cached_manifold.node_b.is_valid()) {
      continue;
    }

    const auto event = collision_event{cached_manifold.node_a, cached_manifold.node_b, cached_manifold.normal, math::vector3::zero, cached_manifold.is_trigger};

    _on_contact_ended(event);
  }
}

auto physics_module::_update_manifold_cache() -> void {
  auto next_cache = containers::dense_map<manifold_key, contact_manifold>{};

  for (const auto& manifold : _manifolds) {
    next_cache.emplace(make_manifold_key(manifold.node_a, manifold.node_b), manifold);
  }

  _manifold_cache = std::move(next_cache);
}

auto physics_module::_reset(scenes::scene& scene) -> void {
  _dynamic_tree.clear();
  _static_tree.clear();
  _dynamic_leaves.clear();
  _static_leaves.clear();
  _heightfield_nodes.clear();
  _candidate_pairs.clear();
  _manifolds.clear();
  _manifold_cache.clear();

  if (_pending_nav_settings) {
    bake_navmesh(scene, *_pending_nav_settings);
  } else {
    _navmesh.reset();
  }
}

auto physics_module::bake_navmesh(scenes::scene& scene, const physics::nav_settings& settings) -> bool {
  _pending_nav_settings = settings;

  auto result = build_navmesh(settings, scene);

  if (result.success) {
    _navmesh = std::move(result.mesh);
  } else {
    _navmesh.reset();
  }

  return result.success;
}

auto physics_module::request_agent_move(scenes::node agent_node, const math::vector3& target) -> bool {
  if (!_navmesh) {
    return false;
  }

  auto agent = agent_node.try_get_component<nav_agent>();

  if (!agent) {
    return false;
  }

  return _crowd.request_move_target(*agent, *_navmesh, target);
}

auto physics_module::_narrowphase(scenes::scene& scene) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  // Static and sleeping bodies; pairs where both are at rest are skipped, so the wake checks below always see an awake mover.
  const auto is_at_rest = [](const rigidbody& body) {
    return body.type == body_type::static_body || (body.type == body_type::dynamic_body && body.is_sleeping);
  };

  // Only real motion (the sleep thresholds) wakes a sleeper: otherwise two resting bodies whose timers expire on different steps would keep waking each other forever.
  const auto is_moving = [this](const rigidbody& body) {
    return body.linear_velocity.length_squared() >= _linear_sleep_threshold * _linear_sleep_threshold
      || body.angular_velocity.length_squared() >= _angular_sleep_threshold * _angular_sleep_threshold;
  };

  for (auto [node_a, node_b] : _candidate_pairs) {
    // Fresh per pair to avoid any aliasing between implicit-static pairs.
    auto fallback_a = rigidbody{body_type::static_body};
    auto fallback_b = rigidbody{body_type::static_body};

    auto& body_a = effective_rigidbody(node_a, fallback_a);
    auto& body_b = effective_rigidbody(node_b, fallback_b);

    if (is_at_rest(body_a) && is_at_rest(body_b)) {
      continue;
    }

    auto manifold = generate_pair_contact(scene, node_a, node_b, _mesh_cache, _hull_cache, assets_module, _pose_cache);

    if (!manifold) {
      continue;
    }

    manifold->is_trigger = node_has_trigger_collider(node_a) || node_has_trigger_collider(node_b);

    if (body_a.is_sleeping && is_moving(body_b)) {
      body_a.is_sleeping = false;
      body_a.sleep_timer = 0.0f;
    }

    if (body_b.is_sleeping && is_moving(body_a)) {
      body_b.is_sleeping = false;
      body_b.sleep_timer = 0.0f;
    }

    _manifolds.push_back(std::move(*manifold));
  }
}

auto physics_module::fixed_update() -> void {
  SBX_PROFILE_SCOPE("physics_module::fixed_update");

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (!scenes_module.is_simulating()) {
    _was_simulating = false;
    return;
  }

  auto& scene = scenes_module.active_scene();

  if (!_was_simulating) {
    _reset(scene);
  }

  _was_simulating = true;

  const auto dt = core::engine::fixed_delta_time().value();

  // Each node's world pose is composed at most once this step.
  _pose_cache.clear();

  _sync_broadphase(scene);
  _generate_candidate_pairs();

  _manifolds.clear();
  _narrowphase(scene);
  _warm_start_manifolds();

  const auto& project = core::engine::project();

  integrate_forces(scene, project.gravity(), dt);

  // Triggers are moved to the back (stable) and excluded from the solver; they stay in _manifolds for events and caching.
  const auto trigger_begin = std::ranges::stable_partition(_manifolds, [](const auto& manifold) { return !manifold.is_trigger; }).begin();
  const auto solid_count = static_cast<std::size_t>(trigger_begin - _manifolds.begin());
  const auto solid_manifolds = std::span<contact_manifold>{_manifolds.data(), solid_count};

  auto constraints = prepare_velocity_constraints(solid_manifolds);
  solve_velocity_constraints(constraints, project.velocity_iterations());
  store_impulses(constraints);

  integrate_velocities(scene, dt);
  apply_positional_correction(solid_manifolds, _position_correction_percent, _position_correction_slop);
  update_sleep_timers(scene, dt, _linear_sleep_threshold, _angular_sleep_threshold, _time_to_sleep);

  _dispatch_contact_events();
  _update_manifold_cache();

  if (_navmesh) {
    _crowd.update(scene, *_navmesh, dt);
  }
}

auto physics_module::query_sphere_contacts(scenes::scene& scene, const math::vector3& center, std::float_t radius, std::vector<sphere_query_hit>& out_hits, const scenes::layer_mask& mask) -> void {
  out_hits.clear();

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto sphere_shape = convex_shape{physics::sphere{radius}};
  const auto sphere_pose = transform{center, math::quaternion::identity, math::vector3::one};

  const auto extent = math::vector3{radius, radius, radius};
  const auto query_aabb = math::volume{center - extent, center + extent};

  // A local cache: queries run off the fixed cadence and must not read stale poses.
  auto cache = pose_cache{};

  const auto visit = [&](const scenes::node& candidate) {
    if (!mask.test(candidate.get_component<scenes::layer>().index)) {
      return;
    }

    const auto resolved = resolve_convex(scene, candidate, _hull_cache, assets_module, cache);

    if (!resolved) {
      return;
    }

    const auto gjk = gjk_intersect(sphere_shape, sphere_pose, resolved->shape, resolved->pose);

    if (!gjk.intersecting) {
      return;
    }

    const auto epa = epa_penetration(sphere_shape, sphere_pose, resolved->shape, resolved->pose, gjk.simplex);

    if (!epa.valid) {
      return;
    }

    // EPA's normal points from the sphere into the candidate; flip it to point away from the surface.
    out_hits.push_back(sphere_query_hit{candidate, epa.point_on_b, -epa.normal, epa.penetration_depth});
  };

  _static_tree.query(query_aabb, visit);
  _dynamic_tree.query(query_aabb, visit);
}

auto physics_module::raycast(scenes::scene& scene, const math::ray& ray, std::float_t max_distance, const scenes::layer_mask& mask) -> std::optional<raycast_hit> {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto nearest = std::optional<raycast_hit>{};

  const auto consider = [&](const scenes::node& candidate, const shape_raycast_hit& hit) {
    if (!nearest || hit.distance < nearest->distance) {
      nearest = raycast_hit{candidate, hit.point, hit.normal, hit.distance};
    }
  };

  // Heightfields aren't in _static_tree; there are usually 0 or 1.
  for (const auto& node : _heightfield_nodes) {
    if (!mask.test(node.get_component<scenes::layer>().index)) {
      continue;
    }

    if (const auto& collider = node.get_component<heightfield_collider>(); collider.data) {
      if (const auto hit = raycast_heightfield(*collider.data, ray, max_distance)) {
        consider(node, *hit);
      }
    }
  }

  // A local cache: raycasts run off the fixed cadence and must not read stale poses.
  auto cache = pose_cache{};

  const auto visit = [&](const scenes::node& candidate, [[maybe_unused]] std::float_t entry_t) {
    if (!mask.test(candidate.get_component<scenes::layer>().index)) {
      return;
    }

    const auto resolved = resolve_convex(scene, candidate, _hull_cache, assets_module, cache);

    if (!resolved) {
      return; // no collider, or a non-convex mesh_collider
    }

    if (const auto hit = raycast_convex_shape(resolved->shape, resolved->pose, ray, max_distance)) {
      consider(candidate, *hit);
    }
  };

  _static_tree.query(ray, visit);
  _dynamic_tree.query(ray, visit);

  return nearest;
}

auto physics_module::late_update() -> void {
  SBX_PROFILE_SCOPE("physics_module::late_update");

  if (!_debug_draw_flags.colliders && !_debug_draw_flags.broadphase && !_debug_draw_flags.contacts && !_debug_draw_flags.navmesh && !_debug_draw_flags.nav_agents) {
    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  _submit_debug_draw(scenes_module.active_scene());
}

auto physics_module::_submit_debug_draw(scenes::scene& scene) -> void {
  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  auto& debug_draw = scene_renderer_module.debug_draw();

  if (_debug_draw_flags.colliders) {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    // A local cache: debug draw runs at render cadence.
    auto cache = pose_cache{};

    // Rigidbodies: every shape in the subtree; bodies with their own mesh_collider are drawn below.
    for (auto&& [entity, body] : scene.query<rigidbody>(ecs::exclude<mesh_collider>).each()) {
      auto node = scene.node_of(entity);
      const auto color = debug_color_for(body.type, body.is_sleeping);

      for (const auto& shape : resolve_body_shapes(scene, node, _hull_cache, assets_module, cache)) {
        draw_convex_shape(debug_draw, shape.shape, world_pose_matrix(shape.pose.position, shape.pose.rotation), shape.pose.scale, color);
      }
    }

    const auto draw_mesh_collider = [&](const scenes::node& node, const mesh_collider& collider, const math::color& color) {
      if (!collider.mesh.is_valid()) {
        return;
      }

      const auto pose = compose_pose(compose_world_pose(scene, node, cache), collider.offset, collider.rotation);
      const auto matrix = world_pose_matrix(pose.position, pose.rotation);

      if (collider.is_convex) {
        const auto& hull_data = _hull_cache.get_or_build(assets_module, collider.mesh->id());

        draw_convex_shape(debug_draw, convex_shape{convex_hull{hull_data.points, hull_data.faces}}, matrix, pose.scale, color);
      } else {
        const auto& mesh_data = _mesh_cache.get_or_build(assets_module, collider.mesh->id());
        const auto triangle_count = mesh_data.indices.size() / 3u;

        for (auto triangle_index = std::size_t{0}; triangle_index < triangle_count; ++triangle_index) {
          const auto i0 = mesh_data.indices[triangle_index * 3u + 0u];
          const auto i1 = mesh_data.indices[triangle_index * 3u + 1u];
          const auto i2 = mesh_data.indices[triangle_index * 3u + 2u];

          const auto v0 = math::vector3{matrix * math::vector4{mesh_data.vertices[i0] * pose.scale, 1.0f}};
          const auto v1 = math::vector3{matrix * math::vector4{mesh_data.vertices[i1] * pose.scale, 1.0f}};
          const auto v2 = math::vector3{matrix * math::vector4{mesh_data.vertices[i2] * pose.scale, 1.0f}};

          debug_draw.add_line(v0, v1, color);
          debug_draw.add_line(v1, v2, color);
          debug_draw.add_line(v2, v0, color);
        }
      }
    };

    for (auto&& [entity, body, collider] : scene.query<rigidbody, mesh_collider>().each()) {
      draw_mesh_collider(scene.node_of(entity), collider, debug_color_for(body.type, body.is_sleeping));
    }

    // Same compound-child rule as _sync_broadphase, so debug draw matches the simulation.
    const auto implicit_static_color = debug_color_for(body_type::static_body, false);

    for (auto&& [entity, collider] : scene.query<shape_collider>(ecs::exclude<rigidbody>).each()) {
      auto node = scene.node_of(entity);

      if (find_owning_rigidbody(scene, node)) {
        continue;
      }

      if (auto resolved = resolve_convex(scene, node, _hull_cache, assets_module, cache)) {
        draw_convex_shape(debug_draw, resolved->shape, world_pose_matrix(resolved->pose.position, resolved->pose.rotation), resolved->pose.scale, implicit_static_color);
      }
    }

    for (auto&& [entity, collider] : scene.query<mesh_collider>(ecs::exclude<rigidbody>).each()) {
      auto node = scene.node_of(entity);

      if (find_owning_rigidbody(scene, node)) {
        continue;
      }

      draw_mesh_collider(node, collider, implicit_static_color);
    }
  }

  if (_debug_draw_flags.broadphase) {
    const auto dynamic_color = math::color{1.0f, 1.0f, 0.0f, 1.0f};
    const auto static_color = math::color{0.6f, 0.6f, 0.0f, 1.0f};

    // The trees only change during Play, and Stop recreates every entity; skipping invalid nodes draws nothing after Stop and everything while playing or paused.
    _dynamic_tree.for_each_leaf([&](broadphase_tree_type::id, const scenes::node& node, const math::volume& fat_aabb) {
      if (node.is_valid()) {
        debug_draw.add_wire_aabb(fat_aabb, dynamic_color);
      }
    });

    _static_tree.for_each_leaf([&](broadphase_tree_type::id, const scenes::node& node, const math::volume& fat_aabb) {
      if (node.is_valid()) {
        debug_draw.add_wire_aabb(fat_aabb, static_color);
      }
    });
  }

  if (_debug_draw_flags.contacts) {
    const auto contact_color = math::color{1.0f, 0.1f, 0.1f, 1.0f};
    constexpr auto normal_length = 0.3f;
    constexpr auto cross_size = 0.1f;

    // Same staleness guard: _manifolds only changes during fixed_update().
    for (const auto& manifold : _manifolds) {
      if (!manifold.node_a.is_valid() || !manifold.node_b.is_valid()) {
        continue;
      }

      for (const auto& point : manifold.points) {
        debug_draw.add_cross(point.point, cross_size, contact_color);
        debug_draw.add_line(point.point, point.point + manifold.normal * normal_length, contact_color);
      }
    }
  }

  if (_debug_draw_flags.navmesh && _navmesh) {
    const auto navmesh_color = math::color{0.1f, 0.6f, 1.0f, 1.0f};
    const auto navmesh_boundary_color = math::color{1.0f, 0.15f, 0.1f, 1.0f};

    draw_navmesh(debug_draw, *_navmesh, navmesh_color, navmesh_boundary_color);
  }

  if (_debug_draw_flags.nav_agents) {
    const auto agent_color = math::color{1.0f, 0.8f, 0.1f, 1.0f};

    for (auto&& [entity, agent, node_transform] : scene.query<nav_agent, scenes::local_transform>().each()) {
      static_cast<void>(entity);

      draw_nav_agent(debug_draw, agent, node_transform.position, agent_color);
    }
  }
}

} // namespace sbx::physics
