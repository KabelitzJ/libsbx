// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
#define LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <libsbx/math/vector3.hpp>

#include <libsbx/ecs/entity.hpp>

#include <libsbx/containers/dense_map.hpp>
#include <libsbx/containers/dynamic_tree.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/shapes.hpp>
#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/contact.hpp>
#include <libsbx/physics/physics_debug.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>

namespace sbx::physics {

/**
 * @brief One collider found overlapping a query_sphere_contacts() sphere.
 */
struct sphere_query_hit {
  scenes::node node{};
  math::vector3 point{};             // world-space contact point, on the collider's surface
  math::vector3 normal{};            // world space, points from the collider's surface toward the sphere's center
  std::float_t penetration_depth{0.0f};
}; // struct sphere_query_hit

/**
 * @brief Owns the world broadphase and the fixed-step integrate/broadphase/narrowphase/solve
 * pipeline for every rigidbody+shape_collider in the active scene. Mirrors scenes::scenes_module's
 * shape: a fixed_update() method, picked up automatically as the core::stage::fixed_update hook.
 *
 * Supports both shape_collider (convex primitives) and mesh_collider (triangle mesh, or -- with
 * mesh_collider::convex -- a cached point-set hull approximation usable by dynamic bodies too; see
 * collider.hpp's doc comment for the Unity-MeshCollider-parity rules on which body types each mode
 * allows), a rigidbody's colliders spread across its subtree (compound colliders -- see
 * narrowphase.hpp's resolve_body_shapes) at any (uniform) local_transform::scale, and a bare
 * shape_collider/mesh_collider with no rigidbody at all as its own implicit-static body (matching
 * Unity: a Collider alone is a static one). A rigidbody itself, though, must still be a root-level
 * (unparented) node: physics reads/writes its scenes::local_transform directly as if it were already
 * world space, rather than the once-per-frame-stale world_transform or a live composed one.
 */
class physics_module final : public utility::noncopyable {

  using broadphase_tree_type = containers::dynamic_tree<scenes::node>;

public:

  using dependencies = core::dependency_list<scenes::scenes_module, assets::assets_module>;

  physics_module();

  ~physics_module();

  auto fixed_update() -> void;

  /**
   * @brief Submits this frame's enabled debug-draw layers (see debug_draw_flags()) into
   * render::scene_renderer_module::debug_draw() -- picked up automatically as the
   * core::stage::late_update hook, which runs after every fixed_update() step this frame and right
   * before core::stage::render, so submissions reflect this frame's final transforms exactly once,
   * never a stale previous-frame pose and never duplicated across sub-stepping.
   */
  auto late_update() -> void;

  [[nodiscard]] auto debug_draw_flags() const noexcept -> const sbx::physics::debug_draw_flags& {
    return _debug_draw_flags;
  }

  auto set_debug_draw_flags(const sbx::physics::debug_draw_flags& flags) noexcept -> void {
    _debug_draw_flags = flags;
  }

  [[nodiscard]] auto gravity() const noexcept -> const math::vector3& {
    return _gravity;
  }

  auto set_gravity(const math::vector3& gravity) noexcept -> void {
    _gravity = gravity;
  }

  [[nodiscard]] auto velocity_iterations() const noexcept -> std::uint32_t {
    return _velocity_iterations;
  }

  auto set_velocity_iterations(std::uint32_t iterations) noexcept -> void {
    _velocity_iterations = iterations;
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
   * @brief Every collider (static or dynamic) whose surface currently overlaps a sphere of @p radius
   * centered at @p center, appended to @p out_hits (cleared first). Treats the sphere as an ad-hoc
   * convex_shape and reuses gjk_intersect + epa_penetration -- the same machinery
   * generate_pair_contact already runs for ordinary body pairs -- so this adds no new collision math,
   * only a new entry point into it. A non-convex mesh_collider candidate (resolve_convex returns
   * nullopt for one) is silently skipped -- particles don't yet collide against raw triangle meshes,
   * only convex shape_colliders and mesh_colliders authored with is_convex == true.
   */
  auto query_sphere_contacts(scenes::scene& scene, const math::vector3& center, std::float_t radius, std::vector<sphere_query_hit>& out_hits) -> void;

private:

  auto _sync_broadphase(scenes::scene& scene) -> void;

  auto _generate_candidate_pairs() -> void;

  auto _narrowphase(scenes::scene& scene) -> void;

  // Seeds each of this step's fresh (cold) manifold points from the nearest same-pair point in
  // _manifold_cache (last step's manifolds, after solving), so prepare_velocity_constraints has a
  // real impulse to warm-start from instead of always starting at zero.
  auto _warm_start_manifolds() -> void;

  // Rebuilds _manifold_cache from this step's _manifolds (whose impulse fields store_impulses has
  // by now filled in with the final solved values) -- naturally drops any pair no longer in contact.
  auto _update_manifold_cache() -> void;

  // Drops every broadphase leaf/pair/manifold. Play mode's "stop" reloads the scene in place from
  // a snapshot (scene_serializer::load over the same registry), which destroys and recreates every
  // entity -- any scenes::node this module is still holding onto from before that becomes a stale
  // handle. Called once on the false -> true edge of is_simulating() so a fresh play session always
  // starts from an empty broadphase instead of dereferencing those stale nodes.
  auto _reset() -> void;

  // Reads current collider transforms plus whatever _dynamic_tree/_static_tree/_manifolds the last
  // fixed_update() step left cached; see physics_debug.hpp for the actual wireframe generation.
  auto _submit_debug_draw(scenes::scene& scene) -> void;

  bool _was_simulating{false};

  physics::debug_draw_flags _debug_draw_flags{};

  math::vector3 _gravity{0.0f, -9.81f, 0.0f};
  std::uint32_t _velocity_iterations{8u};

  std::float_t _linear_sleep_threshold{0.02f};
  std::float_t _angular_sleep_threshold{0.05f};
  std::float_t _time_to_sleep{0.5f};

  std::float_t _position_correction_percent{0.2f};
  std::float_t _position_correction_slop{0.005f};

  // Dynamic (dynamic + kinematic) bodies are refit every step; static bodies are inserted once and
  // never refit -- the standard Box2D/Bullet broadphase split.
  broadphase_tree_type _dynamic_tree{};
  broadphase_tree_type _static_tree{};
  containers::dense_map<scenes::node, broadphase_tree_type::id> _dynamic_leaves{};
  containers::dense_map<scenes::node, broadphase_tree_type::id> _static_leaves{};

  std::vector<std::pair<scenes::node, scenes::node>> _candidate_pairs{};
  std::vector<contact_manifold> _manifolds{};

  // Last step's solved manifolds, keyed by pair, for _warm_start_manifolds to seed impulses from.
  containers::dense_map<manifold_key, contact_manifold> _manifold_cache{};

  mesh_collision_cache _mesh_cache{};
  convex_hull_cache _hull_cache{};

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
