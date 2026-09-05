// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PARTICLES_PARTICLE_HPP_
#define LIBSBX_PARTICLES_PARTICLE_HPP_

#include <cstdint>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/assets/particle_effect.hpp>

namespace sbx::particles {

/**
 * @brief One live particle, simulated on the CPU each fixed_update() by particles_module. `id` is a
 * per-emitter monotonic spawn counter, stable across the array's swap-and-pop recycling — later
 * features (collision, trails, sub-emitters) key off it rather than an array index.
 */
struct particle {
  std::uint32_t id{0u};
  math::vector3 position{};
  math::vector3 previous_position{};
  math::vector3 velocity{};
  math::vector3 constant_force{}; // rolled once at spawn from force_over_lifetime_min/max
  std::float_t rotation{0.0f};
  std::float_t age{0.0f};
  std::float_t lifetime{1.0f};
  std::float_t base_size{1.0f};
  std::float_t size{1.0f};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::uint32_t collision_count{0u};
}; // struct particle

/** @brief One recorded point of a trail (see assets::trail_config). */
struct trail_point {
  math::vector3 position{};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f}; // the owning particle's color when this point was recorded
  std::float_t age{0.0f}; // this point's own age, not the particle's -- drives trail_config::lifetime fade-out
}; // struct trail_point

/**
 * @brief A ribbon of recorded points trailing one particle, keyed by its particle::id (survives the
 * particle array's swap-and-pop recycling). `points.front()` is the head (newest, closest to the
 * particle); `points.back()` is the tail (oldest). Not itself removed the instant its particle dies --
 * see assets::trail_config::die_with_particle.
 */
struct trail {
  std::uint32_t particle_id{0u};
  bool particle_alive{true};
  containers::static_vector<trail_point, assets::trail_max_points> points{};
}; // struct trail

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLE_HPP_
