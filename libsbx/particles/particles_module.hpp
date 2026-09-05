// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PARTICLES_PARTICLES_MODULE_HPP_
#define LIBSBX_PARTICLES_PARTICLES_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/physics_module.hpp>

#include <libsbx/particles/particle.hpp>

namespace sbx::particles {

/**
 * @brief Owns the fixed-step spawn/integrate/collide/recycle pipeline for every @ref scenes::particle_effect in the active scene.
 *
 * Gated on @ref scenes::scenes_module::is_simulating() like @ref physics::physics_module, so particles freeze outside Play, matching physics' broadphase (also populated only while simulating).
 */
class particles_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<scenes::scenes_module, assets::assets_module, physics::physics_module>;

  auto fixed_update() -> void;

private:

  auto _simulate_effect(scenes::scene& scene, scenes::particle_effect& effect, const math::matrix4x4& world, std::float_t dt) -> void;

  auto _spawn(scenes::scene& scene, const assets::particle_emitter& config, scenes::particle_emitter& runtime, const math::matrix4x4& world, const math::vector3& inherited_velocity, bool emitting, std::float_t dt) -> void;

  auto _integrate(scenes::scene& scene, const assets::particle_emitter& config, scenes::particle_emitter& runtime, std::float_t dt) -> void;

  auto _resolve_collisions(scenes::scene& scene, const assets::particle_emitter& config, scenes::particle_emitter& runtime) -> void;

  auto _record_trails(const assets::particle_emitter& config, scenes::particle_emitter& runtime, std::float_t dt) -> void;

  /**
   * @brief Fires every config.sub_emitters binding matching @p event, spawning (or reusing a pooled, stopped) child @ref scenes::particle_effect node at @p p's world position.
   *
   * Pool entries are tracked by math::uuid, not ecs::entity, since @ref scenes::node doesn't expose its raw entity outside scene/scene_serializer; @ref scenes::scene::find resolves an id back to a node.
   */
  auto _trigger_sub_emitters(scenes::scene& scene, scenes::particle_emitter& runtime, const assets::particle_emitter& config, assets::sub_emitter_event event, const particles::particle& p) -> void;

}; // class particles_module

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLES_MODULE_HPP_
