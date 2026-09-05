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
 * @brief Rolls one new particle from an emitter's authored ranges: a position sampled from `config`'s
 * shape (point/sphere/box/cone, in the emitter's local space) transformed into world space by
 * `world`, and a velocity sampled independently from `velocity_min`/`velocity_max` and rotated (not
 * translated) by the same matrix — shape only ever decides where a particle starts, never which way
 * it moves, matching the point/sphere/box behavior this extends. `inherited_velocity` is added on top
 * unconditionally (zero when the owning effect isn't a sub-emitter with inherit_velocity set — see
 * scenes::particle_effect::inherited_velocity).
 */
[[nodiscard]] auto roll_particle(const assets::particle_emitter& config, const math::matrix4x4& world, std::uint32_t id, const math::vector3& inherited_velocity = math::vector3{0.0f, 0.0f, 0.0f}) -> particle;

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_SPAWN_HPP_
