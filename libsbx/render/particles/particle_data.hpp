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

/** @brief One GPU particle, mirrored with particle_data.slang. Color is derived from age; `reserved` pads to 64 bytes. */
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

/** @brief Per-emitter-instance data, rewritten from the CPU every frame and mirrored with particle_data.slang. The owning pool decides the blend mode. */
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

/** @brief GPU-maintained free stack and alive list counters; `alive_count[2]` is indexed by a parity that flips every frame. */
struct particle_counters {
  std::uint32_t dead_count{0u};
  std::array<std::uint32_t, 2u> alive_count{0u, 0u};
  std::uint32_t pad0{0u};
}; // struct particle_counters

static_assert(sizeof(particle_counters) == 16u, "particle_counters must stay byte-mirrored with shaders/particles/particle_data.slang's particle_counters struct");

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_DATA_HPP_
