#include <libsbx/scripting/interop.hpp>

#include <libsbx/utility/logger.hpp>

namespace sbx::scripting {

auto interop::log_log_message(log_level level, managed::string message) -> void {
  switch (level) {
    case log_level::trace: {
      utility::logger<"scripting">::trace(std::string{message});
      break;
    }
    case log_level::debug: {
      utility::logger<"scripting">::debug(std::string{message});
      break;
    }
    case log_level::info: {
      utility::logger<"scripting">::info(std::string{message});
      break;
    }
    case log_level::warn: {
      utility::logger<"scripting">::warn(std::string{message});
      break;
    }
    case log_level::error: {
      utility::logger<"scripting">::error(std::string{message});
      break;
    }
    case log_level::critical: {
      utility::logger<"scripting">::critical(std::string{message});
      break;
    }
  }
}

auto interop::behavior_add_component(std::uint32_t node, managed::reflection_type component_type) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to call add_component on invalid node");

    return;
  };

  auto& type = static_cast<managed::type&>(component_type);

  if (!type) {
    return;
  }

  if (auto entry = _add_component_functions.find(type.get_type_id()); entry != _add_component_functions.end()) {
    auto function = entry->second;

    std::invoke(function, static_cast<scenes::node>(node));
  }
}

auto interop::behavior_has_component(std::uint32_t node, managed::reflection_type component_type) -> bool {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to call has_component on invalid node");

    return false;
  };

  auto& type = static_cast<managed::type&>(component_type);

  if (!type) {
    return false;
  }

  if (auto entry = _has_component_functions.find(type.get_type_id()); entry != _has_component_functions.end()) {
    auto function = entry->second;

    return std::invoke(function, static_cast<scenes::node>(node));
  }

  return false;
}

// auto interop::behavior_remove_component(std::uint32_t node, managed::reflection_type componentType) -> bool {

// }

auto interop::tag_get_tag(std::uint32_t node) -> managed::string {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  auto& tag = scene.get_component<scenes::tag>(static_cast<scenes::node>(node));

  return managed::string::create(tag.data());
}

auto interop::tag_set_tag(std::uint32_t node, managed::string tag) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  scene.get_component<scenes::tag>(static_cast<scenes::node>(node)) = std::string{tag};
}

auto interop::transform_get_position(std::uint32_t node, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  auto& transform = scene.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *position = transform.position();
}

auto interop::transform_set_position(std::uint32_t node, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  if (!position) {
    auto& tag = scene.get_component<scenes::tag>(static_cast<scenes::node>(node));

    utility::logger<"scripting">::error("Attempting to set null position of node '{}'", tag);

    return;
  }

  auto& transform = scene.get_component<scenes::transform>(static_cast<scenes::node>(node));

  transform.set_position(*position);
}

auto interop::transform_get_right(std::uint32_t node, math::vector3* right) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  auto& transform = scene.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *right = transform.right();
}

auto interop::transform_get_forward(std::uint32_t node, math::vector3* forward) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  auto& transform = scene.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *forward = transform.forward();
}

auto interop::transform_get_up(std::uint32_t node, math::vector3* up) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  auto& transform = scene.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *up = transform.up();
}

auto interop::transform_look_at(std::uint32_t node, math::vector3* target) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (!scene.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  if (!target) {
    auto& tag = scene.get_component<scenes::tag>(static_cast<scenes::node>(node));

    utility::logger<"scripting">::error("Attempting to call LookAt with null target of node '{}'", tag);

    return;
  }

  auto& transform = scene.get_component<scenes::transform>(static_cast<scenes::node>(node));

  transform.look_at(*target);
}

auto interop::input_is_key_pressed(devices::key key) -> managed::bool32 { 
  return devices::input::is_key_pressed(key); 
}

auto interop::input_is_key_down(devices::key key) -> managed::bool32 { 
  return devices::input::is_key_down(key); 
}

auto interop::input_is_key_released(devices::key key) -> managed::bool32 { 
  return devices::input::is_key_released(key); 
}

auto interop::input_is_mouse_button_pressed(devices::mouse_button mouse_button) -> managed::bool32 { 
  return devices::input::is_mouse_button_pressed(mouse_button); 
}

auto interop::input_is_mouse_button_down(devices::mouse_button mouse_button) -> managed::bool32 { 
  return devices::input::is_mouse_button_down(mouse_button); 
}

auto interop::input_is_mouse_button_released(devices::mouse_button mouse_button) -> managed::bool32 { 
  return devices::input::is_mouse_button_released(mouse_button); 
}

auto interop::input_mouse_position(math::vector2* position) -> void {
  *position = devices::input::mouse_position();
}

auto interop::input_scroll_delta(math::vector2* scroll_delta) -> void {
  *scroll_delta = devices::input::scroll_delta();
}

} // namespace sbx::scripting
