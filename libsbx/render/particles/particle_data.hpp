// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PARTICLE_DATA_HPP_
#define LIBSBX_RENDER_PARTICLE_DATA_HPP_

#include <array>
#include <cstdint>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/particle_effect.hpp>

namespace sbx::render {

inline static constexpr auto particle_pool_additive = std::uint32_t{0u};
inline static constexpr auto particle_pool_alpha_blend = std::uint32_t{1u};

/**
 * @brief One GPU-resident particle. Byte-mirrored in shaders/particles/particle_data.slang —
 * keep both in sync by hand, field for field, whenever either changes.
 *
 * Color is deliberately not stored here: it's derived every frame in the vertex shader from
 * `age / lifetime` and the owning emitter_instance's start_color/end_color, so every particle
 * spawned by the same emitter reads the same two colors instead of duplicating them per particle.
 * `reserved` pads the struct to 64 bytes and is free for a future per-particle payload (e.g. a
 * texture-atlas UV rect) without changing the stride.
 */
struct particle {
  math::vector3 position{math::vector3::zero};
  std::float_t age{0.0f};
  math::vector3 velocity{math::vector3::zero};
  std::float_t lifetime{1.0f};
  std::float_t size{1.0f};
  std::float_t rotation{0.0f};
  std::uint32_t emitter_slot{0u};
  std::uint32_t seed{0u};
  math::vector4 reserved{math::vector4::zero};
}; // struct particle

static_assert(sizeof(particle) == 64u, "particle must stay byte-mirrored with shaders/particles/particle_data.slang's particle struct");

inline static constexpr auto particle_texture_index_none = std::uint32_t{0xFFFFFFFFu};

/**
 * @brief Per-emitter-instance data, rewritten wholesale from the CPU every frame (same pattern as
 * render_module's _transform_buffer/_light_buffer). One pool-local array of these per particle_pool
 * — an instance's blend mode is fixed by which pool's array it lives in, so no blend-mode field is
 * needed here. Byte-mirrored in shaders/particles/particle_data.slang.
 */
struct emitter_instance {
  math::vector3 position{math::vector3::zero};
  std::float_t emission_rate{0.0f};
  math::vector3 velocity_min{math::vector3::zero};
  std::float_t lifetime_min{1.0f};
  math::vector3 velocity_max{math::vector3::zero};
  std::float_t lifetime_max{1.0f};
  math::vector4 start_color{math::vector4::one};
  math::vector4 end_color{math::vector4::one};
  std::float_t size_min{1.0f};
  std::float_t size_max{1.0f};
  std::float_t gravity{0.0f};
  std::float_t drag{0.0f};
  std::uint32_t active{0u};
  std::uint32_t particles_to_emit{0u};
  std::uint32_t seed{0u};
  std::uint32_t shape{static_cast<std::uint32_t>(assets::emitter_shape::point)};
  math::vector3 shape_extents{math::vector3::zero};
  std::uint32_t texture_index{particle_texture_index_none};
}; // struct emitter_instance

static_assert(sizeof(emitter_instance) == 128u, "emitter_instance must stay byte-mirrored with shaders/particles/particle_data.slang's emitter_instance struct");

/**
 * @brief The pool's persistent free-stack/alive-list bookkeeping. Entirely GPU read-modify-write
 * except for the one-time init upload in particle_pool's constructor (dead_count = max_particles).
 * `alive_count[2]` is indexed by "read"/"write" parity that flips every frame — see particle_pool.
 */
struct particle_counters {
  std::uint32_t dead_count{0u};
  std::array<std::uint32_t, 2u> alive_count{0u, 0u};
  std::uint32_t pad0{0u};
}; // struct particle_counters

static_assert(sizeof(particle_counters) == 16u, "particle_counters must stay byte-mirrored with shaders/particles/particle_data.slang's particle_counters struct");

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_DATA_HPP_
