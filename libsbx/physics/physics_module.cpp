// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/physics_module.hpp>

#include <algorithm>
#include <tuple>
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

// A pose's local_aabb-derived bounding volume, scaled then transformed into world space -- shared by
// every AABB physics_module ever hands the broadphase, whether the shape came from a body_shape
// (resolve_body_shapes/resolve_convex) or a mesh/hull's own cached local_bounds.
[[nodiscard]] auto world_bounds_aabb(const transform& pose, const math::volume& local_bounds) -> math::volume {
  const auto scaled = math::volume{local_bounds.min() * pose.scale, local_bounds.max() * pose.scale};
  return math::volume::transformed(scaled, world_pose_matrix(pose.position, pose.rotation));
}

[[nodiscard]] auto world_shape_aabb(const body_shape& shape) -> math::volume {
  return world_bounds_aabb(shape.pose, local_aabb(shape.shape));
}

physics_module::physics_module() { }

physics_module::~physics_module() { }

auto physics_module::_sync_broadphase(scenes::scene& scene) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto touched_dynamic = containers::dense_map<scenes::node, bool>{};
  auto touched_static = containers::dense_map<scenes::node, bool>{};

  // Shared by every route below: static bodies go into _static_tree once and are never refit;
  // everything else (dynamic, kinematic) is refit every step in _dynamic_tree.
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

  // Rigidbody-driven: every shape a rigidbody owns anywhere in its subtree (compound colliders),
  // unioned into one broadphase leaf keyed by the rigidbody's own node. Excludes a rigidbody that
  // carries a mesh_collider directly on itself -- that's handled by the dedicated mesh_collider pass
  // below unchanged (mixing a mesh_collider with shape_collider compound children on the same body
  // is out of scope for v1). A bare rigidbody with no collider anywhere in its subtree resolves to
  // an empty shape list and is skipped, same as always.
  for (auto&& [entity, body] : scene.query<rigidbody>(ecs::exclude<mesh_collider>).each()) {
    auto node = scene.node_of(entity);

    const auto shapes = resolve_body_shapes(scene, node, _hull_cache, assets_module);

    if (shapes.empty()) {
      continue;
    }

    auto world_box = math::volume{};

    for (const auto& shape : shapes) {
      world_box.include(world_shape_aabb(shape));
    }

    route(node, body.type, world_box);
  }

  // mesh_collider: a non-convex one can only ever be the non-simulated side of a contact (no
  // support mapping for a concave shape), so a dynamic_body carrying one is silently excluded from
  // the broadphase entirely -- see collider.hpp's doc comment. A convex one is an ordinary
  // convex_shape as far as narrowphase is concerned and gets no such restriction.
  for (auto&& [entity, body, collider] : scene.query<rigidbody, mesh_collider>().each()) {
    if (!collider.mesh.is_valid()) {
      continue;
    }

    if (body.type == body_type::dynamic_body && !collider.is_convex) {
      continue;
    }

    auto node = scene.node_of(entity);

    const auto pose = compose_pose(compose_world_pose(scene, node), collider.offset, collider.rotation);

    const auto local_bounds = collider.is_convex
      ? _hull_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds
      : _mesh_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds;

    route(node, body.type, world_bounds_aabb(pose, local_bounds));
  }

  // Implicit-static: a shape_collider/mesh_collider with no rigidbody anywhere in its own ancestor
  // chain is its own independent static body (matching Unity: a Collider alone, no Rigidbody, is a
  // static one) -- unless it's really a compound child of some ancestor's rigidbody, already
  // collected by the rigidbody-driven pass above, in which case it's skipped here.
  for (auto&& [entity, collider] : scene.query<shape_collider>(ecs::exclude<rigidbody>).each()) {
    auto node = scene.node_of(entity);

    if (find_owning_rigidbody(scene, node)) {
      continue;
    }

    if (auto resolved = resolve_convex(scene, node, _hull_cache, assets_module)) {
      route(node, body_type::static_body, world_shape_aabb(*resolved));
    }
  }

  for (auto&& [entity, collider] : scene.query<mesh_collider>(ecs::exclude<rigidbody>).each()) {
    if (!collider.mesh.is_valid()) {
      continue;
    }

    auto node = scene.node_of(entity);

    if (find_owning_rigidbody(scene, node)) {
      continue;
    }

    const auto pose = compose_pose(compose_world_pose(scene, node), collider.offset, collider.rotation);

    const auto local_bounds = collider.is_convex
      ? _hull_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds
      : _mesh_cache.get_or_build(assets_module, collider.mesh->id()).local_bounds;

    route(node, body_type::static_body, world_bounds_aabb(pose, local_bounds));
  }

  prune_stale_leaves(_dynamic_tree, _dynamic_leaves, touched_dynamic);
  prune_stale_leaves(_static_tree, _static_leaves, touched_static);
}

auto physics_module::_generate_candidate_pairs() -> void {
  _candidate_pairs.clear();

  _dynamic_tree.for_each_leaf([this](broadphase_tree_type::id leaf_id, const scenes::node& node, const math::volume& fat_aabb) {
    _dynamic_tree.query(fat_aabb, [this, leaf_id, &node](const scenes::node& other) {
      if (other == node) {
        return;
      }

      if (_dynamic_leaves.at(other) <= leaf_id) {
        return;
      }

      _candidate_pairs.emplace_back(node, other);
    });

    _static_tree.query(fat_aabb, [this, &node](const scenes::node& other) {
      _candidate_pairs.emplace_back(node, other);
    });
  });
}

auto physics_module::_warm_start_manifolds() -> void {
  // Points closer together than this, between this step's fresh geometry and the previous step's
  // cached manifold for the same pair, are considered "the same contact" for impulse carry-over.
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
        continue; // no match within range -- this point starts cold, same as before
      }

      const auto& matched = cached_points[*best_index];
      point.normal_impulse = matched.normal_impulse;
      point.tangent_impulse_1 = matched.tangent_impulse_1;
      point.tangent_impulse_2 = matched.tangent_impulse_2;
    }
  }
}

auto physics_module::_update_manifold_cache() -> void {
  auto next_cache = containers::dense_map<manifold_key, contact_manifold>{};

  for (const auto& manifold : _manifolds) {
    next_cache.emplace(make_manifold_key(manifold.node_a, manifold.node_b), manifold);
  }

  _manifold_cache = std::move(next_cache);
}

auto physics_module::_reset() -> void {
  _dynamic_tree.clear();
  _static_tree.clear();
  _dynamic_leaves.clear();
  _static_leaves.clear();
  _candidate_pairs.clear();
  _manifolds.clear();
  _manifold_cache.clear();
}

auto physics_module::_narrowphase(scenes::scene& scene) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  // A body permanently "at rest" for this pair's purposes: static bodies (never sleep, never move,
  // including a fallback body standing in for an implicit-static collider node -- see
  // effective_rigidbody) and sleeping dynamic bodies. Skip the pair entirely when both sides are --
  // there's nothing that could ever wake either one from this contact alone, so it's not just an
  // optimization: it's what guarantees that once we do reach the wake checks below, the *other*
  // body is an awake mover.
  const auto is_at_rest = [](const rigidbody& body) {
    return body.type == body_type::static_body || (body.type == body_type::dynamic_body && body.is_sleeping);
  };

  // "Genuinely moving", by the same thresholds update_sleep_timers uses to decide something is slow
  // enough to sleep. This -- not merely "not marked sleeping yet" -- is what's allowed to wake a
  // sleeping neighbor: two bodies resting against each other (e.g. a stack of boxes) almost never
  // cross their own individual sleep_timer threshold on the exact same step, so if "any awake
  // neighbor" were enough to wake a sleeper, whichever one falls asleep first would immediately get
  // rewoken by the other one still finishing its own countdown -- and then they'd swap roles and do
  // it again, forever. Gating on real motion instead means a neighbor that's merely idling through
  // the last fraction of a second before its own timer completes never wakes anything; effective_
  // inverse_mass/_inertia (solver.cpp) treating a sleeping body as immovable is what makes this safe
  // -- it still solves correctly as an anchor for whatever rests on it either way.
  const auto is_moving = [this](const rigidbody& body) {
    return body.linear_velocity.length_squared() >= _linear_sleep_threshold * _linear_sleep_threshold
      || body.angular_velocity.length_squared() >= _angular_sleep_threshold * _angular_sleep_threshold;
  };

  for (auto [node_a, node_b] : _candidate_pairs) {
    // Fresh per pair, not shared/static: harmless even so (every write to a fallback is a
    // mathematical no-op, see rigidbody.hpp's effective_rigidbody doc comment), but this avoids any
    // aliasing question between two different implicit-static pairs entirely.
    auto fallback_a = rigidbody{body_type::static_body};
    auto fallback_b = rigidbody{body_type::static_body};

    auto& body_a = effective_rigidbody(node_a, fallback_a);
    auto& body_b = effective_rigidbody(node_b, fallback_b);

    if (is_at_rest(body_a) && is_at_rest(body_b)) {
      continue;
    }

    auto manifold = generate_pair_contact(scene, node_a, node_b, _mesh_cache, _hull_cache, assets_module);

    if (!manifold) {
      continue;
    }

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

  if (!_was_simulating) {
    _reset();
  }

  _was_simulating = true;

  auto& scene = scenes_module.active_scene();
  const auto dt = core::engine::fixed_delta_time().value();

  _sync_broadphase(scene);
  _generate_candidate_pairs();

  _manifolds.clear();
  _narrowphase(scene);
  _warm_start_manifolds();

  integrate_forces(scene, _gravity, dt);

  auto constraints = prepare_velocity_constraints(_manifolds);
  solve_velocity_constraints(constraints, _velocity_iterations);
  store_impulses(constraints);

  integrate_velocities(scene, dt);
  apply_positional_correction(_manifolds, _position_correction_percent, _position_correction_slop);
  update_sleep_timers(scene, dt, _linear_sleep_threshold, _angular_sleep_threshold, _time_to_sleep);

  _update_manifold_cache();
}

auto physics_module::query_sphere_contacts(scenes::scene& scene, const math::vector3& center, std::float_t radius, std::vector<sphere_query_hit>& out_hits) -> void {
  out_hits.clear();

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto sphere_shape = convex_shape{physics::sphere{radius}};
  const auto sphere_pose = transform{center, math::quaternion::identity, math::vector3::one};

  const auto extent = math::vector3{radius, radius, radius};
  const auto query_aabb = math::volume{center - extent, center + extent};

  const auto visit = [&](const scenes::node& candidate) {
    const auto resolved = resolve_convex(scene, candidate, _hull_cache, assets_module);

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

    // epa_penetration's normal points from the sphere (a, the first shape passed above) into the
    // candidate (b) -- flip it so callers get "away from the surface", the direction they actually
    // want to push a particle or reflect its velocity along.
    out_hits.push_back(sphere_query_hit{candidate, epa.point_on_b, -epa.normal, epa.penetration_depth});
  };

  _static_tree.query(query_aabb, visit);
  _dynamic_tree.query(query_aabb, visit);
}

auto physics_module::late_update() -> void {
  SBX_PROFILE_SCOPE("physics_module::late_update");

  if (!_debug_draw_flags.colliders && !_debug_draw_flags.broadphase && !_debug_draw_flags.contacts) {
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

    // Rigidbody-driven: every shape owned anywhere in a rigidbody's subtree (compound colliders),
    // same split _sync_broadphase uses -- a rigidbody with a mesh_collider on its own node is drawn
    // by the mesh_collider loop below instead.
    for (auto&& [entity, body] : scene.query<rigidbody>(ecs::exclude<mesh_collider>).each()) {
      auto node = scene.node_of(entity);
      const auto color = debug_color_for(body.type, body.is_sleeping);

      for (const auto& shape : resolve_body_shapes(scene, node, _hull_cache, assets_module)) {
        draw_convex_shape(debug_draw, shape.shape, world_pose_matrix(shape.pose.position, shape.pose.rotation), shape.pose.scale, color);
      }
    }

    // Shared by the rigidbody-owned and implicit-static mesh_collider loops below.
    const auto draw_mesh_collider = [&](const scenes::node& node, const mesh_collider& collider, const math::color& color) {
      if (!collider.mesh.is_valid()) {
        return;
      }

      const auto pose = compose_pose(compose_world_pose(scene, node), collider.offset, collider.rotation);
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

    // Implicit-static: same "skip if a compound child of some ancestor's rigidbody" rule
    // _sync_broadphase uses, so debug draw shows exactly what's actually simulated.
    const auto implicit_static_color = debug_color_for(body_type::static_body, false);

    for (auto&& [entity, collider] : scene.query<shape_collider>(ecs::exclude<rigidbody>).each()) {
      auto node = scene.node_of(entity);

      if (find_owning_rigidbody(scene, node)) {
        continue;
      }

      if (auto resolved = resolve_convex(scene, node, _hull_cache, assets_module)) {
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

    // _dynamic_tree/_static_tree are only ever touched inside fixed_update(), which only ever runs
    // while simulating (Play) -- Stop reloads the scene in place (see physics_module.hpp's _reset()
    // doc comment), destroying and recreating every entity, so every leaf still sitting here from
    // before that is now keyed by a stale node. Rather than needing to know Stop happened at all
    // (physics_module deliberately has no notion of the editor's play/pause/stop states -- see
    // scenes_module's own doc comment), just skip drawing anything whose node isn't valid any more:
    // exactly nothing while stopped (every leaf is stale by then), everything while playing or
    // paused (the registry is untouched either way, so every leaf stays genuinely valid).
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

    // Same staleness guard as the broadphase layer above -- _manifolds is likewise only ever
    // refreshed inside fixed_update().
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
}

} // namespace sbx::physics
