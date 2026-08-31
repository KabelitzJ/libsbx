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

namespace sbx::physics {

/**
 * @brief Owns the world broadphase and the fixed-step integrate/broadphase/narrowphase/solve
 * pipeline for every rigidbody+shape_collider in the active scene. Mirrors scenes::scenes_module's
 * shape: a fixed_update() method, picked up automatically as the core::stage::fixed_update hook.
 *
 * v1 scope: shape_collider (convex primitive) bodies only -- mesh_collider narrowphase is not yet
 * wired in (mesh_collision_cache exists and works standalone, just unused here for now). Physics
 * only correctly supports root-level (unparented) nodes: it reads/writes scenes::local_transform
 * directly rather than the once-per-frame-stale world_transform, and ignores local_transform::scale.
 */
class physics_module final : public utility::noncopyable {

  using broadphase_tree_type = containers::dynamic_tree<scenes::node>;

public:

  using dependencies = core::dependency_list<scenes::scenes_module>;

  physics_module();

  ~physics_module();

  auto fixed_update() -> void;

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

private:

  auto _sync_broadphase(scenes::scene& scene) -> void;

  auto _generate_candidate_pairs() -> void;

  auto _narrowphase() -> void;

  // Drops every broadphase leaf/pair/manifold. Play mode's "stop" reloads the scene in place from
  // a snapshot (scene_serializer::load over the same registry), which destroys and recreates every
  // entity -- any scenes::node this module is still holding onto from before that becomes a stale
  // handle. Called once on the false -> true edge of is_simulating() so a fresh play session always
  // starts from an empty broadphase instead of dereferencing those stale nodes.
  auto _reset() -> void;

  bool _was_simulating{false};

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

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
