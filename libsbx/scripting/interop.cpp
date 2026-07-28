// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

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

auto interop::behavior_add_component(std::uint64_t uuid, managed::reflection_type component_type) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call add_component on invalid node");

    return;
  }

  auto& type = static_cast<managed::type&>(component_type);

  if (!type) {
    return;
  }

  if (auto entry = _add_component_functions.find(type.get_type_id()); entry != _add_component_functions.end()) {
    auto function = entry->second;

    std::invoke(function, node);
  }
}

auto interop::behavior_has_component(std::uint64_t uuid, managed::reflection_type component_type) -> bool {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call has_component on invalid node");

    return false;
  }

  auto& type = static_cast<managed::type&>(component_type);

  if (!type) {
    return false;
  }

  if (auto entry = _has_component_functions.find(type.get_type_id()); entry != _has_component_functions.end()) {
    auto function = entry->second;

    return std::invoke(function, node);
  }

  return false;
}

// auto interop::behavior_remove_component(std::uint64_t uuid, managed::reflection_type component_type) -> bool {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to call remove_component on invalid node");

//     return false;
//   }

//   auto& type = static_cast<managed::type&>(component_type);

//   if (!type) {
//     return false;
//   }

//   if (auto entry = _remove_component_functions.find(type.get_type_id()); entry != _remove_component_functions.end()) {
//     auto function = entry->second;

//     return std::invoke(function, node);
//   }

//   return false;
// }

auto interop::tag_get_tag(std::uint64_t uuid) -> managed::string {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get tag of invalid node");

    return managed::string::create("");
  }

  return managed::string::create(node.name().c_str());
}

auto interop::tag_set_tag(std::uint64_t uuid, managed::string tag) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set tag of invalid node");

    return;
  }

  node.name() = scenes::tag{std::string{tag}};
}

auto interop::transform_get_position(std::uint64_t uuid, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get position of invalid node");

    return;
  }

  *position = node.transform().position;
}

auto interop::transform_set_position(std::uint64_t uuid, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  if (!position) {
    utility::logger<"scripting">::error("Attempting to set null position of node '{}'", node.name());

    return;
  }

  node.transform().position = *position;
}

auto interop::transform_get_world_position(std::uint64_t uuid, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get world position of invalid node");

    return;
  }

  const auto& matrix = node.world_matrix();

  *position = math::vector3{matrix[3][0], matrix[3][1], matrix[3][2]};
}

auto interop::transform_get_rotation(std::uint64_t uuid, math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get rotation of invalid node");

    return;
  }

  *rotation = node.transform().rotation;
}

auto interop::transform_set_rotation(std::uint64_t uuid, math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set rotation of invalid node");

    return;
  }

  if (!rotation) {
    utility::logger<"scripting">::error("Attempting to set null rotation of node '{}'", node.name());

    return;
  }

  node.transform().rotation = *rotation;
}

auto interop::transform_get_right(std::uint64_t uuid, math::vector3* right) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get right of invalid node");

    return;
  }

  *right = node.transform().right();
}

auto interop::transform_get_forward(std::uint64_t uuid, math::vector3* forward) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get forward of invalid node");

    return;
  }

  *forward = node.transform().forward();
}

auto interop::transform_get_up(std::uint64_t uuid, math::vector3* up) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get up of invalid node");

    return;
  }

  *up = node.transform().up();
}

// auto interop::transform_look_at(std::uint64_t uuid, math::vector3* target) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to call look_at on invalid node");

//     return;
//   }

//   if (!target) {
//     utility::logger<"scripting">::error("Attempting to call look_at with null target of node '{}'", node.name());

//     return;
//   }

//   node.transform().look_at(*target);
// }

// auto interop::character_controller_get_height(std::uint64_t uuid, std::float_t* height) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get height of invalid node");

//     return;
//   }

//   auto& character_controller = node.get_component<physics::character_controller>();

//   *height = character_controller.height;
// }

// auto interop::character_controller_get_radius(std::uint64_t uuid, std::float_t* radius) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get radius of invalid node");

//     return;
//   }

//   auto& character_controller = node.get_component<physics::character_controller>();

//   *radius = character_controller.radius;
// }

// auto interop::character_controller_get_slope_limit(std::uint64_t uuid, std::float_t* slope_limit) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get slope_limit of invalid node");

//     return;
//   }

//   auto& character_controller = node.get_component<physics::character_controller>();

//   *slope_limit = character_controller.slope_limit;
// }

// auto interop::character_controller_get_step_offset(std::uint64_t uuid, std::float_t* step_offset) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get step_offset of invalid node");

//     return;
//   }

//   auto& character_controller = node.get_component<physics::character_controller>();

//   *step_offset = character_controller.step_offset;
// }

// auto interop::character_controller_get_is_grounded(std::uint64_t uuid) -> managed::bool32 {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get is_grounded of invalid node");

//     return false;
//   }

//   auto& character_controller = node.get_component<physics::character_controller>();

//   return character_controller.is_grounded;
// }

// auto interop::character_controller_get_flags(std::uint64_t uuid, std::uint8_t* flags) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get flags of invalid node");

//     return;
//   }

//   auto& character_controller = node.get_component<physics::character_controller>();

//   *flags = reflection::to_underlying(character_controller.flags);
// }

// auto interop::character_controller_move(std::uint64_t uuid, math::vector3* displacement) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.find(math::uuid::from_value(uuid));

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to move invalid node");

//     return;
//   }

//   auto& character_controller = node.get_component<physics::character_controller>();

//   character_controller.displacement += *displacement;
// }

auto interop::input_is_key_pressed(platform::key key) -> managed::bool32 { 
  return platform::input::is_key_pressed(key); 
}

auto interop::input_is_key_down(platform::key key) -> managed::bool32 { 
  return platform::input::is_key_down(key); 
}

auto interop::input_is_key_released(platform::key key) -> managed::bool32 { 
  return platform::input::is_key_released(key); 
}

auto interop::input_is_mouse_button_pressed(platform::mouse_button mouse_button) -> managed::bool32 { 
  return platform::input::is_mouse_button_pressed(mouse_button); 
}

auto interop::input_is_mouse_button_down(platform::mouse_button mouse_button) -> managed::bool32 { 
  return platform::input::is_mouse_button_down(mouse_button); 
}

auto interop::input_is_mouse_button_released(platform::mouse_button mouse_button) -> managed::bool32 { 
  return platform::input::is_mouse_button_released(mouse_button); 
}

auto interop::input_mouse_position(math::vector2* position) -> void {
  *position = platform::input::mouse_position();
}

auto interop::input_scroll_delta(math::vector2* scroll_delta) -> void {
  *scroll_delta = platform::input::scroll_delta();
}

// auto interop::camera_screen_point_to_ray(math::ray* ray, math::vector2* position) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to call screen_point_to_ray with no active camera");

//     return;
//   }

//   if (!ray) {
//     utility::logger<"scripting">::error("Attempting to call screen_point_to_ray with null ray of node '{}'", node.name());

//     return;
//   }

//   // NOTE: screen_point_to_ray lived on environment, which is gone. Reimplement against camera component + viewport.
// }

// auto interop::camera_get_position(math::vector3* position) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get position with no active camera");

//     return;
//   }

//   *position = node.transform().position;
// }

// auto interop::camera_set_position(math::vector3* position) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to set position with no active camera");

//     return;
//   }

//   if (!position) {
//     utility::logger<"scripting">::error("Attempting to set null position of camera node '{}'", node.name());

//     return;
//   }

//   node.transform().position = *position;
// }

// auto interop::camera_get_rotation(math::quaternion* rotation) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get rotation with no active camera");

//     return;
//   }

//   *rotation = node.transform().rotation;
// }

// auto interop::camera_set_rotation(math::quaternion* rotation) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to set rotation with no active camera");

//     return;
//   }

//   if (!rotation) {
//     utility::logger<"scripting">::error("Attempting to set null rotation of camera node '{}'", node.name());

//     return;
//   }

//   node.transform().rotation = *rotation;
// }

// auto interop::camera_get_forward(math::vector3* forward) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get forward with no active camera");

//     return;
//   }

//   *forward = node.transform().forward();
// }

// auto interop::camera_get_right(math::vector3* right) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get right with no active camera");

//     return;
//   }

//   *right = node.transform().right();
// }

// auto interop::camera_get_up(math::vector3* up) -> void {
//   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

//   auto& scene = scenes_module.active_scene();

//   auto node = scene.active_camera();

//   if (!node.is_valid()) {
//     utility::logger<"scripting">::error("Attempting to get up with no active camera");

//     return;
//   }

//   *up = node.transform().up();
// }

// auto interop::camera_get_viewport(math::vector2* viewport) -> void {
//   if (!viewport) {
//     utility::logger<"scripting">::error("Attempting to get null viewport of camera");

//     return;
//   }

//   // NOTE: render_target_size lived on environment, which is gone. Source viewport from the render target / swapchain.
// }

auto interop::time_delta_time(std::float_t* delta_time) -> void {
  if (!delta_time) {
    utility::logger<"scripting">::error("Attempting to set null delta_time");

    return;
  }

  *delta_time = core::engine::delta_time().value();
}

} // namespace sbx::scripting
