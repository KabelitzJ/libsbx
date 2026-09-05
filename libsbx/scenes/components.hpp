// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_COMPONENTS_HPP_
#define LIBSBX_SCENES_COMPONENTS_HPP_

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/ecs/entity.hpp>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/environment_map.hpp>
#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/particles/particle.hpp>

namespace sbx::scenes {

/**
 * @brief The authored transform of a node, relative to its parent.
 *
 * Derived into world_transform each frame by @ref scene::update.
 */
struct local_transform {

  math::vector3 position{0.0f, 0.0f, 0.0f};
  math::quaternion rotation{math::quaternion::identity};
  math::vector3 scale{1.0f, 1.0f, 1.0f};

  [[nodiscard]] auto matrix() const -> math::matrix4x4 {
    const auto translation_matrix = math::matrix4x4::translated(math::matrix4x4::identity, position);
    const auto scale_matrix = math::matrix4x4::scaled(math::matrix4x4::identity, scale);

    return translation_matrix * math::matrix_cast<math::matrix4x4>(rotation) * scale_matrix;
  }

  [[nodiscard]] auto right() const -> math::vector3 {
    return rotation * math::vector3{1.0f, 0.0f, 0.0f};
  }

  [[nodiscard]] auto up() const -> math::vector3 {
    return rotation * math::vector3{0.0f, 1.0f, 0.0f};
  }

  [[nodiscard]] auto forward() const -> math::vector3 {
    return rotation * math::vector3{0.0f, 0.0f, -1.0f};
  }

}; // struct local_transform

/**
 * @brief Cached world matrix, written each frame by scene::update().
 */
struct world_transform {
  math::matrix4x4 matrix{math::matrix4x4::identity};
}; // struct world_transform

struct relationship {
  ecs::entity parent{ecs::null_entity};
  std::vector<ecs::entity> children{};
}; // struct relationship

struct id final : math::uuid {

  using base_type = math::uuid;

  id()
  : base_type{} { }

  id(const base_type& base)
  : base_type{base} { }

}; // class id

template<typename Char>
struct basic_tag final : utility::basic_hashed_string<Char> {

  using base_type = utility::basic_hashed_string<Char>;

  template<typename... Args>
  basic_tag(Args&&... args)
  : base_type{std::forward<Args>(args)...} { }

}; // class tag

using tag = basic_tag<char>;

struct camera {
  std::float_t fov_degrees{60.0f};
  std::float_t near_plane{0.1f};
  std::float_t far_plane{1000.0f};
  std::float_t exposure{0.0f}; // EV stops applied as exp2(exposure) before tonemapping; 0 = unchanged.
}; // struct camera

/**
 * @brief What a node draws.
 *
 * materials is the sole source of truth for what each submesh renders with — there is no
 * render-time fallback to the mesh's own per-submesh material. See @ref sync_materials_with_mesh.
 */
struct mesh_renderer {
  assets::mesh_handle mesh{};
  std::vector<assets::material_handle> materials{};
}; // struct mesh_renderer

/**
 * @brief Fills unset material slots from the mesh's per-submesh materials.
 *
 * Resizes materials to the mesh's submesh count. Never overwrites an already-set slot.
 * No-op if no mesh is assigned.
 */
inline auto sync_materials_with_mesh(mesh_renderer& renderer) -> void {
  if (!renderer.mesh.is_valid()) {
    return;
  }

  const auto& submeshes = renderer.mesh->submeshes();

  if (renderer.materials.size() < submeshes.size()) {
    renderer.materials.resize(submeshes.size());
  }

  for (auto index = std::size_t{0u}; index < submeshes.size(); ++index) {
    if (!renderer.materials[index].is_valid()) {
      renderer.materials[index] = submeshes[index].material;
    }
  }
}

struct directional_light {
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t intensity{1.0f};
  bool casts_shadows{true};
  std::float_t shadow_distance{75.0f}; // How far from the camera cascaded shadow maps extend.
}; // struct directional_light

struct point_light {
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t intensity{25.0f}; // Higher than directional's 1.0: shader applies inverse-square falloff to this, so it needs headroom to still read at a few units' distance.
  std::float_t range{10.0f};
}; // struct point_light

struct spot_light {
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t intensity{25.0f}; // See point_light::intensity.
  std::float_t range{10.0f};
  std::float_t inner_angle{0.4f}; // radians
  std::float_t outer_angle{0.6f}; // radians
}; // struct spot_light

struct skybox {
  assets::environment_map_handle environment{};
  std::float_t intensity{1.0f}; // Visible sky background brightness only.
  std::float_t ambient_intensity{1.0f}; // Scales the IBL diffuse+specular ambient term added to every surface, independent of the background's own brightness.
}; // struct skybox

/**
 * @brief Per-instance runtime state for one emitter, index-paired with assets::particle_effect::emitters().
 *
 * Never serialized: a loaded scene starts with an empty particles array and re-simulates from
 * the saved particle_effect::elapsed.
 */
struct particle_emitter {
  // Two clocks by assets::particle_emitter::simulation_mode: cpu advances at particles_module's
  // fixed timestep; gpu advances via scene_renderer_module's extraction loop at render rate to
  // match particle_simulate_pass's dispatch cadence. Don't unify — see particle-system plan doc.
  std::float_t emission_accumulator{0.0f};
  bool burst_fired{false};

  // GPU-path only: claimed slot in render::particle_pool for this emitter's blend_mode, or
  // invalid_slot if unclaimed. Owned by scene_renderer_module's extraction loop; reset to
  // invalid_slot by particles_module on stop.
  inline static constexpr auto invalid_slot = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t slot{invalid_slot};

  std::uint32_t next_particle_id{0u};
  std::vector<particles::particle> particles{};
  std::vector<particles::trail> trails{}; // only populated when the matching assets::particle_emitter::trail.enabled
  // Pooled child nodes for sub_emitters, identified by scenes::id (not ecs::entity) so a pool entry
  // can be resolved back to a node via scene::find() without scenes::node exposing its raw entity.
  std::vector<math::uuid> sub_emitter_pool{};
}; // struct particle_emitter

enum class particle_playback_state : std::uint8_t {
  playing,
  paused,
  stopped
}; // enum class particle_playback_state

struct particle_effect {
  assets::particle_effect_handle effect{};
  particle_playback_state playback{particle_playback_state::playing};
  bool loop{true};
  std::float_t elapsed{0.0f};
  std::float_t duration{5.0f}; // Ignored while looping; otherwise emission stops once elapsed reaches this.
  std::vector<particle_emitter> emitters{};
  // Runtime-only, not persisted: set once by a sub_emitter_binding with inherit_velocity, added to
  // every particle this effect's emitters roll for as long as it exists.
  math::vector3 inherited_velocity{0.0f, 0.0f, 0.0f};
}; // struct particle_effect

enum class script_field_type : std::uint8_t {
  float32,
  int32,
  boolean,
  string
}; // enum class script_field_type

struct script_field_override {
  std::string name;
  script_field_type type{script_field_type::float32};
  std::float_t float_value{0.0f};
  std::int32_t int_value{0};
  bool bool_value{false};
  std::string string_value{};
}; // struct script_field_override

/**
 * @brief One C# Behavior-derived script attached to a node, by class name and per-field overrides.
 *
 * This is the persisted authoring record, separate from the runtime-only scripting::scripts
 * component (populated from this list by scripting_module::instantiate() when simulation starts).
 * Kept separate because a managed object handle can't be serialized.
 */
struct script_entry {
  std::string class_name;
  std::vector<script_field_override> field_overrides{};
}; // struct script_entry

struct script_component {
  std::vector<script_entry> scripts{};
}; // struct script_component

} // namespace sbx::scenes

template<typename Char>
struct fmt::formatter<sbx::scenes::basic_tag<Char>> : fmt::formatter<sbx::utility::basic_hashed_string<Char>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::scenes::basic_tag<Char>& tag, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", tag.c_str());
  }

}; // struct fmt::formatter

#endif // LIBSBX_SCENES_COMPONENTS_HPP_
