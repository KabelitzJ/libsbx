// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_INTEROP_HPP_
#define LIBSBX_SCRIPTING_INTEROP_HPP_

#include <functional>

#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/type_name.hpp>

#include <libsbx/devices/input.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/character_controller.hpp>

#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/type.hpp>

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

  static auto behavior_add_component(std::uint32_t node, managed::reflection_type component_type) -> void;

  static auto behavior_has_component(std::uint32_t node, managed::reflection_type component_type) -> bool;

  // static auto behavior_remove_component(std::uint32_t node, managed::reflection_type component_type) -> bool;

  static auto tag_get_tag(std::uint32_t node) -> managed::string;

  static auto tag_set_tag(std::uint32_t node, managed::string tag) -> void;

  static auto transform_get_position(std::uint32_t node, math::vector3* position) -> void;

  static auto transform_set_position(std::uint32_t node, math::vector3* position) -> void;

  static auto transform_get_world_position(std::uint32_t node, math::vector3* position) -> void;

  static auto transform_get_rotation(std::uint32_t node, math::quaternion* rotation) -> void;

  static auto transform_set_rotation(std::uint32_t node, math::quaternion* rotation) -> void;

  static auto transform_get_right(std::uint32_t node, math::vector3* right) -> void;

  static auto transform_get_forward(std::uint32_t node, math::vector3* forward) -> void;

  static auto transform_get_up(std::uint32_t node, math::vector3* up) -> void;

  static auto transform_look_at(std::uint32_t node, math::vector3* target) -> void;

  static auto character_controller_get_height(std::uint32_t node, std::float_t* height) -> void;

  static auto character_controller_get_radius(std::uint32_t node, std::float_t* radius) -> void;

  static auto character_controller_get_slope_limit(std::uint32_t node, std::float_t* slope_limit) -> void;

  static auto character_controller_get_step_offset(std::uint32_t node, std::float_t* step_offset) -> void;

  static auto character_controller_get_is_grounded(std::uint32_t node) -> managed::bool32;

  static auto character_controller_get_flags(std::uint32_t node, std::uint8_t* flags) -> void;

  static auto character_controller_move(std::uint32_t node, math::vector3* displacement) -> void;

  static auto input_is_key_pressed(devices::key key) -> managed::bool32;

  static auto input_is_key_down(devices::key key) -> managed::bool32;

  static auto input_is_key_released(devices::key key) -> managed::bool32;

  static auto input_is_mouse_button_pressed(devices::mouse_button mouse_button) -> managed::bool32;

  static auto input_is_mouse_button_down(devices::mouse_button mouse_button) -> managed::bool32;

  static auto input_is_mouse_button_released(devices::mouse_button mouse_button) -> managed::bool32;

  static auto input_mouse_position(math::vector2* position) -> void;

  static auto input_scroll_delta(math::vector2* scroll_delta) -> void;

  static auto camera_screen_point_to_ray(math::ray* ray, math::vector2* position) -> void;

  static auto camera_get_position(math::vector3* position) -> void;

  static auto camera_set_position(math::vector3* position) -> void;

  static auto camera_get_rotation(math::quaternion* rotation) -> void;

  static auto camera_set_rotation(math::quaternion* rotation) -> void;

  static auto camera_get_forward(math::vector3* forward) -> void;

  static auto camera_get_right(math::vector3* right) -> void;

  static auto camera_get_up(math::vector3* up) -> void;

  static auto time_delta_time(std::float_t* delta_time) -> void;

  template<typename Type>
  static auto register_managed_component(std::string_view name, managed::assembly& core_assembly) -> void {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  
    const auto component_name = std::format("Sbx.Core.{}", name);
  
    auto& type = core_assembly.get_type(component_name);
  
    if (type) {
      _add_component_functions[type.get_type_id()] = [&scenes_module](scenes::node node) -> void { 
        auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
        auto& scene = scenes_module.scene();
        auto& graph = scene.graph();
  
        graph.add_component<Type>(node);
      };
      _has_component_functions[type.get_type_id()] = [&scenes_module](scenes::node node) -> bool {
        auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
        auto& scene = scenes_module.scene();
        auto& graph = scene.graph();
  
        return graph.has_component<Type>(node);
      };
      // _remove_component_functions[type.get_type_id()] = [&scenes_module](scenes::node node) { 
      //   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
      //   auto& scene = scenes_module.scene();
      //   auto& environment = scene.environment();
      //   auto& graph = scene.graph();
  
      //   scene.remove_component<Type>(node);
      // };
    } else {
      utility::logger<"scripting">::warn("No C# component class found for {}!", component_name);
    }
  }

private:

  inline static auto _add_component_functions = std::unordered_map<managed::type_id, std::function<void(scenes::node)>>{};
  inline static auto _has_component_functions = std::unordered_map<managed::type_id, std::function<bool(scenes::node)>>{};
  // inline static auto _remove_component_functions = std::unordered_map<managed::type_id, std::function<void(scenes::node)>>{};

}; // class interop

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_INTEROP_HPP_