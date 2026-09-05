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

namespace sbx::particles {

/**
 * @brief Owns the fixed-step spawn/integrate/recycle pipeline for every scenes::particle_effect in
 * the active scene. Mirrors physics::physics_module's shape: a fixed_update() method, picked up
 * automatically as the core::stage::fixed_update hook, gated on scenes_module::is_simulating() the
 * same way physics_module is, so particles freeze in edit mode and only run during Play.
 */
class particles_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<scenes::scenes_module, assets::assets_module>;

  auto fixed_update() -> void;

private:

  auto _simulate_effect(scenes::particle_effect& effect, const math::matrix4x4& world, std::float_t dt) -> void;

  auto _spawn(const assets::particle_emitter& config, scenes::particle_emitter& runtime, const math::matrix4x4& world, bool emitting, std::float_t dt) -> void;

  auto _integrate(const assets::particle_emitter& config, scenes::particle_emitter& runtime, std::float_t dt) -> void;

}; // class particles_module

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLES_MODULE_HPP_
