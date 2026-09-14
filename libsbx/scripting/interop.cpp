// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <algorithm>
#include <filesystem>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>
#include <libsbx/physics/nav/nav_settings.hpp>
#include <libsbx/physics/nav/navmesh.hpp>

#include <libsbx/terrain/terrain_module.hpp>

#include <libsbx/canvas/canvas_module.hpp>
#include <libsbx/canvas/components.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

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

auto interop::behavior_remove_component(std::uint64_t uuid, managed::reflection_type component_type) -> bool {
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
    return; // target coincides with the node's own position -- no well-defined facing direction
  }

  transform.rotation = math::quaternion::look_at(math::vector3::normalized(direction));
}

auto interop::animator_get_playing(std::uint64_t uuid) -> bool {
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

auto interop::animator_get_bool(std::uint64_t uuid, managed::string name) -> bool {
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

  *mass = (inverse_mass > 0.0f) ? (1.0f / inverse_mass) : 0.0f; // 0 == infinite/immovable (static or kinematic), matching rigidbody::inverse_mass's own convention
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

  // scene.destroy_node below recurses over the whole subtree, so OnDestroy/destroy() must too --
  // otherwise a descendant's script instance never runs its cleanup and leaks its GCHandle (same
  // walk shape as instantiate_subtree_scripts' construction-side counterpart).
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

  // parent_uuid == 0 means "move to the scene root" (Node.SetParent(null) on the C# side) rather
  // than an actual node -- 0 is never a real node's uuid (see node_find_by_name's convention).
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

auto interop::node_get_is_active(std::uint64_t uuid) -> bool {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get active of invalid node");

    return false;
  }

  return node.is_active();
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

  // particles_module::_simulate_effect already resizes/re-pairs runtime emitters against
  // whatever effect->emitters() the assigned handle has (it does this every step, to also cover
  // the editor swapping the asset out from under an already-playing instance) -- nothing else
  // needs resetting here.
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

auto interop::particle_effect_get_loop(std::uint64_t uuid) -> bool {
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

auto interop::particle_effect_get_is_playing(std::uint64_t uuid) -> bool {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get is_playing of invalid node");

    return false;
  }

  return node.get_component<scenes::particle_effect>().playback == scenes::particle_playback_state::playing;
}

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

auto interop::camera_get_viewport(math::vector2* viewport) -> void {
  if (!viewport) {
    utility::logger<"scripting">::error("Attempting to get null viewport of camera");

    return;
  }

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  const auto extent = scene_renderer_module.target_extent();

  *viewport = math::vector2{static_cast<std::float_t>(extent.x()), static_cast<std::float_t>(extent.y())};
}

/**
 * How far the viewport's top-left corner sits from the window's -- zero in a standalone build,
 * nonzero in the editor where the game view is a docked panel inset within the window (see
 * viewport_window's set_viewport_offset). Input.MousePosition() is raw window space; every
 * UI/canvas coordinate (RectTransform included) is viewport space -- a script positioning UI
 * from the mouse needs to subtract this itself, the same correction camera_screen_point_to_ray/
 * camera_world_to_screen_point already apply internally for their own (world-facing) purposes.
 */
auto interop::camera_get_viewport_offset(math::vector2* offset) -> void {
  if (!offset) {
    utility::logger<"scripting">::error("Attempting to get null viewport offset of camera");

    return;
  }

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();

  *offset = scene_renderer_module.viewport_offset();
}

auto interop::camera_screen_point_to_ray(math::ray* ray, math::vector2* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call screen_point_to_ray with no active camera");

    return;
  }

  if (!ray || !position) {
    utility::logger<"scripting">::error("Attempting to call screen_point_to_ray with null ray/position of node '{}'", node.name());

    return;
  }

  const auto& camera = node.get_component<scenes::camera>();

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  const auto extent = scene_renderer_module.target_extent();
  const auto offset = scene_renderer_module.viewport_offset();

  const auto aspect = (extent.y() > 0u) ? (static_cast<std::float_t>(extent.x()) / static_cast<std::float_t>(extent.y())) : 1.0f;

  const auto view = math::matrix4x4::inverted(node.world_matrix());
  const auto projection = math::matrix4x4::perspective(math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);
  const auto inverse_view_projection = math::matrix4x4::inverted(projection * view);

  const auto ndc_x = (extent.x() > 0u) ? (((position->x() - offset.x()) / static_cast<std::float_t>(extent.x())) * 2.0f - 1.0f) : 0.0f;
  const auto ndc_y = (extent.y() > 0u) ? (((position->y() - offset.y()) / static_cast<std::float_t>(extent.y())) * 2.0f - 1.0f) : 0.0f;

  const auto unproject = [&inverse_view_projection, ndc_x, ndc_y](std::float_t ndc_z) -> math::vector3 {
    const auto point = inverse_view_projection * math::vector4{ndc_x, ndc_y, ndc_z, 1.0f};
    return math::vector3{point.x(), point.y(), point.z()} / point.w();
  };

  const auto near_point = unproject(0.0f);
  const auto far_point = unproject(1.0f);

  *ray = math::ray{near_point, far_point - near_point};
}

auto interop::camera_world_to_screen_point(math::vector3* world_position, math::vector2* out_position) -> bool {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call world_to_screen_point with no active camera");

    return false;
  }

  if (!world_position || !out_position) {
    utility::logger<"scripting">::error("Attempting to call world_to_screen_point with null world/screen position of node '{}'", node.name());

    return false;
  }

  const auto& camera = node.get_component<scenes::camera>();

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  const auto extent = scene_renderer_module.target_extent();
  const auto offset = scene_renderer_module.viewport_offset();

  const auto aspect = (extent.y() > 0u) ? (static_cast<std::float_t>(extent.x()) / static_cast<std::float_t>(extent.y())) : 1.0f;

  const auto view = math::matrix4x4::inverted(node.world_matrix());
  const auto projection = math::matrix4x4::perspective(math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);

  const auto clip = projection * view * math::vector4{*world_position, 1.0f};

  // w <= 0 means world_position sits at or behind the camera's eye plane -- there's no well-defined
  // screen point for it (the same case camera_screen_point_to_ray never has to handle, since it
  // only ever projects outward from the camera).
  if (clip.w() <= 0.0f) {
    return false;
  }

  const auto ndc_x = clip.x() / clip.w();
  const auto ndc_y = clip.y() / clip.w();

  out_position->x() = (ndc_x * 0.5f + 0.5f) * static_cast<std::float_t>(extent.x()) + offset.x();
  out_position->y() = (ndc_y * 0.5f + 0.5f) * static_cast<std::float_t>(extent.y()) + offset.y();

  return true;
}

auto interop::camera_main_get_position(math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get position with no active camera");

    return;
  }

  *position = node.transform().position;
}

auto interop::camera_main_set_position(math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set position with no active camera");

    return;
  }

  if (!position) {
    utility::logger<"scripting">::error("Attempting to set null position of camera node '{}'", node.name());

    return;
  }

  node.transform().position = *position;
}

auto interop::camera_main_get_rotation(math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get rotation with no active camera");

    return;
  }

  *rotation = node.transform().rotation;
}

auto interop::camera_main_set_rotation(math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set rotation with no active camera");

    return;
  }

  if (!rotation) {
    utility::logger<"scripting">::error("Attempting to set null rotation of camera node '{}'", node.name());

    return;
  }

  node.transform().rotation = *rotation;
}

auto interop::camera_main_get_forward(math::vector3* forward) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get forward with no active camera");

    return;
  }

  *forward = node.transform().forward();
}

auto interop::camera_main_get_right(math::vector3* right) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get right with no active camera");

    return;
  }

  *right = node.transform().right();
}

auto interop::camera_main_get_up(math::vector3* up) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get up with no active camera");

    return;
  }

  *up = node.transform().up();
}

auto interop::camera_get_fov_degrees(std::uint64_t uuid, std::float_t* fov_degrees) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !fov_degrees) {
    utility::logger<"scripting">::error("Attempting to get fov_degrees of invalid node");

    return;
  }

  *fov_degrees = node.get_component<scenes::camera>().fov_degrees;
}

auto interop::camera_set_fov_degrees(std::uint64_t uuid, std::float_t fov_degrees) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set fov_degrees of invalid node");

    return;
  }

  node.get_component<scenes::camera>().fov_degrees = fov_degrees;
}

auto interop::camera_get_near_plane(std::uint64_t uuid, std::float_t* near_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !near_plane) {
    utility::logger<"scripting">::error("Attempting to get near_plane of invalid node");

    return;
  }

  *near_plane = node.get_component<scenes::camera>().near_plane;
}

auto interop::camera_set_near_plane(std::uint64_t uuid, std::float_t near_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set near_plane of invalid node");

    return;
  }

  node.get_component<scenes::camera>().near_plane = near_plane;
}

auto interop::camera_get_far_plane(std::uint64_t uuid, std::float_t* far_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !far_plane) {
    utility::logger<"scripting">::error("Attempting to get far_plane of invalid node");

    return;
  }

  *far_plane = node.get_component<scenes::camera>().far_plane;
}

auto interop::camera_set_far_plane(std::uint64_t uuid, std::float_t far_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set far_plane of invalid node");

    return;
  }

  node.get_component<scenes::camera>().far_plane = far_plane;
}

auto interop::camera_get_exposure(std::uint64_t uuid, std::float_t* exposure) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !exposure) {
    utility::logger<"scripting">::error("Attempting to get exposure of invalid node");

    return;
  }

  *exposure = node.get_component<scenes::camera>().exposure;
}

auto interop::camera_set_exposure(std::uint64_t uuid, std::float_t exposure) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set exposure of invalid node");

    return;
  }

  node.get_component<scenes::camera>().exposure = exposure;
}

auto interop::time_delta_time(std::float_t* delta_time) -> void {
  if (!delta_time) {
    utility::logger<"scripting">::error("Attempting to set null delta_time");

    return;
  }

  *delta_time = core::engine::delta_time().value();
}

auto interop::physics_raycast(math::ray* ray, std::float_t max_distance, std::uint32_t layer_mask, std::uint64_t* out_node_uuid, math::vector3* out_point, math::vector3* out_normal, std::float_t* out_distance) -> bool {
  if (!ray) {
    utility::logger<"scripting">::error("Attempting to call physics_raycast with a null ray");

    return false;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& physics_module = core::engine::get_module<physics::physics_module>();

  const auto hit = physics_module.raycast(scene, *ray, max_distance, scenes::layer_mask{layer_mask});

  if (!hit) {
    return false;
  }

  if (out_node_uuid) { 
    *out_node_uuid = hit->node.id().value(); 
  }

  if (out_point) { 
    *out_point = hit->point; 
  }

  if (out_normal) { 
    *out_normal = hit->normal; 
  }

  if (out_distance) {
    *out_distance = hit->distance;
  }

  return true;
}

auto interop::nav_bake(std::float_t agent_radius, std::float_t agent_height, std::float_t agent_max_slope, std::float_t agent_max_climb, std::float_t cell_size, std::float_t cell_height, std::float_t region_min_size, std::float_t region_merge_size, std::float_t edge_max_length, std::float_t edge_max_error, std::int32_t verts_per_poly) -> bool {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& physics_module = core::engine::get_module<physics::physics_module>();

  auto settings = physics::nav_settings{};
  settings.agent_radius = agent_radius;
  settings.agent_height = agent_height;
  settings.agent_max_slope = agent_max_slope;
  settings.agent_max_climb = agent_max_climb;
  settings.cell_size = cell_size;
  settings.cell_height = cell_height;
  settings.region_min_size = region_min_size;
  settings.region_merge_size = region_merge_size;
  settings.edge_max_length = edge_max_length;
  settings.edge_max_error = edge_max_error;
  settings.verts_per_poly = verts_per_poly;

  return physics_module.bake_navmesh(scene, settings);
}

auto interop::nav_has_navmesh() -> bool {
  auto& physics_module = core::engine::get_module<physics::physics_module>();

  return physics_module.has_navmesh();
}

auto interop::nav_sample_position(math::vector3* point, math::vector3* out_result) -> bool {
  if (!point) {
    utility::logger<"scripting">::error("Attempting to call nav_sample_position with a null point");

    return false;
  }

  auto& physics_module = core::engine::get_module<physics::physics_module>();

  if (!physics_module.has_navmesh()) {
    return false;
  }

  const auto& mesh = physics_module.navmesh();
  const auto ref = physics::find_nearest_poly(mesh, *point);

  if (ref == physics::null_poly_reference) {
    return false;
  }

  if (out_result) {
    *out_result = physics::closest_point_on_poly(mesh, ref, *point);
  }

  return true;
}

auto interop::nav_agent_set_destination(std::uint64_t uuid, math::vector3* target) -> bool {
  if (!target) {
    utility::logger<"scripting">::error("Attempting to call nav_agent_set_destination with a null target");

    return false;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent destination of invalid node");

    return false;
  }

  auto& physics_module = core::engine::get_module<physics::physics_module>();

  return physics_module.request_agent_move(node, *target);
}

auto interop::nav_agent_get_state(std::uint64_t uuid) -> std::uint8_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get nav_agent state of invalid node");

    return 0u;
  }

  return static_cast<std::uint8_t>(node.get_component<physics::nav_agent>().state);
}

auto interop::nav_agent_get_velocity(std::uint64_t uuid, math::vector3* out_velocity) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_velocity) {
    utility::logger<"scripting">::error("Attempting to get nav_agent velocity of invalid node");

    return;
  }

  *out_velocity = node.get_component<physics::nav_agent>().velocity;
}

auto interop::nav_agent_get_remaining_distance(std::uint64_t uuid, std::float_t* out_distance) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_distance) {
    utility::logger<"scripting">::error("Attempting to get nav_agent remaining distance of invalid node");

    return;
  }

  const auto& agent = node.get_component<physics::nav_agent>();

  *out_distance = math::vector3::distance(agent.corridor.position, agent.corridor.target);
}

auto interop::nav_agent_get_radius(std::uint64_t uuid, std::float_t* out_radius) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_radius) {
    utility::logger<"scripting">::error("Attempting to get nav_agent radius of invalid node");

    return;
  }

  *out_radius = node.get_component<physics::nav_agent>().radius;
}

auto interop::nav_agent_set_radius(std::uint64_t uuid, std::float_t radius) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent radius of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().radius = radius;
}

auto interop::nav_agent_get_height(std::uint64_t uuid, std::float_t* out_height) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_height) {
    utility::logger<"scripting">::error("Attempting to get nav_agent height of invalid node");

    return;
  }

  *out_height = node.get_component<physics::nav_agent>().height;
}

auto interop::nav_agent_set_height(std::uint64_t uuid, std::float_t height) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent height of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().height = height;
}

auto interop::nav_agent_get_base_offset(std::uint64_t uuid, std::float_t* out_base_offset) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_base_offset) {
    utility::logger<"scripting">::error("Attempting to get nav_agent base offset of invalid node");

    return;
  }

  *out_base_offset = node.get_component<physics::nav_agent>().base_offset;
}

auto interop::nav_agent_set_base_offset(std::uint64_t uuid, std::float_t base_offset) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent base offset of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().base_offset = base_offset;
}

auto interop::nav_agent_get_speed(std::uint64_t uuid, std::float_t* out_speed) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_speed) {
    utility::logger<"scripting">::error("Attempting to get nav_agent speed of invalid node");

    return;
  }

  *out_speed = node.get_component<physics::nav_agent>().max_speed;
}

auto interop::nav_agent_set_speed(std::uint64_t uuid, std::float_t speed) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent speed of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().max_speed = speed;
}

auto interop::nav_agent_get_acceleration(std::uint64_t uuid, std::float_t* out_acceleration) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_acceleration) {
    utility::logger<"scripting">::error("Attempting to get nav_agent acceleration of invalid node");

    return;
  }

  *out_acceleration = node.get_component<physics::nav_agent>().max_acceleration;
}

auto interop::nav_agent_set_acceleration(std::uint64_t uuid, std::float_t acceleration) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent acceleration of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().max_acceleration = acceleration;
}

auto interop::terrain_generate(std::uint32_t width, std::uint32_t depth, std::float_t cell_size, std::float_t frequency, std::float_t amplitude, std::uint32_t octaves) -> void {
  auto& terrain_module = core::engine::get_module<terrain::terrain_module>();

  terrain_module.generate(terrain::heightmap_generator_settings{width, depth, cell_size, frequency, amplitude, octaves});
}

auto interop::terrain_sample_height(math::vector2* world_xz, std::float_t* out_height) -> void {
  if (!world_xz || !out_height) {
    utility::logger<"scripting">::error("Attempting to call terrain_sample_height with a null pointer");

    return;
  }

  auto& terrain_module = core::engine::get_module<terrain::terrain_module>();

  *out_height = terrain_module.sample_height(*world_xz);
}

auto interop::terrain_sample_normal(math::vector2* world_xz, math::vector3* out_normal) -> void {
  if (!world_xz || !out_normal) {
    utility::logger<"scripting">::error("Attempting to call terrain_sample_normal with a null pointer");

    return;
  }

  auto& terrain_module = core::engine::get_module<terrain::terrain_module>();

  *out_normal = terrain_module.sample_normal(*world_xz);
}

auto interop::mesh_renderer_set_geometry(std::uint64_t uuid, math::vector3* positions, math::vector3* normals, math::vector2* uvs, std::uint32_t vertex_count, std::uint32_t* indices, std::uint32_t index_count, math::color* tint) -> void {
  if (!positions || !normals || !uvs || !indices || vertex_count == 0u || index_count == 0u) {
    utility::logger<"scripting">::error("Attempting to call mesh_renderer_set_geometry with invalid geometry");

    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call mesh_renderer_set_geometry on an invalid node");

    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto vertices = std::vector<assets::vertex>{};
  vertices.reserve(vertex_count);

  auto bounds = math::volume{};

  for (auto index = std::uint32_t{0}; index < vertex_count; ++index) {
    vertices.push_back(assets::vertex{positions[index], normals[index], uvs[index], math::vector4{1.0f, 0.0f, 0.0f, 1.0f}});
    bounds.include(positions[index]);
  }

  auto index_vector = std::vector<std::uint32_t>{indices, indices + index_count};

  auto& renderer = node.get_or_add_component<scenes::mesh_renderer>();

  // Reused across calls rather than created fresh every time -- see this method's own doc comment
  // in interop.hpp (create_material has a fixed capacity a live-edited mesh's per-frame calls would
  // otherwise exhaust). tint on a reused material is applied via update_material instead, which has
  // no such capacity cost.
  auto material = assets::material_handle{};

  if (!renderer.materials.empty() && renderer.materials.front().is_valid()) {
    material = renderer.materials.front();

    if (tint) {
      assets_module.update_material(material, assets::material::create_info{
        .base_color_factor = *tint,
        .metallic_factor = 0.0f,
        .roughness_factor = 0.8f
      });
    }
  } else {
    material = assets_module.create_material(assets::material::create_info{
      .name = "Script Mesh",
      .base_color_factor = tint ? *tint : math::color::white(),
      .metallic_factor = 0.0f,
      .roughness_factor = 0.8f
    });
  }

  auto submeshes = std::vector<assets::mesh::submesh>{assets::mesh::submesh{0u, index_count, bounds, material}};

  renderer.mesh = assets_module.create_mesh(std::move(vertices), std::move(index_vector), std::move(submeshes), bounds);
  renderer.materials = std::vector<assets::material_handle>{material};
}

// Shared by every Canvas_*/RectTransform_*/UIImage_*/UIText_*/UIButton_* binding below -- the same
// uuid-resolve step Transform_*/Rigidbody_* already do inline, factored out here since there are
// enough of these bindings that repeating it each time would dwarf the actual field access.
auto resolve_node(std::uint64_t uuid) -> scenes::node {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  return scene.find(math::uuid::from_value(uuid));
}

auto interop::canvas_get_sort_order(std::uint64_t uuid, std::int32_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->sort_order;
}

auto interop::canvas_set_sort_order(std::uint64_t uuid, std::int32_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->sort_order = value;
}

auto interop::rect_transform_get_anchor_min(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->anchor_min;
}

auto interop::rect_transform_set_anchor_min(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->anchor_min = *value;
}

auto interop::rect_transform_get_anchor_max(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->anchor_max;
}

auto interop::rect_transform_set_anchor_max(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->anchor_max = *value;
}

auto interop::rect_transform_get_anchored_position(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->anchored_position;
}

auto interop::rect_transform_set_anchored_position(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->anchored_position = *value;
}

auto interop::rect_transform_get_size_delta(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->size_delta;
}

auto interop::rect_transform_set_size_delta(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->size_delta = *value;
}

auto interop::rect_transform_get_pivot(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->pivot;
}

auto interop::rect_transform_set_pivot(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->pivot = *value;
}

auto interop::ui_image_get_tint(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_image>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->tint;
}

auto interop::ui_image_set_tint(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_image>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->tint = *value;
}

auto interop::ui_image_load_sprite(std::uint64_t uuid, managed::string path) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_image>();

  if (!node.is_valid() || !component) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  component->sprite = assets_module.load_texture(std::filesystem::path{std::string{path}});
}

auto interop::ui_text_get_text(std::uint64_t uuid) -> managed::string {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return managed::string::create("");
  }

  return managed::string::create(component->text);
}

auto interop::ui_text_set_text(std::uint64_t uuid, managed::string value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->text = std::string{value};
}

auto interop::ui_text_get_font_size(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->font_size;
}

auto interop::ui_text_set_font_size(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->font_size = value;
}

auto interop::ui_text_get_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->color;
}

auto interop::ui_text_set_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->color = *value;
}

auto interop::ui_button_get_interactable(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_button_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_button_get_normal_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->normal_color;
}

auto interop::ui_button_set_normal_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->normal_color = *value;
}

auto interop::ui_button_get_hovered_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->hovered_color;
}

auto interop::ui_button_set_hovered_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->hovered_color = *value;
}

auto interop::ui_button_get_pressed_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->pressed_color;
}

auto interop::ui_button_set_pressed_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->pressed_color = *value;
}

auto interop::ui_button_get_is_hovered(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->is_hovered;
}

auto interop::ui_button_get_is_pressed(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->is_pressed;
}

auto interop::ui_button_get_was_clicked(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->was_clicked;
}

auto interop::canvas_group_get_alpha(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->alpha;
}

auto interop::canvas_group_set_alpha(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->alpha = value;
}

auto interop::canvas_group_get_interactable(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  return node.is_valid() && component && component->interactable;
}

auto interop::canvas_group_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::canvas_group_get_blocks_raycasts(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  return node.is_valid() && component && component->blocks_raycasts;
}

auto interop::canvas_group_set_blocks_raycasts(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->blocks_raycasts = value;
}

auto interop::canvas_group_get_ignore_parent_groups(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  return node.is_valid() && component && component->ignore_parent_groups;
}

auto interop::canvas_group_set_ignore_parent_groups(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->ignore_parent_groups = value;
}

auto interop::canvas_wants_pointer_capture() -> bool {
  auto& canvas_module = core::engine::get_module<canvas::canvas_module>();

  return canvas_module.wants_pointer_capture();
}

auto interop::ui_toggle_get_is_on(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  return node.is_valid() && component && component->is_on;
}

auto interop::ui_toggle_set_is_on(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->is_on = value;
}

auto interop::ui_toggle_get_interactable(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_toggle_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_slider_get_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->value;
}

auto interop::ui_slider_set_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->value = value;
}

auto interop::ui_slider_get_min_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->min_value;
}

auto interop::ui_slider_set_min_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->min_value = value;
}

auto interop::ui_slider_get_max_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->max_value;
}

auto interop::ui_slider_set_max_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->max_value = value;
}

auto interop::ui_slider_get_whole_numbers(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  return node.is_valid() && component && component->whole_numbers;
}

auto interop::ui_slider_set_whole_numbers(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->whole_numbers = value;
}

auto interop::ui_slider_get_interactable(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_slider_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_scrollbar_get_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->value;
}

auto interop::ui_scrollbar_set_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->value = value;
}

auto interop::ui_scrollbar_get_size(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->size;
}

auto interop::ui_scrollbar_set_size(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->size = value;
}

auto interop::ui_scrollbar_get_interactable(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_scrollbar_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_scroll_rect_get_normalized_position(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->normalized_position;
}

auto interop::ui_scroll_rect_set_normalized_position(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->normalized_position = *value;
}

auto interop::ui_scroll_rect_get_horizontal(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  return node.is_valid() && component && component->horizontal;
}

auto interop::ui_scroll_rect_set_horizontal(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->horizontal = value;
}

auto interop::ui_scroll_rect_get_vertical(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  return node.is_valid() && component && component->vertical;
}

auto interop::ui_scroll_rect_set_vertical(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->vertical = value;
}

auto interop::ui_mask_get_show_mask_graphic(std::uint64_t uuid) -> bool {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_mask>();

  return node.is_valid() && component && component->show_mask_graphic;
}

auto interop::ui_mask_set_show_mask_graphic(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_mask>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->show_mask_graphic = value;
}

} // namespace sbx::scripting
