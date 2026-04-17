// SPDX-License-Identifier: MIT
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
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
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
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
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
  auto& graph = scene.graph();

  auto& tag = graph.get_component<scenes::tag>(static_cast<scenes::node>(node));

  return managed::string::create(tag.data());
}

auto interop::tag_set_tag(std::uint32_t node, managed::string tag) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  graph.get_component<scenes::tag>(static_cast<scenes::node>(node)) = std::string{tag};
}

auto interop::transform_get_position(std::uint32_t node, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get position of invalid node");

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *position = transform.position();
}

auto interop::transform_set_position(std::uint32_t node, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  if (!position) {
    auto& tag = graph.get_component<scenes::tag>(static_cast<scenes::node>(node));

    utility::logger<"scripting">::error("Attempting to set null position of node '{}'", tag);

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  transform.set_position(*position);
}

auto interop::transform_get_world_position(std::uint32_t node, math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get position of invalid node");

    return;
  }

  *position = graph.world_position(static_cast<scenes::node>(node));
}

auto interop::transform_get_rotation(std::uint32_t node, math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get rotation of invalid node");

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *rotation = transform.rotation();
}

auto interop::transform_set_rotation(std::uint32_t node, math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set rotation of invalid node");

    return;
  }

  if (!rotation) {
    auto& tag = graph.get_component<scenes::tag>(static_cast<scenes::node>(node));

    utility::logger<"scripting">::error("Attempting to set null rotation of node '{}'", tag);

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  transform.set_rotation(*rotation);
}

auto interop::transform_get_right(std::uint32_t node, math::vector3* right) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *right = transform.right();
}

auto interop::transform_get_forward(std::uint32_t node, math::vector3* forward) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *forward = transform.forward();
}

auto interop::transform_get_up(std::uint32_t node, math::vector3* up) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  *up = transform.up();
}

auto interop::transform_look_at(std::uint32_t node, math::vector3* target) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to set position of invalid node");

    return;
  }

  if (!target) {
    auto& tag = graph.get_component<scenes::tag>(static_cast<scenes::node>(node));

    utility::logger<"scripting">::error("Attempting to call LookAt with null target of node '{}'", tag);

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(node));

  transform.look_at(*target);
}

auto interop::character_controller_get_height(std::uint32_t node, std::float_t* height) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get height of invalid node");

    return;
  }

  auto& character_controller = graph.get_component<physics::character_controller>(static_cast<scenes::node>(node));

  *height = character_controller.height;
}

auto interop::character_controller_get_radius(std::uint32_t node, std::float_t* radius) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get radius of invalid node");

    return;
  }

  auto& character_controller = graph.get_component<physics::character_controller>(static_cast<scenes::node>(node));

  *radius = character_controller.radius;
}

auto interop::character_controller_get_slope_limit(std::uint32_t node, std::float_t* slope_limit) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get slope_limit of invalid node");

    return;
  }

  auto& character_controller = graph.get_component<physics::character_controller>(static_cast<scenes::node>(node));

  *slope_limit = character_controller.slope_limit;
}

auto interop::character_controller_get_step_offset(std::uint32_t node, std::float_t* step_offset) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get step_offset of invalid node");

    return;
  }

  auto& character_controller = graph.get_component<physics::character_controller>(static_cast<scenes::node>(node));

  *step_offset = character_controller.step_offset;
}

auto interop::character_controller_get_is_grounded(std::uint32_t node) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get step_offset of invalid node");

    return false;
  }

  auto& character_controller = graph.get_component<physics::character_controller>(static_cast<scenes::node>(node));

  return character_controller.is_grounded;
}

auto interop::character_controller_get_flags(std::uint32_t node, std::uint8_t* flags) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get step_offset of invalid node");

    return;
  }

  auto& character_controller = graph.get_component<physics::character_controller>(static_cast<scenes::node>(node));

  *flags = utility::to_underlying(character_controller.flags);
}

auto interop::character_controller_move(std::uint32_t node, math::vector3* displacement) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(static_cast<scenes::node>(node))) {
    utility::logger<"scripting">::error("Attempting to get position of invalid node");

    return;
  }

  auto& character_controller = graph.get_component<physics::character_controller>(static_cast<scenes::node>(node));

  character_controller.displacement += *displacement;
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

auto interop::camera_screen_point_to_ray(math::ray* ray, math::vector2* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  if (!ray) {
    auto& tag = graph.get_component<scenes::tag>(static_cast<scenes::node>(camera_node));

    utility::logger<"scripting">::error("Attempting to call ScreenPointToRay with null ray of node '{}'", tag);

    return;
  }

  *ray = environment.screen_point_to_ray(*position);
}

auto interop::camera_get_position(math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(camera_node));

  *position = transform.position();
}

auto interop::camera_set_position(math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  if (!position) {
    auto& tag = graph.get_component<scenes::tag>(static_cast<scenes::node>(camera_node));

    utility::logger<"scripting">::error("Attempting to set null position of camera node '{}'", tag);

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(camera_node));

  transform.set_position(*position);
}

auto interop::camera_get_rotation(math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(camera_node));

  *rotation = transform.rotation();
}

auto interop::camera_set_rotation(math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  if (!rotation) {
    auto& tag = graph.get_component<scenes::tag>(static_cast<scenes::node>(camera_node));

    utility::logger<"scripting">::error("Attempting to set null rotation of camera node '{}'", tag);

    return;
  }

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(camera_node));

  transform.set_rotation(*rotation);
}

auto interop::camera_get_forward(math::vector3* forward) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(camera_node));

  *forward = transform.forward();
}

auto interop::camera_get_right(math::vector3* right) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(camera_node));

  *right = transform.right();
}

auto interop::camera_get_up(math::vector3* up) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();

  auto& transform = graph.get_component<scenes::transform>(static_cast<scenes::node>(camera_node));

  *up = transform.up();
}

auto interop::camera_get_viewport(math::vector2* viewport) -> void {
  if (!viewport) {
    utility::logger<"scripting">::error("Attempting to get null viewport of camera");
    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();

  const auto size = environment.render_target_size();

  *viewport = math::vector2{static_cast<std::float_t>(size.x()), static_cast<std::float_t>(size.y())};
}

auto interop::time_delta_time(std::float_t* delta_time) -> void {
  if (!delta_time) {
    utility::logger<"scripting">::error("Attempting to set null delta_time");

    return;
  }

  *delta_time = core::engine::delta_time().value();
}

} // namespace sbx::scripting
