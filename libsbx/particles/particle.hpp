// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PARTICLES_PARTICLE_HPP_
#define LIBSBX_PARTICLES_PARTICLE_HPP_

#include <cstdint>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::particles {

/**
 * @brief One live particle, simulated on the CPU each fixed_update() by particles_module. `id` is a
 * per-emitter monotonic spawn counter, stable across the array's swap-and-pop recycling — later
 * features (collision, trails, sub-emitters) key off it rather than an array index.
 */
struct particle {
  std::uint32_t id{0u};
  math::vector3 position{};
  // Last step's position. Not yet consumed by anything -- collision currently only tests the
  // post-integration position (a plain sphere query, not swept), a known v1 simplification that can
  // tunnel through thin colliders at high speed. Kept here so a swept/capsule query can use it later
  // without touching every particle-producing call site again.
  math::vector3 previous_position{};
  math::vector3 velocity{};
  std::float_t rotation{0.0f};
  std::float_t age{0.0f};
  std::float_t lifetime{1.0f};
  std::float_t base_size{1.0f};
  std::float_t size{1.0f}; // base_size scaled by whatever over-lifetime curve is active
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::uint32_t collision_count{0u};
}; // struct particle

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLE_HPP_
