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

auto interop::node_destroy(std::uint64_t uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to destroy invalid node");

    return;
  }

  // Mirrors scripting_module::detach_script's "OnDestroy before the instance goes away" ordering --
  // destroying the node out from under a live script instance with no notification would otherwise
  // silently drop it, same concern run_on_destroy's doc comment raises for a full scene teardown.
  if (node.has_component<scripting::scripts>()) {
    for (auto& instance : node.get_component<scripting::scripts>().instances) {
      instance.invoke("OnDestroy");
    }
  }

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

  const auto aspect = (extent.y() > 0u) ? (static_cast<std::float_t>(extent.x()) / static_cast<std::float_t>(extent.y())) : 1.0f;

  const auto view = math::matrix4x4::inverted(node.world_matrix());
  const auto projection = math::matrix4x4::perspective(math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);
  const auto inverse_view_projection = math::matrix4x4::inverted(projection * view);

  const auto ndc_x = (extent.x() > 0u) ? ((position->x() / static_cast<std::float_t>(extent.x())) * 2.0f - 1.0f) : 0.0f;
  const auto ndc_y = (extent.y() > 0u) ? ((position->y() / static_cast<std::float_t>(extent.y())) * 2.0f - 1.0f) : 0.0f;

  const auto unproject = [&inverse_view_projection, ndc_x, ndc_y](std::float_t ndc_z) -> math::vector3 {
    const auto point = inverse_view_projection * math::vector4{ndc_x, ndc_y, ndc_z, 1.0f};
    return math::vector3{point.x(), point.y(), point.z()} / point.w();
  };

  const auto near_point = unproject(0.0f);
  const auto far_point = unproject(1.0f);

  *ray = math::ray{near_point, far_point - near_point};
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

} // namespace sbx::scripting
