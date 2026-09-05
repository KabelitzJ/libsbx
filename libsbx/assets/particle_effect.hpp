// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_
#define LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::assets {

enum class emitter_blend_mode : std::uint8_t {
  additive,
  alpha_blend
}; // enum class emitter_blend_mode

enum class emitter_shape : std::uint8_t {
  point,
  sphere,
  box,
  cone
}; // enum class emitter_shape

/**
 * @brief Params for emitter_shape::cone. Apex at the emitter's origin, axis along local -Z. `radius`
 * is the base circle's radius at the angle's implied height. `emit_from_volume` is the probability a
 * given particle samples from inside the cone's volume instead of from its base disc (0 = always the
 * base, matching Unity's "Emit from: Base"; 1 = always the volume).
 */
struct cone_shape_params {
  math::angle angle{math::degree{25.0f}};
  std::float_t radius{1.0f};
  std::float_t emit_from_volume{0.0f};
}; // struct cone_shape_params

enum class particle_collision_mode : std::uint8_t {
  none,
  planes,
  world
}; // enum class particle_collision_mode

/**
 * @brief One explicit collision plane, for particle_collision_mode::planes. `normal` points away
 * from the plane's surface (the side particles bounce off of); `distance` is the plane's offset
 * along `normal` from the world origin, i.e. a particle is behind the plane when
 * `dot(normal, position) - distance < 0`.
 */
struct collision_plane {
  math::vector3 normal{0.0f, 1.0f, 0.0f};
  std::float_t distance{0.0f};
}; // struct collision_plane

inline constexpr auto collision_max_planes = std::size_t{4};

/**
 * @brief Particle collision, matching Unity's Collision module's two modes: an explicit plane list
 * (cheap, no broadphase involved) or the real physics world (queries physics_module's broadphase +
 * GJK/EPA, treating each particle as a small sphere -- see physics_module::query_sphere_contacts).
 */
struct collision_config {
  particle_collision_mode mode{particle_collision_mode::none};
  containers::static_vector<collision_plane, collision_max_planes> planes{};
  std::float_t bounce{0.5f};
  std::float_t lifetime_loss{0.0f};
  std::float_t dampen{0.0f};
  std::float_t radius_scale{1.0f};
  std::uint32_t max_collisions_per_particle{0u};
}; // struct collision_config

inline constexpr auto curve_max_keys = std::size_t{8};

/** 
 * @brief One keyframe of a curve. `time` is normalized lifetime in [0, 1]. 
 */
struct curve_key {
  std::float_t time{0.0f};
  std::float_t value{0.0f};
}; // struct curve_key

/**
 * @brief A small fixed-capacity keyframed curve over normalized particle lifetime, linearly
 * interpolated between the two keys bracketing a given `t` (clamped at the ends). Keys don't need to
 * be authored in time order -- evaluate() finds the bracketing pair by scanning all of them, cheap
 * given the tiny capacity. An empty curve means "no curve authored"; every over-lifetime field this
 * type is used for defines its own fallback for that case (see particle_emitter's fields below).
 */
struct curve {
  containers::static_vector<curve_key, curve_max_keys> keys{};

  [[nodiscard]] auto evaluate(std::float_t t) const -> std::float_t;

  [[nodiscard]] auto has_keys() const noexcept -> bool {
    return !keys.is_empty();
  }
}; // struct curve

/** @brief Three independent curve channels, one per axis -- for velocity/force-over-lifetime. */
struct vector3_curve {
  curve x{};
  curve y{};
  curve z{};

  [[nodiscard]] auto evaluate(std::float_t t) const -> math::vector3;

  [[nodiscard]] auto has_keys() const noexcept -> bool {
    return x.has_keys() || y.has_keys() || z.has_keys();
  }
}; // struct vector3_curve

inline constexpr auto gradient_max_keys = std::size_t{8};

struct gradient_color_key {
  std::float_t time{0.0f};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
}; // struct gradient_color_key

struct gradient_alpha_key {
  std::float_t time{0.0f};
  std::float_t alpha{1.0f};
}; // struct gradient_alpha_key

/**
 * @brief A Unity-Gradient-style color ramp over normalized lifetime: color and alpha are keyed and
 * interpolated independently, then recombined by evaluate(). Empty (no color_keys) means "no gradient
 * authored" -- particle_emitter::color_over_lifetime falls back to the plain start_color/end_color
 * lerp in that case, so every existing .particle_effect file keeps its old look.
 */
struct gradient {
  containers::static_vector<gradient_color_key, gradient_max_keys> color_keys{};
  containers::static_vector<gradient_alpha_key, gradient_max_keys> alpha_keys{};

  [[nodiscard]] auto evaluate(std::float_t t) const -> math::color;

  [[nodiscard]] auto has_keys() const noexcept -> bool {
    return !color_keys.is_empty();
  }
}; // struct gradient

enum class particle_render_mode : std::uint8_t {
  billboard,
  mesh
}; // enum class particle_render_mode

enum class sub_emitter_event : std::uint8_t {
  birth,
  death,
  collision
}; // enum class sub_emitter_event

class particle_effect;

/**
 * @brief Spawns a child particle_effect instance (non-looping, one-shot) whenever a particle from
 * this emitter fires @p event. Child instances are pooled per emitter (see
 * scenes::particle_emitter::sub_emitter_pool) rather than spawning a fresh scene node every time, so
 * a high-frequency event (birth at a fast emission rate, or collision) doesn't churn nodes unbounded.
 */
struct sub_emitter_binding {
  sub_emitter_event event{sub_emitter_event::birth};
  asset_handle<particle_effect> effect{};
  std::float_t probability{1.0f};
  bool inherit_velocity{false};
}; // struct sub_emitter_binding

inline constexpr auto trail_max_points = std::size_t{20};

/**
 * @brief A ribbon trail following each particle, camera-facing like the billboards (see
 * shaders/particles/trail.slang). `color_over_trail` is evaluated per point by its position along the
 * ribbon (0 = head/newest, 1 = tail/oldest) at render-extraction time, not baked in when the point was
 * recorded, so it always reflects the ribbon's current length as points age out.
 */
struct trail_config {
  bool enabled{false};
  std::float_t min_vertex_distance{0.1f};
  std::float_t lifetime{0.5f};
  std::float_t width{0.1f};
  gradient color_over_trail{};
  bool die_with_particle{false};
}; // struct trail_config

enum class particle_simulation_mode : std::uint8_t {
  cpu,
  gpu
}; // enum class particle_simulation_mode

struct particle_emitter {
  std::string name{"emitter"};
  particle_simulation_mode simulation_mode{particle_simulation_mode::cpu};
  emitter_blend_mode blend_mode{emitter_blend_mode::additive};
  std::float_t emission_rate{10.0f};
  std::uint32_t burst_count{0u};
  emitter_shape shape{emitter_shape::point};
  math::vector3 shape_extents{0.0f, 0.0f, 0.0f};
  cone_shape_params cone{};
  math::vector3 velocity_min{-1.0f, 1.0f, -1.0f};
  math::vector3 velocity_max{1.0f, 2.0f, 1.0f};
  std::float_t lifetime_min{1.0f};
  std::float_t lifetime_max{2.0f};
  math::color start_color{1.0f, 1.0f, 1.0f, 1.0f};
  math::color end_color{1.0f, 1.0f, 1.0f, 0.0f};
  gradient color_over_lifetime{};
  std::float_t size_min{0.1f};
  std::float_t size_max{0.2f};
  curve size_over_lifetime{};
  std::float_t rotation_min{0.0f};
  std::float_t rotation_max{0.0f};
  curve rotation_over_lifetime{};
  vector3_curve velocity_over_lifetime{};
  math::vector3 force_over_lifetime_min{0.0f, 0.0f, 0.0f};
  math::vector3 force_over_lifetime_max{0.0f, 0.0f, 0.0f};
  std::float_t gravity{0.0f};
  std::float_t drag{0.0f};
  texture_handle texture{};
  particle_render_mode render_mode{particle_render_mode::billboard};
  mesh_handle render_mesh{};
  material_handle render_material{};
  collision_config collision{};
  std::vector<sub_emitter_binding> sub_emitters{};
  trail_config trail{};

  /**
   * @brief Whether this emitter's current config can run on the GPU path (simulation_mode
   * == gpu). That path is billboard-only, has no collision, no sub-emitters/trails, and no
   * cone shape support -- see libsbx/render/particles/particle_data.hpp's emission_shape enum.
   */
  [[nodiscard]] auto supports_gpu_simulation() const -> bool {
    return shape != emitter_shape::cone
        && collision.mode == particle_collision_mode::none
        && sub_emitters.empty()
        && !trail.enabled
        && render_mode == particle_render_mode::billboard;
  }
}; // struct particle_emitter

class particle_effect final {

  friend class asset_residency;

public:

  struct create_info {
    std::string name{"particle_effect"};
    std::vector<particle_emitter> emitters{};
  }; // struct create_info

  particle_effect() = default;

  explicit particle_effect(const create_info& create_info)
  : _emitters{create_info.emitters},
    _name{create_info.name} { }

  [[nodiscard]] auto emitters() const noexcept -> const std::vector<particle_emitter>& {
    return _emitters;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

private:

  std::vector<particle_emitter> _emitters{};
  math::uuid _id{math::uuid::nil()};
  std::string _name{"particle_effect"};

}; // class particle_effect

using particle_effect_handle = asset_handle<particle_effect>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_
