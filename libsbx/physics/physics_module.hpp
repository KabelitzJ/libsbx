// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
#define LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/ecs/entity.hpp>

#include <libsbx/containers/dense_map.hpp>
#include <libsbx/containers/dynamic_tree.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/shapes.hpp>
#include <libsbx/physics/character_controller.hpp>
#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/contact.hpp>
#include <libsbx/physics/physics_debug.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>
#include <libsbx/physics/narrowphase.hpp>
#include <libsbx/physics/shape_cast.hpp>
#include <libsbx/physics/nav/crowd.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>
#include <libsbx/physics/nav/navmesh.hpp>
#include <libsbx/physics/nav/navmesh_builder.hpp>

namespace sbx::physics {

/** @brief One collider overlapping a query_sphere_contacts() or overlap_shape() query shape. */
struct sphere_query_hit {
  scenes::node node{};
  math::vector3 point{};             // world space, on the collider's surface
  math::vector3 normal{};            // world space, from the collider's surface toward the sphere's center
  std::float_t penetration_depth{0.0f};
}; // struct sphere_query_hit

/** @brief The nearest collider a physics_module::raycast() ray hit. */
struct raycast_hit {
  scenes::node node{};
  math::vector3 point{};   // world space
  math::vector3 normal{};  // world space, outward from the collider's surface
  std::float_t distance{0.0f};
}; // struct raycast_hit

/**
 * @brief Owns the broadphase and the fixed-step integrate, broadphase, narrowphase and solve pipeline for the active scene.
 *
 * Supports shape colliders, mesh colliders (triangle mesh, or a convex hull usable by dynamic bodies; see collider.hpp), compound colliders across a rigidbody's subtree at uniform scale, and bare colliders as implicit static bodies, like Unity.
 * A rigidbody must be a root node: its local_transform is read and written as world space.
 */
struct sweep_hit {
  scenes::node node{};
  math::vector3 point{};   // world space, on the hit collider
  math::vector3 normal{};  // world space, the collider's surface normal, toward the cast shape
  std::float_t distance{0.0f}; // along the cast direction
  bool initially_overlapping{false}; // the shape started inside this collider; normal is then the push-out direction
}; // struct sweep_hit

class physics_module final : public utility::noncopyable {

  using broadphase_tree_type = containers::dynamic_tree<scenes::node>;

public:

  using dependencies = core::dependency_list<scenes::scenes_module, assets::assets_module>;

  physics_module();

  ~physics_module();

  auto fixed_update() -> void;

  /** @brief Submits the enabled debug-draw layers once per frame, after every fixed step, so they show this frame's final transforms. */
  auto late_update() -> void;

  [[nodiscard]] auto debug_draw_flags() const noexcept -> const sbx::physics::debug_draw_flags& {
    return _debug_draw_flags;
  }

  auto set_debug_draw_flags(const sbx::physics::debug_draw_flags& flags) noexcept -> void {
    _debug_draw_flags = flags;
  }

  [[nodiscard]] auto linear_sleep_threshold() const noexcept -> std::float_t {
    return _linear_sleep_threshold;
  }

  auto set_linear_sleep_threshold(std::float_t threshold) noexcept -> void {
    _linear_sleep_threshold = threshold;
  }

  [[nodiscard]] auto angular_sleep_threshold() const noexcept -> std::float_t {
    return _angular_sleep_threshold;
  }

  auto set_angular_sleep_threshold(std::float_t threshold) noexcept -> void {
    _angular_sleep_threshold = threshold;
  }

  [[nodiscard]] auto time_to_sleep() const noexcept -> std::float_t {
    return _time_to_sleep;
  }

  auto set_time_to_sleep(std::float_t seconds) noexcept -> void {
    _time_to_sleep = seconds;
  }

  /**
   * @brief Makes the queries safe to call from several threads at once until the next fixed_update: builds the broadphase if simulation just started.
   *
   * The shape caches lock themselves, the trees are only read, and each query composes poses into its own cache.
   *
   * @param scene The scene the queries will run against.
   */
  auto prepare_parallel_queries(scenes::scene& scene) -> void;

  /**
   * @brief Every collider overlapping a sphere; overlap_shape() with a sphere, triggers included.
   *
   * @param scene The scene.
   * @param center The sphere's center.
   * @param radius The sphere's radius.
   * @param out_hits Cleared, then receives the hits.
   * @param mask Only colliders on these layers are tested.
   */
  auto query_sphere_contacts(scenes::scene& scene, const math::vector3& center, std::float_t radius, std::vector<sphere_query_hit>& out_hits, const scenes::layer_mask& mask = scenes::layer_mask::everything()) -> void;

  /**
   * @brief The nearest collider @p ray hits within @p max_distance: heightfields, convex shapes and non-convex meshes.
   *
   * @param scene The scene.
   * @param ray The world-space ray.
   * @param max_distance The maximum distance.
   * @param mask Only colliders on these layers are tested.
   *
   * @return The hit, or nullopt.
   */
  [[nodiscard]] auto raycast(scenes::scene& scene, const math::ray& ray, std::float_t max_distance, const scenes::layer_mask& mask = scenes::layer_mask::everything()) -> std::optional<raycast_hit>;

  /**
   * @brief The first collider @p shape hits when swept from @p pose along @p direction, rotation held fixed. Only valid while simulating, like raycast().
   *
   * @param direction Need not be normalized.
   * @param ignore A node to skip together with its whole subtree, typically the caster itself.
   * @param include_triggers Whether trigger colliders count as hits.
   * @param include_dynamic Whether dynamic and kinematic bodies count as hits; false leaves static colliders, meshes and terrain.
   *
   * @return The nearest hit, or nullopt.
   */
  [[nodiscard]] auto shape_cast(scenes::scene& scene, const convex_shape& shape, const transform& pose, const math::vector3& direction, std::float_t max_distance, const scenes::layer_mask& mask = scenes::layer_mask::everything(), const scenes::node& ignore = scenes::node{}, bool include_triggers = true, bool include_dynamic = true) -> std::optional<sweep_hit>;

  /**
   * @brief Every collider overlapping @p shape at @p pose, with a push-out normal and depth per hit. A non-convex mesh or terrain yields one hit per overlapping triangle.
   *
   * @param out_hits Cleared, then receives the hits.
   * @param ignore A node to skip together with its whole subtree.
   */
  auto overlap_shape(scenes::scene& scene, const convex_shape& shape, const transform& pose, std::vector<sphere_query_hit>& out_hits, const scenes::layer_mask& mask = scenes::layer_mask::everything(), const scenes::node& ignore = scenes::node{}, bool include_triggers = true) -> void;

  /**
   * @brief Fires once per pair on the fixed step a contact or trigger overlap begins; scripting_module delivers OnCollisionEnter/OnTriggerEnter from it.
   *
   * @return The signal.
   */
  /**
   * @brief Moves a character_controller node by @p displacement right away: pushes out of overlaps, climbs steps, slides along walls and snaps down onto the ground.
   *
   * Call once per fixed step (script FixedUpdate): velocity is the move divided by engine::fixed_delta_time().
   * Updates the controller's runtime fields and fires on_controller_hit() for each surface hit. Collides with the layers the node's layer collides with; triggers are ignored.
   *
   * @return The collision_below/above/sides flags of this move; 0 for a node without a controller.
   */
  auto move_character(scenes::scene& scene, scenes::node node, const math::vector3& displacement) -> std::uint8_t;

  /**
   * @brief Like Unity's SimpleMove: moves at @p speed (m/s; its vertical part ignored) for one fixed step, with the project's gravity accumulated while airborne. Call from FixedUpdate.
   *
   * @return Whether the controller is grounded afterwards.
   */
  auto simple_move_character(scenes::scene& scene, scenes::node node, const math::vector3& speed) -> bool;

  auto on_controller_hit() -> signals::signal<const controller_hit&>& {
    return _on_controller_hit;
  }

  auto on_contact_began() -> signals::signal<const collision_event&>& {
    return _on_contact_began;
  }

  /**
   * @brief Fires once per pair on the step it stops touching.
   *
   * @return The signal.
   */
  auto on_contact_ended() -> signals::signal<const collision_event&>& {
    return _on_contact_ended;
  }

  [[nodiscard]] auto nav_settings() const noexcept -> const std::optional<physics::nav_settings>& {
    return _pending_nav_settings;
  }

  /**
   * @brief Bakes the navmesh from the active static geometry now (Edit mode included) and keeps @p settings for the next Play-start rebake. A failed bake clears the previous navmesh.
   *
   * @param scene The scene.
   * @param settings The bake settings.
   *
   * @return Whether any usable polygons were produced.
   */
  auto bake_navmesh(scenes::scene& scene, const physics::nav_settings& settings) -> bool;

  [[nodiscard]] auto has_navmesh() const noexcept -> bool {
    return _navmesh.has_value();
  }

  [[nodiscard]] auto navmesh() const -> const physics::navmesh& {
    return *_navmesh;
  }

  /**
   * @brief Paths @p agent_node's nav_agent to @p target over the navmesh; movement happens in fixed_update().
   *
   * @param agent_node The agent's node.
   * @param target The destination.
   *
   * @return False if there's no navmesh or no nav_agent.
   */
  auto request_agent_move(scenes::node agent_node, const math::vector3& target) -> bool;

  [[nodiscard]] auto mesh_cache() -> mesh_collision_cache& {
    return _mesh_cache;
  }

  [[nodiscard]] auto hull_cache() -> convex_hull_cache& {
    return _hull_cache;
  }

private:

  auto _sync_broadphase(scenes::scene& scene) -> void;

  auto _generate_candidate_pairs() -> void;

  auto _narrowphase(scenes::scene& scene) -> void;

  // Seeds this step's new manifold points from the nearest cached point of the same pair, so the solver warm-starts.
  auto _warm_start_manifolds() -> void;

  // Rebuilds the cache from this step's solved manifolds, dropping pairs no longer in contact.
  auto _update_manifold_cache() -> void;

  // Diffs this step's manifolds against the cache to fire began/ended events; must run before _update_manifold_cache.
  auto _dispatch_contact_events() -> void;

  // Where a continuous body must stop this step instead of its integrated position.
  struct continuous_clamp {
    scenes::node node;
    math::vector3 start_position; // local_transform::position before integration
    math::vector3 direction;
    std::float_t travel;
  }; // struct continuous_clamp

  // Sweeps fast continuous bodies along this step's displacement against static geometry; call after the velocity solve, before integration.
  [[nodiscard]] auto _sweep_continuous_bodies(scenes::scene& scene, std::float_t dt) -> std::vector<continuous_clamp>;

  // Queries can run before the first physics step of a play session (e.g. a Move in OnCreate): fills the broadphase then, instead of answering from empty trees.
  auto _ensure_broadphase(scenes::scene& scene) -> void;

  // Drops every leaf, pair and manifold when simulation starts: Stop reloads the scene in place, so held nodes are stale.
  auto _reset(scenes::scene& scene) -> void;

  // Debug wireframes for the current colliders and the last step's trees and manifolds.
  auto _submit_debug_draw(scenes::scene& scene) -> void;

  bool _was_simulating{false};
  bool _reset_pending{false};

  physics::debug_draw_flags _debug_draw_flags{};


  std::float_t _linear_sleep_threshold{0.02f};
  std::float_t _angular_sleep_threshold{0.05f};
  std::float_t _time_to_sleep{0.5f};

  std::float_t _position_correction_percent{0.2f};
  std::float_t _position_correction_slop{0.005f};

  // Dynamic and kinematic bodies are refit every step; static ones are inserted once.
  broadphase_tree_type _dynamic_tree{};
  broadphase_tree_type _static_tree{};
  containers::dense_map<scenes::node, broadphase_tree_type::id> _dynamic_leaves{};
  containers::dense_map<scenes::node, broadphase_tree_type::id> _static_leaves{};

  // Heightfields stay out of _static_tree; see _sync_broadphase.
  std::vector<scenes::node> _heightfield_nodes{};

  std::vector<std::pair<scenes::node, scenes::node>> _candidate_pairs{};
  std::vector<contact_manifold> _manifolds{};

  // Last step's solved manifolds, keyed by pair, for _warm_start_manifolds to seed impulses from.
  containers::dense_map<manifold_key, contact_manifold> _manifold_cache{};

  mesh_collision_cache _mesh_cache{};
  convex_hull_cache _hull_cache{};

  std::optional<physics::nav_settings> _pending_nav_settings{};
  std::optional<physics::navmesh> _navmesh{};
  physics::crowd _crowd{};

  // Composed world poses for this step, shared by broadphase and narrowphase.
  pose_cache _pose_cache{};

  signals::signal<const collision_event&> _on_contact_began{};
  signals::signal<const collision_event&> _on_contact_ended{};
  signals::signal<const controller_hit&> _on_controller_hit{};

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
