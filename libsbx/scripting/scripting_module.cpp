// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/scripting_module.hpp>

#include <fmt/core.h>
#include <fmt/args.h>
#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/scripting/interop.hpp>

namespace sbx::scripting {

scripting_module::scripting_module() {
  // auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();

  // const auto dotnet_dir = filesystem_module.native_path_of(std::string{"engine://dotnet"});

  const auto dotnet_dir = std::filesystem::path{"build/x86_64/gcc/debug/dotnet"};

  auto config = scripting::managed::rumtime_config{
		.backend_path = dotnet_dir.generic_string(),
		.exception_callback = _exception_callback
	};

  _runtime.initialize(config);

  _context = _runtime.create_assembly_load_context("ScriptingContext");

  auto core_assembly_path = std::filesystem::path{dotnet_dir / "Sbx.Core.dll"};

	_core_assembly = _context.load_assembly(core_assembly_path.string());

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Log_LogMessage", reinterpret_cast<void*>(&interop::log_log_message));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Behavior_AddComponent", reinterpret_cast<void*>(&interop::behavior_add_component));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Behavior_HasComponent", reinterpret_cast<void*>(&interop::behavior_has_component));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Behavior_RemoveComponent", reinterpret_cast<void*>(&interop::behavior_remove_component));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Tag_GetTag", reinterpret_cast<void*>(&interop::tag_get_tag));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Tag_SetTag", reinterpret_cast<void*>(&interop::tag_set_tag));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetPosition", reinterpret_cast<void*>(&interop::transform_get_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_SetPosition", reinterpret_cast<void*>(&interop::transform_set_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetWorldPosition", reinterpret_cast<void*>(&interop::transform_get_world_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetRotation", reinterpret_cast<void*>(&interop::transform_get_rotation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_SetRotation", reinterpret_cast<void*>(&interop::transform_set_rotation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetRight", reinterpret_cast<void*>(&interop::transform_get_right));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetForward", reinterpret_cast<void*>(&interop::transform_get_forward));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetUp", reinterpret_cast<void*>(&interop::transform_get_up));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_LookAt", reinterpret_cast<void*>(&interop::transform_look_at));

  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetHeight", reinterpret_cast<void*>(&interop::character_controller_get_height));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetRadius", reinterpret_cast<void*>(&interop::character_controller_get_radius));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetSlopeLimit", reinterpret_cast<void*>(&interop::character_controller_get_slope_limit));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetStepOffset", reinterpret_cast<void*>(&interop::character_controller_get_step_offset));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetIsGrounded", reinterpret_cast<void*>(&interop::character_controller_get_is_grounded));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetFlags", reinterpret_cast<void*>(&interop::character_controller_get_flags));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_Move", reinterpret_cast<void*>(&interop::character_controller_move));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsKeyPressed", reinterpret_cast<void*>(&interop::input_is_key_pressed));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsKeyDown", reinterpret_cast<void*>(&interop::input_is_key_down));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsKeyReleased", reinterpret_cast<void*>(&interop::input_is_key_released));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsMouseButtonPressed", reinterpret_cast<void*>(&interop::input_is_mouse_button_pressed));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsMouseButtonDown", reinterpret_cast<void*>(&interop::input_is_mouse_button_down));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsMouseButtonReleased", reinterpret_cast<void*>(&interop::input_is_mouse_button_released));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_MousePosition", reinterpret_cast<void*>(&interop::input_mouse_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_ScrollDelta", reinterpret_cast<void*>(&interop::input_scroll_delta));

  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_ScreenPointToRay", reinterpret_cast<void*>(&interop::camera_screen_point_to_ray));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetPosition", reinterpret_cast<void*>(&interop::camera_get_position));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetPosition", reinterpret_cast<void*>(&interop::camera_set_position));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetRotation", reinterpret_cast<void*>(&interop::camera_get_rotation));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetRotation", reinterpret_cast<void*>(&interop::camera_set_rotation));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetForward", reinterpret_cast<void*>(&interop::camera_get_forward));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetRight", reinterpret_cast<void*>(&interop::camera_get_right));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetUp", reinterpret_cast<void*>(&interop::camera_get_up));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetViewport", reinterpret_cast<void*>(&interop::camera_get_viewport));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Time_DeltaTime", reinterpret_cast<void*>(&interop::time_delta_time));

  interop::register_managed_component<scenes::tag>("Tag", _core_assembly);
  interop::register_managed_component<scenes::local_transform>("Transform", _core_assembly);
  // interop::register_managed_component<physics::character_controller>("CharacterController", _core_assembly);

  _core_assembly.upload_internal_calls();
}

scripting_module::~scripting_module() {
  _runtime.shutdown();
}

auto scripting_module::update() -> void {
  SBX_PROFILE_SCOPE("scripting_module::update");

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto scripts_query = scene.query<scripting::scripts>();

  for (auto&& [node, scripts] : scripts_query.each()) {
    for (auto& instance : scripts.instances) {
      instance.invoke("OnUpdate");
    }
  }
}

auto scripting_module::load_assembly(const std::filesystem::path& assembly_path, std::initializer_list<internal_call> bindings) -> void {
  _assembly_path = assembly_path;

  auto& assembly = _context.get_or_load_assembly(_assembly_path.string());

  for (const auto& binding : bindings) {
    assembly.add_internal_call(binding.type_name, binding.method_name, binding.function);
  }

  assembly.upload_internal_calls();

  utility::logger<"scripting">::info("Loaded game assembly '{}' with {} bindings", assembly_path.string(), bindings.size());
}

auto scripting_module::instantiate(scenes::node& node, std::string_view class_name) -> managed::object {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto& assembly = _context.get_or_load_assembly(_assembly_path.string());

  auto type = assembly.get_type(class_name);

  auto instance = type.create_instance();

  instance.set_field_value("UUID", static_cast<std::uint64_t>(node.get_component<scenes::id>().value()));

  instance.invoke("OnCreate");

  auto& scripts = node.get_or_add_component<scripting::scripts>();

  scripts.instances.push_back(instance);

  return instance;
}

auto scripting_module::_exception_callback(std::string_view message) -> void {
  utility::logger<"scripting">::error("Script runtime error: {}", message); 

  throw script_runtime_error{std::string{message}};
}

} // namespace sbx::scripting
