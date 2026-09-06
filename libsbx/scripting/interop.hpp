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
