// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/physics_module.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/scenes/node.hpp>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/debug/debug_draw.hpp>

#include <libsbx/physics/narrowphase.hpp>
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

physics_module::physics_module() { }

physics_module::~physics_module() { }

auto physics_module::_sync_broadphase(scenes::scene& scene) -> void {
  auto touched_dynamic = containers::dense_map<scenes::node, bool>{};
  auto touched_static = containers::dense_map<scenes::node, bool>{};

  for (auto&& [entity, body, collider, local] : scene.query<rigidbody, shape_collider, scenes::local_transform>().each()) {
    auto node = scene.node_of(entity);

    const auto pose_position = local.position + local.rotation * collider.offset;
    const auto pose_rotation = math::quaternion::normalized(local.rotation * collider.rotation);
    const auto world_box = math::volume::transformed(local_aabb(collider.shape), world_pose_matrix(pose_position, pose_rotation));

    if (body.type == body_type::static_body) {
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

auto physics_module::_narrowphase() -> void {
  // A body permanently "at rest" for this pair's purposes: static bodies (never sleep, never move)
  // and sleeping dynamic bodies. Skip the pair entirely when both sides are -- there's nothing
  // that could ever wake either one from this contact alone, so it's not just an optimization: it's
  // what guarantees that once we do reach the wake checks below, the *other* body is an awake mover.
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
    auto& body_a = node_a.get_component<rigidbody>();
    auto& body_b = node_b.get_component<rigidbody>();

    if (is_at_rest(body_a) && is_at_rest(body_b)) {
      continue;
    }

    auto manifold = generate_contact(node_a, node_b);

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
  _narrowphase();
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
    // Same query _sync_broadphase uses -- a shape_collider without a rigidbody never enters the
    // broadphase either, so drawing anything for it here would be misleading.
    for (auto&& [entity, body, collider, local] : scene.query<rigidbody, shape_collider, scenes::local_transform>().each()) {
      const auto pose_position = local.position + local.rotation * collider.offset;
      const auto pose_rotation = math::quaternion::normalized(local.rotation * collider.rotation);
      const auto matrix = world_pose_matrix(pose_position, pose_rotation);

      draw_convex_shape(debug_draw, collider.shape, matrix, debug_color_for(body.type, body.is_sleeping));
    }
  }

  if (_debug_draw_flags.broadphase) {
    const auto dynamic_color = math::color{1.0f, 1.0f, 0.0f, 1.0f};
    const auto static_color = math::color{0.6f, 0.6f, 0.0f, 1.0f};

    _dynamic_tree.for_each_leaf([&](broadphase_tree_type::id, const scenes::node&, const math::volume& fat_aabb) {
      debug_draw.add_wire_aabb(fat_aabb, dynamic_color);
    });

    _static_tree.for_each_leaf([&](broadphase_tree_type::id, const scenes::node&, const math::volume& fat_aabb) {
      debug_draw.add_wire_aabb(fat_aabb, static_color);
    });
  }

  if (_debug_draw_flags.contacts) {
    const auto contact_color = math::color{1.0f, 0.1f, 0.1f, 1.0f};
    constexpr auto normal_length = 0.3f;
    constexpr auto cross_size = 0.1f;

    for (const auto& manifold : _manifolds) {
      for (const auto& point : manifold.points) {
        debug_draw.add_cross(point.point, cross_size, contact_color);
        debug_draw.add_line(point.point, point.point + manifold.normal * normal_length, contact_color);
      }
    }
  }
}

} // namespace sbx::physics
