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

#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/type.hpp>
#include <libsbx/scripting/managed/assembly.hpp>

namespace sbx::scripting {

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

  static auto behavior_add_component(std::uint64_t uuid, managed::reflection_type component_type) -> void;

  static auto behavior_has_component(std::uint64_t uuid, managed::reflection_type component_type) -> bool;

  // static auto behavior_remove_component(std::uint64_t uuid, managed::reflection_type component_type) -> bool;

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

  static auto animator_get_playing(std::uint64_t uuid) -> bool;

  static auto animator_set_playing(std::uint64_t uuid, bool value) -> void;

  static auto animator_get_current_state_name(std::uint64_t uuid) -> managed::string;

  static auto animator_set_float(std::uint64_t uuid, managed::string name, std::float_t value) -> void;

  static auto animator_set_bool(std::uint64_t uuid, managed::string name, bool value) -> void;

  static auto animator_set_int(std::uint64_t uuid, managed::string name, std::int32_t value) -> void;

  static auto animator_set_trigger(std::uint64_t uuid, managed::string name) -> void;

  static auto animator_get_float(std::uint64_t uuid, managed::string name) -> std::float_t;

  static auto animator_get_bool(std::uint64_t uuid, managed::string name) -> bool;

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

  static auto node_destroy(std::uint64_t uuid) -> void;

  static auto node_set_parent(std::uint64_t uuid, std::uint64_t parent_uuid) -> void;

  /** @brief Loads (or reassigns) which .particle_effect asset this node's ParticleEffect component plays -- @p path is project-relative, same convention as every other asset path taken from script/YAML. */
  static auto particle_effect_load(std::uint64_t uuid, managed::string path) -> void;

  static auto particle_effect_play(std::uint64_t uuid) -> void;

  static auto particle_effect_pause(std::uint64_t uuid) -> void;

  static auto particle_effect_stop(std::uint64_t uuid) -> void;

  static auto particle_effect_get_loop(std::uint64_t uuid) -> bool;

  static auto particle_effect_set_loop(std::uint64_t uuid, bool value) -> void;

  static auto particle_effect_get_is_playing(std::uint64_t uuid) -> bool;

  // static auto character_controller_get_height(std::uint64_t uuid, std::float_t* height) -> void;

  // static auto character_controller_get_radius(std::uint64_t uuid, std::float_t* radius) -> void;

  // static auto character_controller_get_slope_limit(std::uint64_t uuid, std::float_t* slope_limit) -> void;

  // static auto character_controller_get_step_offset(std::uint64_t uuid, std::float_t* step_offset) -> void;

  // static auto character_controller_get_is_grounded(std::uint64_t uuid) -> managed::bool32;

  // static auto character_controller_get_flags(std::uint64_t uuid, std::uint8_t* flags) -> void;

  // static auto character_controller_move(std::uint64_t uuid, math::vector3* displacement) -> void;

  static auto input_is_key_pressed(platform::key key) -> managed::bool32;

  static auto input_is_key_down(platform::key key) -> managed::bool32;

  static auto input_is_key_released(platform::key key) -> managed::bool32;

  static auto input_is_mouse_button_pressed(platform::mouse_button mouse_button) -> managed::bool32;

  static auto input_is_mouse_button_down(platform::mouse_button mouse_button) -> managed::bool32;

  static auto input_is_mouse_button_released(platform::mouse_button mouse_button) -> managed::bool32;

  static auto input_mouse_position(math::vector2* position) -> void;

  static auto input_scroll_delta(math::vector2* scroll_delta) -> void;

  // Main*: derived from scenes::scene::active_camera()'s own world_transform -- same source
  // Transform's getters use for an arbitrary node, just always resolved against whichever node is
  // the active camera instead of the calling script's own uuid.
  static auto camera_screen_point_to_ray(math::ray* ray, math::vector2* position) -> void;

  static auto camera_main_get_position(math::vector3* position) -> void;

  static auto camera_main_set_position(math::vector3* position) -> void;

  static auto camera_main_get_rotation(math::quaternion* rotation) -> void;

  static auto camera_main_set_rotation(math::quaternion* rotation) -> void;

  static auto camera_main_get_forward(math::vector3* forward) -> void;

  static auto camera_main_get_right(math::vector3* right) -> void;

  static auto camera_main_get_up(math::vector3* up) -> void;

  static auto camera_get_viewport(math::vector2* viewport) -> void;

  // Per-node scenes::camera field access, for a script sitting on a camera node itself (GetComponent<CameraSettings>()) -- distinct from the Main-prefixed functions above, which always target scene.active_camera() regardless of which node the calling script is on.
  static auto camera_get_fov_degrees(std::uint64_t uuid, std::float_t* fov_degrees) -> void;

  static auto camera_set_fov_degrees(std::uint64_t uuid, std::float_t fov_degrees) -> void;

  static auto camera_get_near_plane(std::uint64_t uuid, std::float_t* near_plane) -> void;

  static auto camera_set_near_plane(std::uint64_t uuid, std::float_t near_plane) -> void;

  static auto camera_get_far_plane(std::uint64_t uuid, std::float_t* far_plane) -> void;

  static auto camera_set_far_plane(std::uint64_t uuid, std::float_t far_plane) -> void;

  static auto camera_get_exposure(std::uint64_t uuid, std::float_t* exposure) -> void;

  static auto camera_set_exposure(std::uint64_t uuid, std::float_t exposure) -> void;

  static auto time_delta_time(std::float_t* delta_time) -> void;

  /**
   * @brief Raycasts against the active scene's broadphase -- shape_collider/convex mesh_collider
   * primitives and heightfield_collider terrain (see physics::physics_module::raycast). Returns
   * false, leaving every out parameter untouched, when nothing was hit within max_distance.
   */
  static auto physics_raycast(math::ray* ray, std::float_t max_distance, std::uint64_t* out_node_uuid, math::vector3* out_point, math::vector3* out_normal, std::float_t* out_distance) -> bool;

  /** @brief (Re)generates the active scene's terrain -- see terrain::terrain_module::generate. Replaces any terrain a previous call generated. */
  static auto terrain_generate(std::uint32_t width, std::uint32_t depth, std::float_t cell_size, std::float_t frequency, std::float_t amplitude, std::uint32_t octaves) -> void;

  /** @brief Elevation sampling against the active scene's terrain_module heightmap -- 0 if no terrain has been generated yet (see terrain::heightmap::sample_bilinear's own empty-map fallback). */
  static auto terrain_sample_height(math::vector2* world_xz, std::float_t* out_height) -> void;

  /** @brief Surface normal sampling against the active scene's terrain_module heightmap -- +Y if no terrain has been generated yet (see terrain::heightmap::sample_normal's own empty-map fallback). */
  static auto terrain_sample_normal(math::vector2* world_xz, math::vector3* out_normal) -> void;

  /**
   * @brief Builds a mesh from raw vertex/index data and assigns it to this node's mesh_renderer
   * component (creating the component, and a backing material, the first time this is called for a
   * given node). See assets::asset_residency::create_mesh -- the mesh renders through the ordinary
   * mesh_renderer/opaque_pass path, no bespoke rendering system involved.
   *
   * Reuses the node's already-assigned material across repeated calls instead of creating a new
   * one every time: a live-edited mesh (e.g. a road network's ghost preview while dragging) may
   * call this every frame, and asset_residency::create_material has a fixed material_capacity that
   * a fresh material per call would exhaust in short order.
   */
  static auto mesh_renderer_set_geometry(std::uint64_t uuid, math::vector3* positions, math::vector3* normals, math::vector2* uvs, std::uint32_t vertex_count, std::uint32_t* indices, std::uint32_t index_count, math::color* tint) -> void;

  // Canvas: node uuid -> canvas::canvas/rect_transform/ui_image/ui_text/ui_button field access,
  // same uuid-resolve-then-get/set convention as Transform_*/Rigidbody_* above.
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

  static auto ui_text_get_text(std::uint64_t uuid) -> managed::string;
  static auto ui_text_set_text(std::uint64_t uuid, managed::string value) -> void;
  static auto ui_text_get_font_size(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_text_set_font_size(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_text_get_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_text_set_color(std::uint64_t uuid, math::color* value) -> void;

  static auto ui_button_get_interactable(std::uint64_t uuid) -> bool;
  static auto ui_button_set_interactable(std::uint64_t uuid, bool value) -> void;
  static auto ui_button_get_normal_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_button_set_normal_color(std::uint64_t uuid, math::color* value) -> void;
  static auto ui_button_get_hovered_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_button_set_hovered_color(std::uint64_t uuid, math::color* value) -> void;
  static auto ui_button_get_pressed_color(std::uint64_t uuid, math::color* out_value) -> void;
  static auto ui_button_set_pressed_color(std::uint64_t uuid, math::color* value) -> void;
  static auto ui_button_get_is_hovered(std::uint64_t uuid) -> bool;
  static auto ui_button_get_is_pressed(std::uint64_t uuid) -> bool;
  static auto ui_button_get_was_clicked(std::uint64_t uuid) -> bool;

  /** @brief Whether the cursor is currently over any interactable UI element -- see canvas::canvas_module's own doc comment. Any world-picking code (a road tool) should check this before casting its own ray. */
  static auto canvas_wants_pointer_capture() -> bool;

  template<typename Type>
  static auto register_managed_component(std::string_view name, managed::assembly& core_assembly) -> void {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  
    const auto component_name = std::format("Sbx.Core.{}", name);
  
    auto& type = core_assembly.get_type(component_name);
  
    if (type) {
      _add_component_functions[type.get_type_id()] = [&scenes_module](scenes::node& node) -> void { 
        node.add_component<Type>();
      };
      _has_component_functions[type.get_type_id()] = [&scenes_module](const scenes::node& node) -> bool {
        return node.has_component<Type>();
      };
      // _remove_component_functions[type.get_type_id()] = [&scenes_module](scenes::node& node) { 
      //   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
      //   auto& scene = scenes_module.active_scene();
      //   auto& environment = scene.environment();
      //   auto& graph = scene.graph();
  
      //   scene.remove_component<Type>(node);
      // };
    } else {
      utility::logger<"scripting">::warn("No C# component class found for {}!", component_name);
    }
  }

private:

  inline static auto _add_component_functions = std::unordered_map<managed::type_id, std::function<void(scenes::node&)>>{};
  inline static auto _has_component_functions = std::unordered_map<managed::type_id, std::function<bool(const scenes::node&)>>{};
  // inline static auto _remove_component_functions = std::unordered_map<managed::type_id, std::function<void(scenes::node)>>{};

}; // class interop

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_INTEROP_HPP_
