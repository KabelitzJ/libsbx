// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>
#include <libsbx/physics/nav/nav_settings.hpp>
#include <libsbx/physics/nav/navmesh.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/scripting/scripting_module.hpp>

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

auto interop::scripting_attach_script(std::uint64_t uuid, managed::string class_name) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to attach script to invalid node");
    return;
  }

  auto& scripting_module = core::engine::get_module<scripting::scripting_module>();

  scripting_module.attach_script(node, std::string{class_name});
}

auto interop::scripting_get_instance(std::uint64_t uuid, managed::string class_name) -> managed::object {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  utility::logger<"scripting">::info("scripting_get_instance: {} {}", uuid, std::string{class_name});

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get script instance of invalid node");
    return managed::object{};
  }

  auto scripts = node.try_get_component<scripting::scripts>();

  if (!scripts) {
    return managed::object{};
  }

  const auto& instances = scripts->instances;

  const auto target_name = std::string{class_name};

  for (auto& instance : instances) {
    utility::logger<"scripting">::info("scripting_get_instance: {}", std::string{instance.get_type().get_full_name()});
  }

  auto entry = std::ranges::find_if(instances, [&target_name](const auto& instance) {
    return instance.get_type().get_full_name() == target_name;
  });

  utility::logger<"scripting">::info("scripting_get_instance: {}", entry != instances.end());

  return (entry != instances.end()) ? *entry : managed::object{};
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

auto interop::behavior_has_component(std::uint64_t uuid, managed::reflection_type component_type) -> managed::bool32 {
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

auto interop::behavior_remove_component(std::uint64_t uuid, managed::reflection_type component_type) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call remove_component on invalid node");

    return false;
  }

  auto& type = static_cast<managed::type&>(component_type);

  if (!type) {
    return false;
  }

  if (auto entry = _remove_component_functions.find(type.get_type_id()); entry != _remove_component_functions.end()) {
    auto function = entry->second;

    return std::invoke(function, node);
  }

  return false;
}

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

auto interop::transform_get_scale(std::uint64_t uuid, math::vector3* scale) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get scale of invalid node");

    return;
  }

  *scale = node.transform().scale;
}

auto interop::transform_set_scale(std::uint64_t uuid, math::vector3* scale) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set scale of invalid node");

    return;
  }

  if (!scale) {
    utility::logger<"scripting">::error("Attempting to set null scale of node '{}'", node.name());

    return;
  }

  node.transform().scale = *scale;
}

auto interop::transform_look_at(std::uint64_t uuid, math::vector3* target) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call look_at on invalid node");

    return;
  }

  if (!target) {
    utility::logger<"scripting">::error("Attempting to call look_at with null target of node '{}'", node.name());

    return;
  }

  auto& transform = node.transform();
  const auto direction = *target - transform.position;

  if (direction.length_squared() <= math::epsilonf) {
    return; // target coincides with the node's position, so there is no facing direction
  }

  transform.rotation = math::quaternion::look_at(math::vector3::normalized(direction));
}

auto interop::animator_get_playing(std::uint64_t uuid) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get playing of invalid node");

    return false;
  }

  return node.get_component<scenes::animator>().playing;
}

auto interop::animator_set_playing(std::uint64_t uuid, bool value) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set playing of invalid node");

    return;
  }

  node.get_component<scenes::animator>().playing = value;
}

auto interop::animator_get_current_state_name(std::uint64_t uuid) -> managed::string {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get current state name of invalid node");

    return managed::string::create("");
  }

  const auto& animator = node.get_component<scenes::animator>();

  if (!animator.graph.is_valid()) {
    return managed::string::create("");
  }

  const auto& states = animator.graph->states();
  const auto entry = std::ranges::find(states, animator.current_state_id, &assets::animation_state::id);

  return managed::string::create(entry != states.end() ? entry->name.c_str() : "");
}

auto interop::animator_set_float(std::uint64_t uuid, managed::string name, std::float_t value) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set animator float of invalid node");

    return;
  }

  node.get_component<scenes::animator>().set_float(std::string{name}, value);
}

auto interop::animator_set_bool(std::uint64_t uuid, managed::string name, bool value) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set animator bool of invalid node");

    return;
  }

  node.get_component<scenes::animator>().set_bool(std::string{name}, value);
}

auto interop::animator_set_int(std::uint64_t uuid, managed::string name, std::int32_t value) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set animator int of invalid node");

    return;
  }

  node.get_component<scenes::animator>().set_int(std::string{name}, value);
}

auto interop::animator_set_trigger(std::uint64_t uuid, managed::string name) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set animator trigger of invalid node");

    return;
  }

  node.get_component<scenes::animator>().set_trigger(std::string{name});
}

auto interop::animator_get_float(std::uint64_t uuid, managed::string name) -> std::float_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get animator float of invalid node");

    return 0.0f;
  }

  auto* value = node.get_component<scenes::animator>().find_parameter(std::string{name});
  const auto* found = value ? std::get_if<std::float_t>(value) : nullptr;

  return found ? *found : 0.0f;
}

auto interop::animator_get_bool(std::uint64_t uuid, managed::string name) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get animator bool of invalid node");

    return false;
  }

  auto* value = node.get_component<scenes::animator>().find_parameter(std::string{name});
  const auto* found = value ? std::get_if<bool>(value) : nullptr;

  return found ? *found : false;
}

auto interop::animator_get_int(std::uint64_t uuid, managed::string name) -> std::int32_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get animator int of invalid node");

    return 0;
  }

  auto* value = node.get_component<scenes::animator>().find_parameter(std::string{name});
  const auto* found = value ? std::get_if<std::int32_t>(value) : nullptr;

  return found ? *found : 0;
}

auto interop::rigidbody_get_linear_velocity(std::uint64_t uuid, math::vector3* velocity) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get linear velocity of invalid node");

    return;
  }

  *velocity = node.get_component<physics::rigidbody>().linear_velocity;
}

auto interop::rigidbody_set_linear_velocity(std::uint64_t uuid, math::vector3* velocity) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !velocity) {
    utility::logger<"scripting">::error("Attempting to set linear velocity of invalid node");

    return;
  }

  node.get_component<physics::rigidbody>().linear_velocity = *velocity;
}

auto interop::rigidbody_get_angular_velocity(std::uint64_t uuid, math::vector3* velocity) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get angular velocity of invalid node");

    return;
  }

  *velocity = node.get_component<physics::rigidbody>().angular_velocity;
}

auto interop::rigidbody_set_angular_velocity(std::uint64_t uuid, math::vector3* velocity) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !velocity) {
    utility::logger<"scripting">::error("Attempting to set angular velocity of invalid node");

    return;
  }

  node.get_component<physics::rigidbody>().angular_velocity = *velocity;
}

auto interop::rigidbody_get_mass(std::uint64_t uuid, std::float_t* mass) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !mass) {
    utility::logger<"scripting">::error("Attempting to get mass of invalid node");

    return;
  }

  const auto inverse_mass = node.get_component<physics::rigidbody>().inverse_mass;

  *mass = (inverse_mass > 0.0f) ? (1.0f / inverse_mass) : 0.0f; // 0 means immovable
}

auto interop::rigidbody_set_mass(std::uint64_t uuid, std::float_t mass) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set mass of invalid node");

    return;
  }

  node.get_component<physics::rigidbody>().inverse_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
}

auto interop::rigidbody_get_gravity_scale(std::uint64_t uuid, std::float_t* scale) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !scale) {
    utility::logger<"scripting">::error("Attempting to get gravity scale of invalid node");

    return;
  }

  *scale = node.get_component<physics::rigidbody>().gravity_scale;
}

auto interop::rigidbody_set_gravity_scale(std::uint64_t uuid, std::float_t scale) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set gravity scale of invalid node");

    return;
  }

  node.get_component<physics::rigidbody>().gravity_scale = scale;
}

auto interop::rigidbody_add_force(std::uint64_t uuid, math::vector3* force) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !force) {
    utility::logger<"scripting">::error("Attempting to add force to invalid node");

    return;
  }

  node.get_component<physics::rigidbody>().force_accumulator += *force;
}

auto interop::rigidbody_add_torque(std::uint64_t uuid, math::vector3* torque) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !torque) {
    utility::logger<"scripting">::error("Attempting to add torque to invalid node");

    return;
  }

  node.get_component<physics::rigidbody>().torque_accumulator += *torque;
}

auto interop::node_find_by_name(managed::string name) -> std::uint64_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(utility::hashed_string{std::string{name}});

  return node.is_valid() ? node.id().value() : 0u;
}

auto interop::node_create(managed::string name) -> std::uint64_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.create_node(utility::hashed_string{std::string{name}});

  return node.id().value();
}

auto interop::node_instantiate_prefab(managed::string path, std::uint64_t parent_uuid) -> std::uint64_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto prefab = assets_module.load_prefab(std::filesystem::path{std::string{path}});

  if (!prefab.is_valid()) {
    utility::logger<"scripting">::error("Node.Instantiate('{}') resolved to an invalid prefab", std::string{path});
    return 0u;
  }

  auto instance = scenes::scene_serializer::instantiate_prefab(scene, prefab);

  if (!instance.is_valid()) {
    utility::logger<"scripting">::error("Node.Instantiate('{}') failed to instantiate", std::string{path});
    return 0u;
  }

  if (parent_uuid != 0u) {
    if (auto parent = scene.find(math::uuid::from_value(parent_uuid)); parent.is_valid()) {
      instance.set_parent(parent);
    } else {
      utility::logger<"scripting">::error("Node.Instantiate('{}') given invalid parent", std::string{path});
    }
  }

  auto& scripting_module = core::engine::get_module<scripting::scripting_module>();
  scripting_module.instantiate_subtree_scripts(scene, instance);

  return instance.id().value();
}

auto interop::node_destroy(std::uint64_t uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to destroy invalid node");

    return;
  }

  // destroy_node removes the whole subtree, so every descendant's scripts need OnDestroy too or their GCHandles leak.
  const auto destroy_scripts = [](this const auto& self, scenes::scene& scene, scenes::node current) -> void {
    if (auto* scripts = current.try_get_component<scripting::scripts>().get()) {
      for (auto& instance : scripts->instances) {
        instance.invoke("OnDestroy");
        instance.destroy();
      }
    }

    for (const auto child : current.get_component<scenes::relationship>().children) {
      self(scene, scene.node_of(child));
    }
  };

  destroy_scripts(scene, node);

  scene.destroy_node(node);
}

auto interop::node_set_parent(std::uint64_t uuid, std::uint64_t parent_uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set parent of invalid node");

    return;
  }

  // parent_uuid 0 means the scene root (Node.SetParent(null)).
  auto parent = (parent_uuid == 0u) ? scene.root() : scene.find(math::uuid::from_value(parent_uuid));

  if (!parent.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set parent to invalid node");

    return;
  }

  node.set_parent(parent);
}

auto interop::node_set_active(std::uint64_t uuid, bool active) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set active of invalid node");

    return;
  }

  node.set_active(active);
}

auto interop::node_get_is_active(std::uint64_t uuid) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get active of invalid node");

    return false;
  }

  return node.is_active();
}

auto interop::scene_load(managed::string path) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto handle = assets_module.load_scene(std::filesystem::path{std::string{path}});

  if (!handle.is_valid()) {
    utility::logger<"scripting">::error("SceneManager.Load('{}') resolved to an invalid scene", std::string{path});

    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  scenes::scene_serializer::load(scenes_module.active_scene(), handle->snapshot());
}

auto interop::scene_save(managed::string path) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& scene = scenes_module.active_scene();

  auto handle = assets_module.create_scene(scenes::scene_serializer::build(scene), scene.name());
  assets_module.save_scene(handle, std::filesystem::path{std::string{path}});
}

auto interop::scene_new() -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  scenes_module.active_scene() = scenes::scene{};

  auto& scene = scenes_module.active_scene();

  auto camera = scene.create_node("Camera");
  camera.add_component<scenes::camera>();
  scene.set_active_camera(camera);
}

auto interop::particle_effect_load(std::uint64_t uuid, managed::string path) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to load particle effect on invalid node");

    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  // particles_module re-pairs runtime emitters against the assigned effect every step, so nothing needs resetting here.
  auto& effect = node.get_component<scenes::particle_effect>().effect;
  effect = assets_module.load_particle_effect(std::filesystem::path{std::string{path}});

  if (!effect.is_valid()) {
    utility::logger<"scripting">::error("ParticleEffect.Load('{}') on node '{}' resolved to an invalid handle", std::string{path}, node.name());
  }
}

auto interop::particle_effect_play(std::uint64_t uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to play particle effect of invalid node");

    return;
  }

  node.get_component<scenes::particle_effect>().playback = scenes::particle_playback_state::playing;
}

auto interop::particle_effect_pause(std::uint64_t uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to pause particle effect of invalid node");

    return;
  }

  node.get_component<scenes::particle_effect>().playback = scenes::particle_playback_state::paused;
}

auto interop::particle_effect_stop(std::uint64_t uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to stop particle effect of invalid node");

    return;
  }

  node.get_component<scenes::particle_effect>().playback = scenes::particle_playback_state::stopped;
}

auto interop::particle_effect_get_loop(std::uint64_t uuid) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get loop of invalid node");

    return false;
  }

  return node.get_component<scenes::particle_effect>().loop;
}

auto interop::particle_effect_set_loop(std::uint64_t uuid, bool value) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set loop of invalid node");

    return;
  }

  node.get_component<scenes::particle_effect>().loop = value;
}

auto interop::particle_effect_get_is_playing(std::uint64_t uuid) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get is_playing of invalid node");

    return false;
  }

  return node.get_component<scenes::particle_effect>().playback == scenes::particle_playback_state::playing;
}

auto interop::project_get_assets_directory() -> managed::string {
  return managed::string::create(core::engine::project().assets_directory().string().c_str());
}

} // namespace sbx::scripting
