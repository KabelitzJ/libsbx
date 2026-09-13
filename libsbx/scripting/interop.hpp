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

  static auto scripting_attach_script(std::uint64_t uuid, managed::string class_name) -> void;
  static auto scripting_get_instance(std::uint64_t uuid, managed::string class_name) -> managed::object;

  static auto behavior_add_component(std::uint64_t uuid, managed::reflection_type component_type) -> void;
  static auto behavior_has_component(std::uint64_t uuid, managed::reflection_type component_type) -> bool;
  static auto behavior_remove_component(std::uint64_t uuid, managed::reflection_type component_type) -> bool;

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

  /**
   * @brief Instantiates the prefab at @p path (project-relative, same convention as every other
   * asset path taken from script/YAML) as a new node subtree, parented under @p parent_uuid
   * (0 = top-level, same "no real node has uuid 0" convention as node_set_parent's parent_uuid).
   * Any scripts baked into the prefab's nodes are brought to life immediately (instantiate +
   * OnCreate) if the scene is currently simulating — see scripting_module::instantiate_subtree_scripts.
   * Returns 0 if @p path doesn't resolve to a valid prefab. An invalid @p parent_uuid doesn't fail
   * the call — the instance is still created and returned, just left at the top level, same
   * recovery as node_set_parent's own invalid-parent handling.
   */
  static auto node_instantiate_prefab(managed::string path, std::uint64_t parent_uuid) -> std::uint64_t;

  static auto node_destroy(std::uint64_t uuid) -> void;

  static auto node_set_parent(std::uint64_t uuid, std::uint64_t parent_uuid) -> void;

  static auto node_set_active(std::uint64_t uuid, bool active) -> void;

  static auto node_get_is_active(std::uint64_t uuid) -> bool;

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

  /** @brief Inverse of camera_screen_point_to_ray -- returns false (out_position left untouched) if world_position projects behind the camera (clip.w <= 0). */
  static auto camera_world_to_screen_point(math::vector3* world_position, math::vector2* out_position) -> bool;

  static auto camera_main_get_position(math::vector3* position) -> void;

  static auto camera_main_set_position(math::vector3* position) -> void;

  static auto camera_main_get_rotation(math::quaternion* rotation) -> void;

  static auto camera_main_set_rotation(math::quaternion* rotation) -> void;

  static auto camera_main_get_forward(math::vector3* forward) -> void;

  static auto camera_main_get_right(math::vector3* right) -> void;

  static auto camera_main_get_up(math::vector3* up) -> void;

  static auto camera_get_viewport(math::vector2* viewport) -> void;

  static auto camera_get_viewport_offset(math::vector2* offset) -> void;

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
   * primitives and heightfield_collider terrain (see physics::physics_module::raycast). @p
   * layer_mask is the raw bits of a Sbx.Core.Physics.LayerMask (0xFFFFFFFF = every layer).
   * Returns false, leaving every out parameter untouched, when nothing was hit within max_distance.
   */
  static auto physics_raycast(math::ray* ray, std::float_t max_distance, std::uint32_t layer_mask, std::uint64_t* out_node_uuid, math::vector3* out_point, math::vector3* out_normal, std::float_t* out_distance) -> bool;

  /** @brief Bakes the navmesh from the active scene's static geometry right now (physics::physics_module::bake_navmesh). Returns whether the bake produced any usable polygons. */
  static auto nav_bake(std::float_t agent_radius, std::float_t agent_height, std::float_t agent_max_slope, std::float_t agent_max_climb, std::float_t cell_size, std::float_t cell_height, std::float_t region_min_size, std::float_t edge_max_length, std::float_t edge_max_error, std::int32_t verts_per_poly) -> bool;

  /** @brief Whether physics_module currently holds a baked navmesh. */
  static auto nav_has_navmesh() -> bool;

  /** @brief The closest point on the navmesh to @p point (nearest polygon, then clamped to it). Returns false, leaving out_result untouched, if there's no navmesh yet. */
  static auto nav_sample_position(math::vector3* point, math::vector3* out_result) -> bool;

  /** @brief Requests the node's nav_agent walk to @p target (physics::physics_module::request_agent_move). Returns false if there's no navmesh, the node has no nav_agent, or no path was found. */
  static auto nav_agent_set_destination(std::uint64_t uuid, math::vector3* target) -> bool;

  /** @brief nav_agent_state (0 = Idle, 1 = Moving, 2 = TargetUnreachable) as a plain byte for the C# enum. */
  static auto nav_agent_get_state(std::uint64_t uuid) -> std::uint8_t;

  static auto nav_agent_get_velocity(std::uint64_t uuid, math::vector3* out_velocity) -> void;

  /** @brief Straight-line distance from the agent's current corridor position to its current target. */
  static auto nav_agent_get_remaining_distance(std::uint64_t uuid, std::float_t* out_distance) -> void;

  static auto nav_agent_get_radius(std::uint64_t uuid, std::float_t* out_radius) -> void;
  static auto nav_agent_set_radius(std::uint64_t uuid, std::float_t radius) -> void;

  /** @brief Used to vertically cull crowd neighbors -- two agents whose height gap exceeds half the sum of their heights are never treated as colliding, even if close in X/Z (e.g. one on a ramp above the other). */
  static auto nav_agent_get_height(std::uint64_t uuid, std::float_t* out_height) -> void;
  static auto nav_agent_set_height(std::uint64_t uuid, std::float_t height) -> void;

  /** @brief How far above the sampled navmesh surface height the node's own pivot should sit (e.g. half the height for a capsule centered on its own origin, 0 for a foot-pivoted model). */
  static auto nav_agent_get_base_offset(std::uint64_t uuid, std::float_t* out_base_offset) -> void;
  static auto nav_agent_set_base_offset(std::uint64_t uuid, std::float_t base_offset) -> void;

  static auto nav_agent_get_speed(std::uint64_t uuid, std::float_t* out_speed) -> void;
  static auto nav_agent_set_speed(std::uint64_t uuid, std::float_t speed) -> void;

  static auto nav_agent_get_acceleration(std::uint64_t uuid, std::float_t* out_acceleration) -> void;
  static auto nav_agent_set_acceleration(std::uint64_t uuid, std::float_t acceleration) -> void;

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

  /** @brief Project-relative path, same convention as particle_effect_load/node_instantiate_prefab. */
  static auto ui_image_load_sprite(std::uint64_t uuid, managed::string path) -> void;

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

  static auto canvas_group_get_alpha(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto canvas_group_set_alpha(std::uint64_t uuid, std::float_t value) -> void;
  static auto canvas_group_get_interactable(std::uint64_t uuid) -> bool;
  static auto canvas_group_set_interactable(std::uint64_t uuid, bool value) -> void;
  static auto canvas_group_get_blocks_raycasts(std::uint64_t uuid) -> bool;
  static auto canvas_group_set_blocks_raycasts(std::uint64_t uuid, bool value) -> void;
  static auto canvas_group_get_ignore_parent_groups(std::uint64_t uuid) -> bool;
  static auto canvas_group_set_ignore_parent_groups(std::uint64_t uuid, bool value) -> void;

  /** @brief Whether the cursor is currently over any interactable UI element -- see canvas::canvas_module's own doc comment. Any world-picking code (a road tool) should check this before casting its own ray. */
  static auto canvas_wants_pointer_capture() -> bool;

  static auto ui_toggle_get_is_on(std::uint64_t uuid) -> bool;
  static auto ui_toggle_set_is_on(std::uint64_t uuid, bool value) -> void;
  static auto ui_toggle_get_interactable(std::uint64_t uuid) -> bool;
  static auto ui_toggle_set_interactable(std::uint64_t uuid, bool value) -> void;

  static auto ui_slider_get_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_slider_set_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_slider_get_min_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_slider_set_min_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_slider_get_max_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_slider_set_max_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_slider_get_whole_numbers(std::uint64_t uuid) -> bool;
  static auto ui_slider_set_whole_numbers(std::uint64_t uuid, bool value) -> void;
  static auto ui_slider_get_interactable(std::uint64_t uuid) -> bool;
  static auto ui_slider_set_interactable(std::uint64_t uuid, bool value) -> void;

  static auto ui_scrollbar_get_value(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_scrollbar_set_value(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_scrollbar_get_size(std::uint64_t uuid, std::float_t* out_value) -> void;
  static auto ui_scrollbar_set_size(std::uint64_t uuid, std::float_t value) -> void;
  static auto ui_scrollbar_get_interactable(std::uint64_t uuid) -> bool;
  static auto ui_scrollbar_set_interactable(std::uint64_t uuid, bool value) -> void;

  static auto ui_scroll_rect_get_normalized_position(std::uint64_t uuid, math::vector2* out_value) -> void;
  static auto ui_scroll_rect_set_normalized_position(std::uint64_t uuid, math::vector2* value) -> void;
  static auto ui_scroll_rect_get_horizontal(std::uint64_t uuid) -> bool;
  static auto ui_scroll_rect_set_horizontal(std::uint64_t uuid, bool value) -> void;
  static auto ui_scroll_rect_get_vertical(std::uint64_t uuid) -> bool;
  static auto ui_scroll_rect_set_vertical(std::uint64_t uuid, bool value) -> void;

  static auto ui_mask_get_show_mask_graphic(std::uint64_t uuid) -> bool;
  static auto ui_mask_set_show_mask_graphic(std::uint64_t uuid, bool value) -> void;

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
