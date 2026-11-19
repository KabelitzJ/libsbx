// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/animation/animation_module.hpp>

#include <algorithm>
#include <optional>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/ecs/view.hpp>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/physics/character_controller.hpp>
#include <libsbx/physics/physics_module.hpp>

namespace sbx::animation {

auto animation_module::post_update() -> void {
  SBX_PROFILE_SCOPE("animation_module::post_update");

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  const auto delta_time = scenes_module.simulation_delta_time().value();

  // Scripts set IK goals first; handlers may change the scene, so this runs before any component is looked at.
  _ik_nodes.clear();

  for (auto&& [entity, animator, id] : scene.query<scenes::animator, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    if (!animator.ik_chains.empty() || !animator.look_at_joint.empty()) {
      _ik_nodes.push_back(static_cast<math::uuid>(id));
    }
  }

  for (const auto& node : _ik_nodes) {
    _on_animator_ik.emit(node);
  }

  _entries.clear();

  auto needs_ground = false;

  for (auto&& [entity, renderer, pose, world, id] : scene.query<scenes::mesh_renderer, scenes::skeleton_pose, scenes::world_transform, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    if (!renderer.mesh.is_valid() || !pose.skeleton.is_valid()) {
      continue;
    }

    auto animator = scene.node_of(entity).try_get_component<scenes::animator>();

    if (animator != nullptr) {
      needs_ground |= std::ranges::any_of(animator->ik_chains, &scenes::animator_ik_chain::foot_placement);
    }

    _entries.push_back(skinned_entry{static_cast<math::uuid>(id), memory::make_observer<const scenes::mesh_renderer>(renderer), memory::make_observer(pose), animator, world.matrix});
  }

  // Foot placement probes the physics world from the workers; only while simulating, when the broadphase holds the scene.
  auto ground = ground_query{};

  if (needs_ground && scenes_module.is_simulating()) {
    auto& physics_module = core::engine::get_module<physics::physics_module>();

    physics_module.prepare_parallel_queries(scene);

    ground = [&physics_module, &scene](const math::vector3& origin, std::float_t distance, const scenes::layer_mask& mask) -> std::optional<ground_hit> {
      const auto hit = physics_module.raycast(scene, math::ray{origin, math::vector3{0.0f, -1.0f, 0.0f}}, distance, mask);

      if (!hit) {
        return std::nullopt;
      }

      return ground_hit{hit->point, hit->normal};
    };
  }

  // Poses are independent per instance.
  static constexpr auto poses_per_chunk = std::size_t{4u};

  _scratch.resize(std::max(_scratch.size(), (_entries.size() + poses_per_chunk - 1u) / poses_per_chunk));

  core::engine::get_module<async::async_module>().pool().parallel_for_chunks(0u, _entries.size(), poses_per_chunk, [&](std::size_t begin, std::size_t end) {
    SBX_PROFILE_SCOPE("Skeleton poses");

    auto& scratch = _scratch[begin / poses_per_chunk];

    scratch.fired_events.clear();
    scratch.ground = ground;

    for (auto index = begin; index < end; ++index) {
      const auto& entry = _entries[index];
      const auto first_event = scratch.fired_events.size();

      scratch.world = entry.world;

      evaluate_skeleton_pose(*entry.pose->skeleton, *entry.renderer, entry.animator, *entry.pose, delta_time, scratch);

      for (auto event = first_event; event < scratch.fired_events.size(); ++event) {
        scratch.fired_events[event].node = entry.node;
      }
    }
  });

  // Collected before any handler runs: a handler may destroy nodes, and the entries point into their components.
  _root_motions.clear();

  for (const auto& entry : _entries) {
    if (entry.animator != nullptr && entry.animator->apply_root_motion) {
      _root_motions.push_back(root_motion_event{entry.node, entry.animator->root_motion_delta_position, entry.animator->root_motion_delta_rotation});
    }
  }

  // Chunks cover consecutive instances, so walking them in order keeps instance order.
  const auto chunk_count = (_entries.size() + poses_per_chunk - 1u) / poses_per_chunk;

  for (auto chunk = std::size_t{0u}; chunk < chunk_count; ++chunk) {
    for (const auto& event : _scratch[chunk].fired_events) {
      _on_event.emit(event);
    }

    _scratch[chunk].fired_events.clear();
  }

  for (auto& motion : _root_motions) {
    _on_animator_move.emit(motion);

    if (!motion.handled) {
      _apply_root_motion(motion);
    }
  }
}

auto animation_module::_apply_root_motion(const root_motion_event& motion) -> void {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(motion.node);

  if (!node.is_valid()) {
    return;
  }

  auto& transform = node.get_component<scenes::local_transform>();

  // A controller moves on the next fixed step, so the character still collides, climbs steps and slides along walls.
  if (auto controller = node.try_get_component<physics::character_controller>()) {
    const auto& world = node.get_component<scenes::world_transform>().matrix;

    controller->pending_motion = controller->pending_motion + math::vector3{world * math::vector4{motion.delta_position, 0.0f}};
  } else {
    transform.position = transform.position + transform.rotation * (transform.scale * motion.delta_position);
  }

  transform.rotation = math::quaternion::normalized(transform.rotation * motion.delta_rotation);
}

} // namespace sbx::animation
