// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PARTICLES_SPAWN_HPP_
#define LIBSBX_PARTICLES_SPAWN_HPP_

#include <cstdint>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/particles/particle.hpp>

namespace sbx::particles {

/**
 * @brief Rolls one new particle from an emitter's authored ranges.
 *
 * Position is sampled from `config`'s shape in local space and transformed by `world`; velocity is sampled independently and rotated (not translated) by `world` -- shape decides where a particle starts, never which way it moves.
 *
 * @param config Emitter configuration to sample from.
 *
 * @param world World transform of the emitter.
 *
 * @param id Particle id to assign.
 *
 * @param inherited_velocity Added on top unconditionally; zero unless the owning effect is a sub-emitter with @ref scenes::particle_effect::inherited_velocity set.
 *
 * @return Newly rolled particle.
 */
[[nodiscard]] auto roll_particle(const assets::particle_emitter& config, const math::matrix4x4& world, std::uint32_t id, const math::vector3& inherited_velocity = math::vector3{0.0f, 0.0f, 0.0f}) -> particle;

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_SPAWN_HPP_
