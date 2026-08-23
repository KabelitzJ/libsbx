// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_
#define LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::assets {

/**
 * @brief Which particle_pool an emitter's particles live in — fixed per emitter definition, not
 * per instance, since Vulkan blend state is fixed per draw call (see libsbx/render/particle_pool.hpp).
 */
enum class particle_blend_mode : std::uint8_t {
  additive,
  alpha_blend
}; // enum class particle_blend_mode

/**
 * @brief Where within the emitter's volume a spawned particle's position is randomized. Ordinal
 * values are byte-mirrored onto render::emitter_instance_gpu::shape / shaders/particles' own
 * particle_emission_shape_* constants — keep all three in sync by hand if this changes.
 */
enum class particle_emission_shape : std::uint8_t {
  /** @brief Every particle spawns exactly at the emitter's world position. shape_extents unused. */
  point,
  /** @brief Uniformly distributed inside a sphere of radius shape_extents.x, centered on the emitter. */
  sphere,
  /** @brief Uniformly distributed inside a box of half-extents shape_extents, centered on the emitter. */
  box
}; // enum class particle_emission_shape

/**
 * @brief One emitter's authored parameters. This is the CPU-authored counterpart of
 * render::emitter_instance_gpu — render_module::_build_packet() turns one of these (plus a world
 * position and a host-side emission accumulator) into that GPU struct every frame. burst_count
 * doesn't appear there directly: it's folded into the GPU struct's particles_to_emit for exactly
 * one frame (the emitter's first frame of a given activation — see
 * scenes::particle_emitter_runtime::burst_fired) rather than needing its own GPU-side field.
 */
struct particle_emitter_definition {
  std::string name{"emitter"};
  particle_blend_mode blend_mode{particle_blend_mode::additive};
  std::float_t emission_rate{10.0f}; // particles per second, continuous
  std::uint32_t burst_count{0u};     // spawned once, on top of emission_rate, the first frame this emitter becomes active
  particle_emission_shape shape{particle_emission_shape::point};
  math::vector3 shape_extents{0.0f, 0.0f, 0.0f}; // meaning depends on shape — see particle_emission_shape
  math::vector3 velocity_min{-1.0f, 1.0f, -1.0f};
  math::vector3 velocity_max{1.0f, 2.0f, 1.0f};
  std::float_t lifetime_min{1.0f};
  std::float_t lifetime_max{2.0f};
  math::color start_color{1.0f, 1.0f, 1.0f, 1.0f};
  math::color end_color{1.0f, 1.0f, 1.0f, 0.0f};
  std::float_t size_min{0.1f};
  std::float_t size_max{0.2f};
  std::float_t gravity{0.0f};
  std::float_t drag{0.0f};
  // Sampled in draw.slang's fragment shader in place of the procedural circular falloff when
  // valid (invalid/default — the common case for additive fire/spark-style emitters — keeps the
  // procedural look).
  texture_handle texture{};
}; // struct particle_emitter_definition

/**
 * @brief An authorable particle effect: a named group of emitters spawned together (e.g. an
 * "explosion" effect made of a fire, debris and smoke emitter). Mirrors libsbx/assets/material.hpp's
 * shape — plain data, loaded/created/updated/saved through assets_module, referenced by
 * scenes::particle_effect_instance.
 */
class particle_effect final {

  friend class asset_residency;

public:

  struct create_info {
    std::string name{"particle_effect"};
    std::vector<particle_emitter_definition> emitters{};
  }; // struct create_info

  particle_effect() = default;

  explicit particle_effect(const create_info& create_info)
  : _emitters{create_info.emitters},
    _name{create_info.name} { }

  [[nodiscard]] auto emitters() const noexcept -> const std::vector<particle_emitter_definition>& {
    return _emitters;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

private:

  std::vector<particle_emitter_definition> _emitters{};
  math::uuid _id{math::uuid::nil()};
  std::string _name{"particle_effect"};

}; // class particle_effect

using particle_effect_handle = asset_handle<particle_effect>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_
