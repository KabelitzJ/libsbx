// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/scripting_module.hpp>

#include <algorithm>

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
  const auto dotnet_dir = _dotnet_directory();

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

  _load_game_assembly();
}

scripting_module::~scripting_module() {
  _runtime.shutdown();
}

auto scripting_module::update() -> void {
  SBX_PROFILE_SCOPE("scripting_module::update");

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  // Scripts only tick while the scene is actually simulating — see scenes_module::is_simulating's
  // doc comment. Without this, OnUpdate would run continuously in the editor while just editing.
  if (!scenes_module.is_simulating()) {
    return;
  }

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
  if (!_has_game_assembly) {
    utility::logger<"scripting">::error("Cannot instantiate '{}' — no compiled game assembly is loaded (see last_compile_succeeded())", class_name);

    return managed::object{};
  }

  auto type = _game_assembly.get_type(class_name);

  auto instance = type.create_instance();

  instance.set_field_value("UUID", static_cast<std::uint64_t>(node.get_component<scenes::id>().value()));

  // Apply any persisted field overrides before OnCreate, so scripted logic in OnCreate sees
  // author-set values immediately (mirrors Unity applying serialized fields before Awake/Start).
  if (node.has_component<scenes::script_component>()) {
    const auto& persisted = node.get_component<scenes::script_component>();

    for (const auto& entry : persisted.scripts) {
      if (entry.class_name == class_name) {
        _apply_field_overrides(instance, entry);
        break;
      }
    }
  }

  instance.invoke("OnCreate");

  auto& scripts = node.get_or_add_component<scripting::scripts>();

  scripts.instances.push_back(instance);

  return instance;
}

auto scripting_module::instantiate_scene_scripts(scenes::scene& target) -> void {
  auto query = target.query<scenes::script_component>();

  for (auto&& [entity, list] : query.each()) {
    auto node = target.node_of(entity);

    for (const auto& entry : list.scripts) {
      instantiate(node, entry.class_name);
    }
  }
}

auto scripting_module::attach_script(scenes::node& node, std::string_view class_name) -> void {
  auto& scripts = node.get_or_add_component<scenes::script_component>();

  const auto already_attached = std::ranges::any_of(scripts.scripts, [&](const auto& entry) {
    return entry.class_name == class_name;
  });

  if (already_attached) {
    return;
  }

  scripts.scripts.push_back(scenes::script_entry{.class_name = std::string{class_name}});

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (scenes_module.is_simulating()) {
    instantiate(node, class_name);
  }
}

auto scripting_module::detach_script(scenes::node& node, std::string_view class_name) -> void {
  if (!node.has_component<scenes::script_component>()) {
    return;
  }

  auto& persisted = node.get_component<scenes::script_component>();

  std::erase_if(persisted.scripts, [&](const auto& entry) {
    return entry.class_name == class_name;
  });

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (scenes_module.is_simulating() && node.has_component<scripting::scripts>()) {
    auto& runtime_scripts = node.get_component<scripting::scripts>();

    std::erase_if(runtime_scripts.instances, [&](auto& instance) {
      if (instance.get_type().get_full_name() == class_name) {
        instance.invoke("OnDestroy");
        return true;
      }

      return false;
    });
  }
}

auto scripting_module::_apply_field_overrides(managed::object& instance, const scenes::script_entry& entry) -> void {
  for (const auto& field : entry.field_overrides) {
    switch (field.type) {
      case scenes::script_field_type::float32: instance.set_field_value(field.name, field.float_value); break;
      case scenes::script_field_type::int32:   instance.set_field_value(field.name, field.int_value); break;
      case scenes::script_field_type::boolean: instance.set_field_value(field.name, field.bool_value); break;
      case scenes::script_field_type::string:  instance.set_field_value(field.name, field.string_value); break;
    }
  }
}

auto scripting_module::run_on_destroy(scenes::scene& target) -> void {
  auto scripts_query = target.query<scripting::scripts>();

  for (auto&& [node, scripts] : scripts_query.each()) {
    for (auto& instance : scripts.instances) {
      instance.invoke("OnDestroy");
    }
  }
}

auto scripting_module::recompile_scripts() -> void {
  _load_game_assembly();
}

auto scripting_module::_dotnet_directory() const -> std::filesystem::path {
  // Sibling of the running executable's directory — both bin/ and dotnet/ land directly under
  // the build root (see SBX_DOTNET_OUT_DIR/RUNTIME_OUTPUT_DIRECTORY in the root CMakeLists.txt),
  // regardless of cwd or Debug/Release config.
  return sbx::filesystem::executable_directory() / ".." / "dotnet";
}

auto scripting_module::_load_game_assembly() -> void {
  const auto core_assembly_path = _dotnet_directory() / "Sbx.Core.dll";

  _script_compiler.compile_if_stale(_runtime, core_assembly_path);

  if (!_script_compiler.last_compile_succeeded()) {
    // Never discard a working game assembly (if any) out from under live script instances just
    // because a *new* recompile attempt failed — keep whatever was previously loaded.
    return;
  }

  if (_has_game_assembly) {
    _runtime.unload_assembly_load_context(_game_context);
    _has_game_assembly = false;

    // unload_assembly_load_context() clears the process-wide managed::detail::type_cache (see its
    // doc comment) — including _core_assembly's cached types, even though _context/_core_assembly
    // (Sbx.Core.dll) was never unloaded. Without this, every _core_assembly.get_type(name)/
    // get_types() call after the first recompile would come back empty/not-found (e.g. the
    // Properties panel's "Add Script" picker resolving "Sbx.Core.Behavior" to look up subclasses).
    _core_assembly.reload_types();
  }

  _game_context = _runtime.create_assembly_load_context("GameScripts");

  if (std::filesystem::exists(_script_compiler.output_path())) {
    _game_assembly = _game_context.load_assembly(_script_compiler.output_path().string());
    _has_game_assembly = true;
  }
}

auto scripting_module::_exception_callback(std::string_view message) -> void {
  utility::logger<"scripting">::error("Script runtime error: {}", message);

  throw script_runtime_error{std::string{message}};
}

} // namespace sbx::scripting
