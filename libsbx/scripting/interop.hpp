// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCRIPTING_INTEROP_HPP_
#define LIBSBX_SCRIPTING_INTEROP_HPP_

#include <functional>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/utility/type_name.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/platform/input.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/instance_buffer.hpp>

#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/type.hpp>
#include <libsbx/scripting/managed/assembly.hpp>

namespace sbx::scripting {

/** @brief Mirrors Sbx.Core.FontGlyph: same fields and units as assets::font::glyph, per unit of font size, y down. */
struct font_glyph_data {
  std::float_t uv_x0;
  std::float_t uv_y0;
  std::float_t uv_x1;
  std::float_t uv_y1;
  std::float_t width;
  std::float_t height;
  std::float_t bearing_x;
  std::float_t bearing_y;
  std::float_t advance;
}; // struct font_glyph_data

/** @brief Mirrors Sbx.Core.PostProcessSettings: scenes::post_process_settings with bools and enums as uint32 and the lookup table as a texture uuid (0 = none). */
struct post_process_data {
  std::float_t exposure;

  std::uint32_t bloom_enabled;
  std::float_t bloom_intensity;
  std::float_t bloom_threshold;
  std::float_t bloom_knee;

  std::uint32_t dof_enabled;
  std::uint32_t dof_mode;  // 0 distance, 1 screen band
  std::float_t dof_focus_distance;
  std::float_t dof_focus_range;
  std::float_t dof_band_center;
  std::float_t dof_band_height;
  std::float_t dof_max_blur;
  std::uint32_t dof_samples;

  std::uint64_t lut;       // texture uuid, 0 = none
  std::float_t lut_contribution;
  std::float_t contrast;
  std::float_t saturation;

  std::uint32_t fog_enabled;
  math::color fog_color;
  std::float_t fog_density;
  std::float_t fog_start;
  std::float_t fog_height_falloff;
  std::float_t fog_base_height;
  std::float_t fog_max_opacity;
  std::uint32_t fog_affects_sky;

  std::uint32_t ao_enabled;
  std::float_t ao_radius;
  std::float_t ao_intensity;
  std::uint32_t ao_samples;
}; // struct post_process_data

// Whole-component mirrors for the layout calls: flags are uint32 (0/1) and enums their underlying value, so the managed structs lay out identically.

struct layout_group_data {
  std::float_t spacing;
  std::float_t padding_left;
  std::float_t padding_top;
  std::float_t padding_right;
  std::float_t padding_bottom;
  std::uint32_t child_alignment;
  std::uint32_t control_child_width;
  std::uint32_t control_child_height;
  std::uint32_t child_force_expand_width;
  std::uint32_t child_force_expand_height;
}; // struct layout_group_data

struct layout_element_data {
  std::float_t min_width;
  std::float_t min_height;
  std::float_t preferred_width;
  std::float_t preferred_height;
  std::float_t flexible_width;
  std::float_t flexible_height;
  std::uint32_t ignore_layout;
}; // struct layout_element_data

struct interop {

  enum class log_level : std::int32_t {
    trace = utility::bit_v<0>,
    debug = utility::bit_v<1>,
    info = utility::bit_v<2>,
    warn = utility::bit_v<3>,
    error = utility::bit_v<4>,
    critical = utility::bit_v<5>
  }; // enum class log_level

  static auto log_log_message(log_level level, managed::string message) -> void;

  static auto scripting_attach_script(std::uint64_t uuid, managed::string class_name) -> void;
  static auto scripting_get_instance(std::uint64_t uuid, managed::string class_name) -> managed::object;

  static auto behavior_add_component(std::uint64_t uuid, managed::reflection_type component_type) -> void;
  static auto behavior_has_component(std::uint64_t uuid, managed::reflection_type component_type) -> managed::bool32;
  static auto behavior_remove_component(std::uint64_t uuid, managed::reflection_type component_type) -> managed::bool32;

  static auto tag_get_tag(std::uint64_t uuid) -> managed::string;

  static auto tag_set_tag(std::uint64_t uuid, managed::string tag) -> void;

  static auto transform_get_position(std::uint64_t uuid, math::vector3* position) -> void;

  static auto transform_set_position(std::uint64_t uuid, math::vector3* position) -> void;

  static auto transform_get_world_position(std::uint64_t uuid, math::vector3* position) -> void;

  static auto transform_get_rotation(std::uint64_t uuid, math::quaternion* rotation) -> void;

  static auto transform_set_rotation(std::uint64_t uuid, math::quaternion* rotation) -> void;

  static auto transform_get_right(std::uint64_t uuid, math::vector3* right) -> void;

  static auto transform_get_forward(std::uint64_t uuid, math::vector3* forward) -> void;

  static auto transform_get_up(std::uint64_t uuid, math::vector3* up) -> void;

  static auto transform_get_scale(std::uint64_t uuid, math::vector3* scale) -> void;

  static auto transform_set_scale(std::uint64_t uuid, math::vector3* scale) -> void;

  static auto transform_look_at(std::uint64_t uuid, math::vector3* target) -> void;

  static auto animator_get_playing(std::uint64_t uuid) -> managed::bool32;

  static auto animator_set_playing(std::uint64_t uuid, bool value) -> void;

  static auto animator_get_current_state_name(std::uint64_t uuid) -> managed::string;

  static auto animator_set_float(std::uint64_t uuid, managed::string name, std::float_t value) -> void;

  static auto animator_set_bool(std::uint64_t uuid, managed::string name, bool value) -> void;

  static auto animator_set_int(std::uint64_t uuid, managed::string name, std::int32_t value) -> void;

  static auto animator_set_trigger(std::uint64_t uuid, managed::string name) -> void;

  static auto animator_get_float(std::uint64_t uuid, managed::string name) -> std::float_t;

  static auto animator_get_bool(std::uint64_t uuid, managed::string name) -> managed::bool32;

  static auto animator_get_int(std::uint64_t uuid, managed::string name) -> std::int32_t;

  static auto rigidbody_get_linear_velocity(std::uint64_t uuid, math::vector3* velocity) -> void;

  static auto rigidbody_set_linear_velocity(std::uint64_t uuid, math::vector3* velocity) -> void;

  static auto rigidbody_get_angular_velocity(std::uint64_t uuid, math::vector3* velocity) -> void;

  static auto rigidbody_set_angular_velocity(std::uint64_t uuid, math::vector3* velocity) -> void;

  static auto rigidbody_get_mass(std::uint64_t uuid, std::float_t* mass) -> void;

  static auto rigidbody_set_mass(std::uint64_t uuid, std::float_t mass) -> void;

  static auto rigidbody_get_gravity_scale(std::uint64_t uuid, std::float_t* scale) -> void;

  static auto rigidbody_set_gravity_scale(std::uint64_t uuid, std::float_t scale) -> void;

  static auto rigidbody_add_force(std::uint64_t uuid, math::vector3* force) -> void;

  static auto rigidbody_add_torque(std::uint64_t uuid, math::vector3* torque) -> void;

  static auto node_find_by_name(managed::string name) -> std::uint64_t;

  static auto node_create(managed::string name) -> std::uint64_t;

  /**
   * @brief Instantiates the prefab at @p path (project-relative) as a new subtree under @p parent_uuid (0 = top level).
   *
   * Scripts in the prefab are instantiated immediately if the scene is simulating.
   * An invalid @p parent_uuid leaves the instance at the top level.
   *
   * @return The new root's uuid, or 0 if @p path isn't a valid prefab.
   */
  static auto node_instantiate_prefab(managed::string path, std::uint64_t parent_uuid) -> std::uint64_t;

  static auto node_destroy(std::uint64_t uuid) -> void;

  static auto node_set_parent(std::uint64_t uuid, std::uint64_t parent_uuid) -> void;

  static auto node_set_active(std::uint64_t uuid, bool active) -> void;

  static auto node_get_is_active(std::uint64_t uuid) -> managed::bool32;

  /** @brief Replaces the active scene with the scene at @p path (project-relative). No-op if @p path isn't a valid scene. */
  static auto scene_load(managed::string path) -> void;

  /** @brief Saves the active scene to @p path (project-relative) and registers it as a scene asset. */
  static auto scene_save(managed::string path) -> void;

  /** @brief Replaces the active scene with an empty one containing a default camera. Unsaved changes are discarded. */
  static auto scene_new() -> void;

  /** @brief Sets which .particle_effect asset (project-relative @p path) the node's ParticleEffect plays. */
  static auto particle_effect_load(std::uint64_t uuid, managed::string path) -> void;

  static auto particle_effect_play(std::uint64_t uuid) -> void;

  static auto particle_effect_pause(std::uint64_t uuid) -> void;

  static auto particle_effect_stop(std::uint64_t uuid) -> void;

  static auto particle_effect_get_loop(std::uint64_t uuid) -> managed::bool32;

  static auto particle_effect_set_loop(std::uint64_t uuid, bool value) -> void;

  static auto particle_effect_get_is_playing(std::uint64_t uuid) -> managed::bool32;

  static auto input_is_key_pressed(platform::key key) -> managed::bool32;

  static auto input_is_key_down(platform::key key) -> managed::bool32;

  static auto input_is_key_released(platform::key key) -> managed::bool32;

  static auto input_is_mouse_button_pressed(platform::mouse_button mouse_button) -> managed::bool32;

  static auto input_is_mouse_button_down(platform::mouse_button mouse_button) -> managed::bool32;

  static auto input_is_mouse_button_released(platform::mouse_button mouse_button) -> managed::bool32;

  static auto input_mouse_position(math::vector2* position) -> void;

  static auto input_scroll_delta(math::vector2* scroll_delta) -> void;

  // Main*: always resolved against the scene's active camera, not the calling script's node.
  static auto camera_screen_point_to_ray(math::ray* ray, math::vector2* position) -> void;

  /** @brief Inverse of camera_screen_point_to_ray. False, leaving out_position untouched, if world_position is behind the camera. */
  static auto camera_world_to_screen_point(math::vector3* world_position, math::vector2* out_position) -> managed::bool32;

  static auto camera_main_get_position(math::vector3* position) -> void;

  static auto camera_main_set_position(math::vector3* position) -> void;

  static auto camera_main_get_rotation(math::quaternion* rotation) -> void;

  static auto camera_main_set_rotation(math::quaternion* rotation) -> void;

  static auto camera_main_get_forward(math::vector3* forward) -> void;

  static auto camera_main_get_right(math::vector3* right) -> void;

  static auto camera_main_get_up(math::vector3* up) -> void;

  static auto camera_get_viewport(math::vector2* viewport) -> void;

  static auto camera_get_viewport_offset(math::vector2* offset) -> void;

  static auto render_settings_set_wireframe_enabled(bool enabled) -> void;

  static auto render_settings_get_wireframe_enabled() -> managed::bool32;

  // Per-node scenes::camera access (GetComponent<CameraSettings>()), unlike the Main* functions above.
  static auto camera_get_fov_degrees(std::uint64_t uuid, std::float_t* fov_degrees) -> void;

  static auto camera_set_fov_degrees(std::uint64_t uuid, std::float_t fov_degrees) -> void;

  static auto camera_get_near_plane(std::uint64_t uuid, std::float_t* near_plane) -> void;

  static auto camera_set_near_plane(std::uint64_t uuid, std::float_t near_plane) -> void;

  static auto camera_get_far_plane(std::uint64_t uuid, std::float_t* far_plane) -> void;

  static auto camera_set_far_plane(std::uint64_t uuid, std::float_t far_plane) -> void;

  static auto camera_get_exposure(std::uint64_t uuid, std::float_t* exposure) -> void;

  static auto camera_set_exposure(std::uint64_t uuid, std::float_t exposure) -> void;

  /** @brief The camera's whole post processing; the managed side edits a copy and writes it back. */
  static auto camera_get_post_process(std::uint64_t uuid, post_process_data* out_value) -> void;

  static auto camera_set_post_process(std::uint64_t uuid, const post_process_data* value) -> void;

  static auto time_delta_time(std::float_t* delta_time) -> void;

  /**
   * @brief Raycasts against the active scene's colliders and terrain.
   *
   * @p layer_mask is the raw bits of a Sbx.Core.Physics.LayerMask (0xFFFFFFFF = every layer).
   *
   * @return False, leaving every out parameter untouched, when nothing was hit within max_distance.
   */
  static auto physics_raycast(math::ray* ray, std::float_t max_distance, std::uint32_t layer_mask, std::uint64_t* out_node_uuid, math::vector3* out_point, math::vector3* out_normal, std::float_t* out_distance) -> managed::bool32;

  /** @brief Bakes the navmesh from the active scene's static geometry. Returns whether any usable polygons were produced. */
  static auto nav_bake(std::float_t agent_radius, std::float_t agent_height, std::float_t agent_max_slope, std::float_t agent_max_climb, std::float_t cell_size, std::float_t cell_height, std::float_t region_min_size, std::float_t region_merge_size, std::float_t edge_max_length, std::float_t edge_max_error, std::int32_t verts_per_poly) -> managed::bool32;

  static auto nav_has_navmesh() -> managed::bool32;

  /** @brief The closest point on the navmesh to @p point. False, leaving out_result untouched, if there's no navmesh. */
  static auto nav_sample_position(math::vector3* point, math::vector3* out_result) -> managed::bool32;

  /** @brief Requests a path to @p target. False if there's no navmesh, the node has no nav_agent, or no path was found. */
  static auto nav_agent_set_destination(std::uint64_t uuid, math::vector3* target) -> managed::bool32;

  /** @brief nav_agent_state as a byte: 0 = Idle, 1 = Moving, 2 = TargetUnreachable. */
  static auto nav_agent_get_state(std::uint64_t uuid) -> std::uint8_t;

  static auto nav_agent_get_velocity(std::uint64_t uuid, math::vector3* out_velocity) -> void;

  static auto nav_agent_get_remaining_distance(std::uint64_t uuid, std::float_t* out_distance) -> void;

  static auto nav_agent_get_radius(std::uint64_t uuid, std::float_t* out_radius) -> void;
  static auto nav_agent_set_radius(std::uint64_t uuid, std::float_t radius) -> void;

  /** @brief Agents whose height gap exceeds half the sum of their heights never collide, even when close in X/Z. */
  static auto nav_agent_get_height(std::uint64_t uuid, std::float_t* out_height) -> void;
  static auto nav_agent_set_height(std::uint64_t uuid, std::float_t height) -> void;

  /** @brief Height of the node's pivot above the navmesh surface (half the height for a centered capsule, 0 for a foot pivot). */
  static auto nav_agent_get_base_offset(std::uint64_t uuid, std::float_t* out_base_offset) -> void;
  static auto nav_agent_set_base_offset(std::uint64_t uuid, std::float_t base_offset) -> void;

  static auto nav_agent_get_speed(std::uint64_t uuid, std::float_t* out_speed) -> void;
  static auto nav_agent_set_speed(std::uint64_t uuid, std::float_t speed) -> void;

  static auto nav_agent_get_acceleration(std::uint64_t uuid, std::float_t* out_acceleration) -> void;
  static auto nav_agent_set_acceleration(std::uint64_t uuid, std::float_t acceleration) -> void;

  /** @brief Regenerates the active scene's terrain, replacing any previous one. */
  static auto terrain_generate(std::uint32_t width, std::uint32_t depth, std::float_t cell_size, std::float_t frequency, std::float_t amplitude, std::uint32_t octaves) -> void;

  /** @brief Terrain height at @p world_xz, 0 if no terrain exists. */
  static auto terrain_sample_height(math::vector2* world_xz, std::float_t* out_height) -> void;

  /** @brief Terrain normal at @p world_xz, +Y if no terrain exists. */
  static auto terrain_sample_normal(math::vector2* world_xz, math::vector3* out_normal) -> void;

  /** @brief math::noise::simplex, roughly in [-1, 1]. */
  static auto math_noise_simplex(std::float_t x, std::float_t y, std::float_t z) -> std::float_t;

  /** @brief math::noise::fractal (multi-octave simplex), roughly in [-1, 1]. */
  static auto math_noise_fractal(std::float_t x, std::float_t y, std::float_t z, std::uint32_t octaves, std::float_t lacunarity, std::float_t gain) -> std::float_t;

  /**
   * @brief Builds a mesh from raw vertex/index data for the node's mesh_renderer, creating the component and a material on first use.
   *
   * The node's material is reused across calls: create_material has a fixed capacity that per-frame calls would exhaust.
   * @p colors is optional (nullptr = opaque white) and multiplies the material's base color; @p tint is a single whole-mesh color.
   * The mesh is swapped in by apply_pending_geometry.
   */
  static auto mesh_renderer_set_geometry(std::uint64_t uuid, math::vector3* positions, math::vector3* normals, math::vector2* uvs, math::color* colors, std::uint32_t vertex_count, std::uint32_t* indices, std::uint32_t index_count, math::color* tint) -> void;

  /** @brief Assigns a material to one submesh slot, creating the component and growing the slots as needed. material_uuid 0 clears the slot. */
  static auto mesh_renderer_set_material(std::uint64_t uuid, std::uint32_t submesh_index, std::uint64_t material_uuid) -> void;

  /** @brief The mesh every instance draws, creating the instanced_mesh_renderer and a default material as needed. Swapped in by apply_pending_geometry. */
  static auto instanced_mesh_renderer_set_geometry(std::uint64_t uuid, math::vector3* positions, math::vector3* normals, math::vector2* uvs, math::color* colors, std::uint32_t vertex_count, std::uint32_t* indices, std::uint32_t index_count) -> void;

  static auto instanced_mesh_renderer_set_material(std::uint64_t uuid, std::uint64_t material_uuid) -> void;

  /** @brief Replaces every instance (count 0 = none). The GPU buffer is created by apply_pending_geometry. */
  static auto instanced_mesh_renderer_set_instances(std::uint64_t uuid, scenes::instance_data* instances, std::uint32_t count) -> void;

  /** @brief Loads a .material by project-relative path. Returns its uuid, or 0 if the path isn't a material. */
  static auto material_load(managed::string path) -> std::uint64_t;

  /** @brief Absolute path of the active project's assets directory, for scripts reading their own data files. */
  static auto project_get_assets_directory() -> managed::string;

  static auto texture_load(managed::string path, std::uint32_t format) -> std::uint64_t;

  /** @brief Allocates an empty compute-writable storage image. format: 0 = RGBA8, 1 = R32 float, 2 = R8 unorm (Sbx.Core.TextureFormat order). */
  static auto texture_create_storage_image(std::uint32_t width, std::uint32_t height, std::uint32_t format) -> std::uint64_t;

  /**
   * @brief Copies a storage image's pixels into caller-sized @p out_pixels with a blocking GPU readback.
   *
   * format must match the image (0 = RGBA8, 1 = R32 float, 2 = R8 unorm); single-channel formats land in .r only.
   * Logs and does nothing on an invalid texture, a non-storage image, or mismatched size/format.
   */
  static auto texture_read_pixels(std::uint64_t texture_uuid, std::uint32_t width, std::uint32_t height, std::uint32_t format, math::color* out_pixels) -> void;

  /** @brief Frees the texture's bindless indices and GPU image. No-op if texture_uuid doesn't resolve. */
  static auto texture_release(std::uint64_t texture_uuid) -> void;

  /** @brief A 2D texture array of the given layers, each exactly width x height. Returns 0 (and logs why) if any layer doesn't fit. */
  static auto texture2d_array_create(std::uint64_t* layer_uuids, std::uint32_t layer_count, std::uint32_t width, std::uint32_t height) -> std::uint64_t;

  static auto texture2d_array_is_resident(std::uint64_t array_uuid) -> managed::bool32;

  static auto texture2d_array_release(std::uint64_t array_uuid) -> void;

  /** @brief Puts a texture array into a material's generic slot (the shader reads texture_arrays[generic_textures[index]]). */
  static auto material_set_generic_texture_array(std::uint64_t material_uuid, std::uint32_t index, std::uint64_t array_uuid) -> void;

  /**
   * @brief Writes raw RGBA8 pixels (row-major, no padding) to a PNG on disk, for debugging compute output.
   *
   * @p path is a plain filesystem path, not an asset path; missing directories are created.
   *
   * @return False (and logs) on null pixels, zero size, or a failed write.
   */
  static auto debug_write_png(managed::string path, std::uint32_t width, std::uint32_t height, const std::uint8_t* rgba_pixels) -> managed::bool32;

  /**
   * @brief Whether a loaded texture's pixels have finished uploading. False for an unknown uuid, always true for storage images.
   *
   * Load() returns a valid handle immediately while the data streams in, so check this before sampling it in a compute shader.
   */
  static auto texture_is_resident(std::uint64_t texture_uuid) -> managed::bool32;

  /** @brief Loads a TTF as an SDF atlas font by project-relative path. Returns its uuid, or 0 on failure; glyphs arrive asynchronously (see font_is_resident). */
  static auto font_load(managed::string path) -> std::uint64_t;

  static auto font_is_resident(std::uint64_t font_uuid) -> managed::bool32;

  /** @brief False (out untouched) for a codepoint outside the font, or before it's resident. */
  static auto font_get_glyph(std::uint64_t font_uuid, std::uint32_t codepoint, font_glyph_data* out) -> managed::bool32;

  /** @brief x = line height, y = ascent, z = descent, per 1 unit of font size. */
  static auto font_get_metrics(std::uint64_t font_uuid, math::vector3* out) -> void;

  /** @brief Puts a font's SDF atlas (single channel, 0.5 = glyph edge) into a material's generic texture slot. */
  static auto material_set_generic_texture_font(std::uint64_t material_uuid, std::uint32_t index, std::uint64_t font_uuid) -> void;

  /** @brief Copies a material into a new, independently registered instance. Returns 0 if source_uuid doesn't resolve. */
  static auto material_create_instance(std::uint64_t source_uuid) -> std::uint64_t;

  /**
   * @brief Whether a loaded material's file content has been applied. False for an unknown uuid, always true for instances.
   *
   * Check this before CreateInstance on a loaded template: the copy is a one-time snapshot of whatever fields it has at that moment.
   */
  static auto material_is_loaded(std::uint64_t material_uuid) -> managed::bool32;

  /** @brief Frees a material instance's slot. No-op if material_uuid doesn't resolve; never call on a shared loaded material. */
  static auto material_release(std::uint64_t material_uuid) -> void;

  /**
   * @brief Overwrites one of a material's texture slots in place (0 albedo, 1 normal, 2 metallic_roughness, 3 occlusion, 4 emissive).
   *
   * Every handle to this material sees the change, so use an instance unless the change should be global.
   */
  static auto material_set_texture(std::uint64_t material_uuid, std::uint32_t slot, std::uint64_t texture_uuid) -> void;

  static auto material_set_generic_param(std::uint64_t material_uuid, std::uint32_t index, std::float_t x, std::float_t y, std::float_t z, std::float_t w) -> void;

  static auto material_set_generic_texture(std::uint64_t material_uuid, std::uint32_t index, std::uint64_t texture_uuid) -> void;

  // Script compute: every object is an opaque id into scripting_module::resources().
  // Functions returning bool log the reason and return false on misuse; the C# wrappers turn that into an exception.
  // Shader parameters bind by push_data field name via Slang reflection, type-checked against the setter.
  // Everything runs on the compute queue; dispatches are recorded into a command list and submitted together.

  /** @brief Allocates a host-visible storage buffer of max(count, 1) * stride bytes. access: 0 = upload (host writes, GPU reads), 1 = readback (GPU writes, host reads). Returns 0 on failure. */
  static auto compute_buffer_create(std::int32_t count, std::int32_t stride, std::uint32_t access) -> std::uint64_t;

  /** @brief Copies byte_count bytes into the buffer. False if the id is unknown or byte_count exceeds the buffer. */
  static auto compute_buffer_set_data(std::uint64_t id, const void* data, std::int32_t byte_count) -> managed::bool32;

  /** @brief Copies byte_count bytes out of a readback buffer. Only meaningful after the compute_commands that wrote it was submitted. */
  static auto compute_buffer_get_data(std::uint64_t id, void* data, std::int32_t byte_count) -> managed::bool32;

  /** @brief Retires the buffer. No-op for an unknown id. */
  static auto compute_buffer_release(std::uint64_t id) -> void;

  /** @brief Compiles and reflects a project-relative .slang path with a `compute_main` entry point. Returns 0 (and logs) if the file is missing, fails to compile, or its push_data doesn't fit the push-constant range. */
  static auto compute_shader_load(managed::string path) -> std::uint64_t;

  /** @brief Sets a scalar/vector field. kind must match the field's reflected kind; data points at exactly that field's size in bytes. */
  static auto compute_shader_set_value(std::uint64_t id, managed::string name, std::uint32_t kind, const void* data) -> managed::bool32;

  /** @brief Sets a sampled_texture (storage = false) or storage_texture (storage = true) field. */
  static auto compute_shader_set_texture(std::uint64_t id, managed::string name, std::uint64_t texture_uuid, bool storage) -> managed::bool32;

  /** @brief Sets a pointer field to the buffer's device address. The buffer's stride must equal the pointee's reflected stride. */
  static auto compute_shader_set_buffer(std::uint64_t id, managed::string name, std::uint64_t buffer_id) -> managed::bool32;

  /** @brief Drops the shader's parameter state. The compiled shader/pipeline stay cached engine-side by path. */
  static auto compute_shader_release(std::uint64_t id) -> void;

  /** @brief Begins a command list on the compute queue. */
  static auto compute_commands_begin() -> std::uint64_t;

  /** @brief Records a dispatch with the shader's current parameters. False if any non-sampler field was never set. */
  static auto compute_commands_dispatch(std::uint64_t id, std::uint64_t shader_id, std::uint32_t group_count_x, std::uint32_t group_count_y, std::uint32_t group_count_z) -> managed::bool32;

  /**
   * @brief Submits the list. wait = true blocks until the GPU finishes and consumes the list.
   * wait = false returns immediately; poll compute_commands_is_complete, and keep every resource
   * the list uses alive until it reports true. Either way, results are then visible to later
   * sampling, transfer (ReadPixels) and host (GetData) reads.
   */
  static auto compute_commands_submit(std::uint64_t id, bool wait) -> managed::bool32;

  /** @brief True once a list submitted with wait = false has finished on the GPU (or the id is unknown). Never blocks. */
  static auto compute_commands_is_complete(std::uint64_t id) -> managed::bool32;

  /** @brief Frees the list: discards it if never submitted, waits for it first if still running. No-op for an unknown id. */
  static auto compute_commands_release(std::uint64_t id) -> void;

  static auto canvas_get_sort_order(std::uint64_t uuid, std::int32_t* out_value) -> void;
  static auto canvas_set_sort_order(std::uint64_t uuid, std::int32_t value) -> void;

  static auto rect_transform_get_anchor_min(std::uint64_t uuid, math::vector2* out_value) -> void;
  static auto rect_transform_set_anchor_min(std::uint64_t uuid, math::vector2* value) -> void;
  static auto rect_transform_get_anchor_max(std::uint64_t uuid, math::vector2* out_value) -> void;
  static auto rect_transform_set_anchor_max(std::uint64_t uuid, math::vector2* value) -> void;
  static auto rect_transform_get_anchored_position(std::uint64_t uuid, math::vector2* out_value) -> void;
  static auto rect_transform_set_anchored_position(std::uint64_t uuid, math::vector2* value) -> void;
  static auto rect_transform_get_size_delta(std::uint64_t uuid, math::vector2* out_value) -> void;
  static auto rect_transform_set_size_delta(std::uint64_t uuid, math::vector2* value) -> void;
  static auto rect_transform_get_pivot(std::uint64_t uuid, math::vector2* out_value) -> void;
  static auto rect_transform_set_pivot(std::uint64_t uuid, math::vector2* value) -> void;

  static auto ui_image_get_tint(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_image_set_tint(std::uint64_t uuid, math::color* value) -> void;

  /** @brief Loads the sprite at a project-relative @p path. */
  static auto ui_image_load_sprite(std::uint64_t uuid, managed::string path) -> void;

  static auto ui_text_get_text(std::uint64_t uuid) -> managed::string;
  static auto ui_text_set_text(std::uint64_t uuid, managed::string value) -> void;
  static auto ui_text_get_font_size(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_text_set_font_size(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_text_get_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_text_set_color(std::uint64_t uuid, math::color* value) -> void;

  /**
   * @brief Applies every queued set_geometry/set_instances call and retires released instance buffers.
   *
   * Runs on render idle: replacing a mesh while the render thread records would retire buffers it still uses.
   */
  static auto apply_pending_geometry() -> void;

  static auto ui_text_get_alignment(std::uint64_t uuid, std::uint32_t* out_horizontal, std::uint32_t* out_vertical) -> void;

  static auto ui_text_set_alignment(std::uint64_t uuid, std::uint32_t horizontal, std::uint32_t vertical) -> void;

  /** @brief Loads the font at a project-relative @p path. */
  static auto ui_text_load_font(std::uint64_t uuid, managed::string path) -> void;

  static auto ui_button_get_interactable(std::uint64_t uuid) -> managed::bool32;
  static auto ui_button_set_interactable(std::uint64_t uuid, bool value) -> void;
  static auto ui_button_get_normal_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_button_set_normal_color(std::uint64_t uuid, math::color* value) -> void;
  static auto ui_button_get_hovered_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_button_set_hovered_color(std::uint64_t uuid, math::color* value) -> void;
  static auto ui_button_get_pressed_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_button_set_pressed_color(std::uint64_t uuid, math::color* value) -> void;
  static auto ui_button_get_is_hovered(std::uint64_t uuid) -> managed::bool32;
  static auto ui_button_get_is_pressed(std::uint64_t uuid) -> managed::bool32;
  static auto ui_button_get_was_clicked(std::uint64_t uuid) -> managed::bool32;

  static auto canvas_group_get_alpha(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto canvas_group_set_alpha(std::uint64_t uuid, std::float_t value) -> void;
  static auto canvas_group_get_interactable(std::uint64_t uuid) -> managed::bool32;
  static auto canvas_group_set_interactable(std::uint64_t uuid, bool value) -> void;
  static auto canvas_group_get_blocks_raycasts(std::uint64_t uuid) -> managed::bool32;
  static auto canvas_group_set_blocks_raycasts(std::uint64_t uuid, bool value) -> void;
  static auto canvas_group_get_ignore_parent_groups(std::uint64_t uuid) -> managed::bool32;
  static auto canvas_group_set_ignore_parent_groups(std::uint64_t uuid, bool value) -> void;

  /** @brief Whether the cursor is over an interactable UI element; world picking should check this first. */
  static auto canvas_wants_pointer_capture() -> managed::bool32;

  static auto ui_toggle_get_is_on(std::uint64_t uuid) -> managed::bool32;
  static auto ui_toggle_set_is_on(std::uint64_t uuid, bool value) -> void;
  static auto ui_toggle_get_interactable(std::uint64_t uuid) -> managed::bool32;
  static auto ui_toggle_set_interactable(std::uint64_t uuid, bool value) -> void;

  static auto ui_slider_get_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_slider_set_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_slider_get_min_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_slider_set_min_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_slider_get_max_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_slider_set_max_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_slider_get_whole_numbers(std::uint64_t uuid) -> managed::bool32;
  static auto ui_slider_set_whole_numbers(std::uint64_t uuid, bool value) -> void;
  static auto ui_slider_get_interactable(std::uint64_t uuid) -> managed::bool32;
  static auto ui_slider_set_interactable(std::uint64_t uuid, bool value) -> void;

  static auto ui_scrollbar_get_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_scrollbar_set_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_scrollbar_get_size(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_scrollbar_set_size(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_scrollbar_get_interactable(std::uint64_t uuid) -> managed::bool32;
  static auto ui_scrollbar_set_interactable(std::uint64_t uuid, bool value) -> void;

  static auto ui_scroll_rect_get_normalized_position(std::uint64_t uuid, math::vector2* out_value) -> void;
  static auto ui_scroll_rect_set_normalized_position(std::uint64_t uuid, math::vector2* value) -> void;
  static auto ui_scroll_rect_get_horizontal(std::uint64_t uuid) -> managed::bool32;
  static auto ui_scroll_rect_set_horizontal(std::uint64_t uuid, bool value) -> void;
  static auto ui_scroll_rect_get_vertical(std::uint64_t uuid) -> managed::bool32;
  static auto ui_scroll_rect_set_vertical(std::uint64_t uuid, bool value) -> void;
  static auto ui_scroll_rect_set_content(std::uint64_t uuid, std::uint64_t content) -> void;

  static auto ui_mask_get_show_mask_graphic(std::uint64_t uuid) -> managed::bool32;
  static auto ui_mask_set_show_mask_graphic(std::uint64_t uuid, bool value) -> void;

  static auto layout_group_get(std::uint64_t uuid, bool vertical, layout_group_data* out_value) -> void;

  static auto layout_group_set(std::uint64_t uuid, bool vertical, const layout_group_data* value) -> void;

  static auto layout_element_get(std::uint64_t uuid, layout_element_data* out_value) -> void;

  static auto layout_element_set(std::uint64_t uuid, const layout_element_data* value) -> void;

  static auto content_size_fitter_get(std::uint64_t uuid, std::uint32_t* out_horizontal, std::uint32_t* out_vertical) -> void;

  static auto content_size_fitter_set(std::uint64_t uuid, std::uint32_t horizontal, std::uint32_t vertical) -> void;

  template<typename Type>
  static auto register_managed_component(std::string_view full_name, managed::assembly& core_assembly) -> void {
    auto& type = core_assembly.get_type(full_name);
  
    if (type) {
      _add_component_functions[type.get_type_id()] = [](scenes::node& node) -> void { 
        node.add_component<Type>();
      };
      _has_component_functions[type.get_type_id()] = [](const scenes::node& node) -> bool {
        return node.has_component<Type>();
      };
      _remove_component_functions[type.get_type_id()] = [](scenes::node& node) -> bool { 
        return node.remove_component<Type>();
      };
    } else {
      utility::logger<"scripting">::warn("No C# component class found for {}!", full_name);
    }
  }

private:

  inline static auto _add_component_functions = std::unordered_map<managed::type_id, std::function<void(scenes::node&)>>{};
  inline static auto _has_component_functions = std::unordered_map<managed::type_id, std::function<bool(const scenes::node&)>>{};
  inline static auto _remove_component_functions = std::unordered_map<managed::type_id, std::function<bool(scenes::node&)>>{};

}; // class interop

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_INTEROP_HPP_
