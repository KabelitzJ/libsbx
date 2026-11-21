// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <optional>
#include <ranges>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>
#include <libsbx/physics/nav/nav_settings.hpp>
#include <libsbx/physics/nav/navmesh.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/prefab_prototype.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/animation/pose_evaluation.hpp>

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

auto interop::node_get_layer(std::uint64_t uuid) -> std::uint8_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get layer of invalid node");

    return 0u;
  }

  return node.layer().index;
}

auto interop::node_set_layer(std::uint64_t uuid, std::uint8_t layer) -> void {
  if (layer >= core::layer_count) {
    utility::logger<"scripting">::error("Attempting to set layer {} (valid: 0-{})", layer, core::layer_count - 1u);

    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set layer of invalid node");

    return;
  }

  node.layer().index = layer;
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

  return animator_get_layer_state_name(uuid, 0);
}

// The animator of a node, with its layers matched to its graph; null (after logging) if the node has none.
auto layered_animator_of(std::uint64_t uuid, std::string_view what) -> memory::observer_ptr<scenes::animator> {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !node.has_component<scenes::animator>()) {
    utility::logger<"scripting">::error("Animator.{}: node {} has no animator", what, uuid);
    return nullptr;
  }

  auto& animator = node.get_component<scenes::animator>();

  if (animator.graph.is_valid()) {
    animation::reconcile_animator_layers(*animator.graph, animator);
  }

  return memory::make_observer(animator);
}

auto interop::animation_event_get_name() -> managed::string {
  const auto event = core::engine::get_module<scripting_module>().current_animation_event();

  return managed::string::create(event != nullptr ? event->name.c_str() : "");
}

auto interop::animation_event_get_string_parameter() -> managed::string {
  const auto event = core::engine::get_module<scripting_module>().current_animation_event();

  return managed::string::create(event != nullptr ? event->string_parameter.c_str() : "");
}

auto interop::animation_event_get_clip_name() -> managed::string {
  const auto event = core::engine::get_module<scripting_module>().current_animation_event();

  return managed::string::create(event != nullptr ? event->clip_name.c_str() : "");
}

auto interop::animator_mark_root_motion_handled() -> void {
  if (auto motion = core::engine::get_module<scripting_module>().current_root_motion()) {
    motion->handled = true;
  }
}

auto interop::animator_get_apply_root_motion(std::uint64_t uuid) -> managed::bool32 {
  const auto animator = layered_animator_of(uuid, "ApplyRootMotion");

  return animator != nullptr && animator->apply_root_motion;
}

auto interop::animator_set_apply_root_motion(std::uint64_t uuid, bool value) -> void {
  if (auto animator = layered_animator_of(uuid, "ApplyRootMotion")) {
    animator->apply_root_motion = value;
  }
}

auto interop::animator_get_delta_position(std::uint64_t uuid, math::vector3* out_value) -> void {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!out_value || !node.is_valid() || !node.has_component<scenes::animator>()) {
    return;
  }

  const auto& delta = node.get_component<scenes::animator>().root_motion_delta_position;

  *out_value = math::vector3{node.world_matrix() * math::vector4{delta, 0.0f}};
}

auto interop::animator_get_delta_rotation(std::uint64_t uuid, math::quaternion* out_value) -> void {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!out_value || !node.is_valid() || !node.has_component<scenes::animator>()) {
    return;
  }

  *out_value = node.get_component<scenes::animator>().root_motion_delta_rotation;
}

// An animator's IK chain by name; null (after logging) if the node has no animator or no such chain.
auto ik_chain_of(std::uint64_t uuid, managed::string chain, std::string_view what) -> scenes::animator_ik_chain* {
  auto animator = layered_animator_of(uuid, what);

  if (animator == nullptr) {
    return nullptr;
  }

  auto* found = animator->find_ik_chain(std::string{chain});

  if (found == nullptr) {
    utility::logger<"scripting">::error("Animator.{}: no IK chain named '{}'", what, std::string{chain});
  }

  return found;
}

auto interop::animator_set_ik_position(std::uint64_t uuid, managed::string chain, math::vector3* position) -> void {
  if (auto* found = ik_chain_of(uuid, chain, "SetIKPosition"); found != nullptr && position != nullptr) {
    found->target_position = *position;
  }
}

auto interop::animator_set_ik_rotation(std::uint64_t uuid, managed::string chain, math::quaternion* rotation) -> void {
  if (auto* found = ik_chain_of(uuid, chain, "SetIKRotation"); found != nullptr && rotation != nullptr) {
    found->target_rotation = math::quaternion::normalized(*rotation);
  }
}

auto interop::animator_set_ik_hint(std::uint64_t uuid, managed::string chain, math::vector3* position) -> void {
  if (auto* found = ik_chain_of(uuid, chain, "SetIKHint"); found != nullptr && position != nullptr) {
    found->hint_position = *position;
    found->has_hint = true;
  }
}

auto interop::animator_clear_ik_hint(std::uint64_t uuid, managed::string chain) -> void {
  if (auto* found = ik_chain_of(uuid, chain, "ClearIKHint")) {
    found->has_hint = false;
  }
}

auto interop::animator_set_ik_weights(std::uint64_t uuid, managed::string chain, std::float_t position_weight, std::float_t rotation_weight) -> void {
  if (auto* found = ik_chain_of(uuid, chain, "SetIKWeights")) {
    found->position_weight = std::clamp(position_weight, 0.0f, 1.0f);
    found->rotation_weight = std::clamp(rotation_weight, 0.0f, 1.0f);
  }
}

auto interop::animator_set_look_at(std::uint64_t uuid, math::vector3* position, std::float_t weight) -> void {
  if (auto animator = layered_animator_of(uuid, "SetLookAt"); animator != nullptr && position != nullptr) {
    animator->look_at_target = *position;
    animator->look_at_weight = std::clamp(weight, 0.0f, 1.0f);
  }
}

auto interop::animator_get_layer_state_name(std::uint64_t uuid, std::int32_t layer) -> managed::string {
  const auto animator = layered_animator_of(uuid, "GetCurrentStateName");

  if (animator == nullptr || !animator->graph.is_valid() || layer < 0 || static_cast<std::size_t>(layer) >= animator->layers.size()) {
    return managed::string::create("");
  }

  const auto& states = animator->graph->layers()[static_cast<std::size_t>(layer)].states;
  const auto entry = std::ranges::find(states, animator->layers[static_cast<std::size_t>(layer)].current_state_id, &assets::animation_state::id);

  return managed::string::create(entry != states.end() ? entry->name.c_str() : "");
}

auto interop::animator_get_layer_count(std::uint64_t uuid) -> std::int32_t {
  const auto animator = layered_animator_of(uuid, "LayerCount");

  return animator != nullptr ? static_cast<std::int32_t>(animator->layers.size()) : 0;
}

auto interop::animator_get_layer_index(std::uint64_t uuid, managed::string name) -> std::int32_t {
  const auto animator = layered_animator_of(uuid, "GetLayerIndex");

  if (animator == nullptr || !animator->graph.is_valid()) {
    return -1;
  }

  const auto& layers = animator->graph->layers();
  const auto entry = std::ranges::find(layers, std::string{name}, &assets::animation_layer::name);

  return entry != layers.end() ? static_cast<std::int32_t>(entry - layers.begin()) : -1;
}

auto interop::animator_get_layer_weight(std::uint64_t uuid, std::int32_t layer) -> std::float_t {
  const auto animator = layered_animator_of(uuid, "GetLayerWeight");

  if (animator == nullptr || layer < 0 || static_cast<std::size_t>(layer) >= animator->layers.size()) {
    return 0.0f;
  }

  return layer == 0 ? 1.0f : animator->layers[static_cast<std::size_t>(layer)].weight;
}

auto interop::animator_set_layer_weight(std::uint64_t uuid, std::int32_t layer, std::float_t weight) -> void {
  if (auto animator = layered_animator_of(uuid, "SetLayerWeight"); animator != nullptr && layer >= 0) {
    animator->set_layer_weight(static_cast<std::size_t>(layer), weight);
  }
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

auto interop::rigidbody_get_collision_detection(std::uint64_t uuid) -> std::uint32_t {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !node.has_component<physics::rigidbody>()) {
    utility::logger<"scripting">::error("Rigidbody.CollisionDetection: node {} has no rigidbody", uuid);
    return 0u;
  }

  return static_cast<std::uint32_t>(node.get_component<physics::rigidbody>().collision_detection);
}

auto interop::rigidbody_set_collision_detection(std::uint64_t uuid, std::uint32_t mode) -> void {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !node.has_component<physics::rigidbody>()) {
    utility::logger<"scripting">::error("Rigidbody.CollisionDetection: node {} has no rigidbody", uuid);
    return;
  }

  if (mode > static_cast<std::uint32_t>(physics::collision_detection_mode::continuous)) {
    utility::logger<"scripting">::error("Rigidbody.CollisionDetection: invalid mode {}", mode);
    return;
  }

  node.get_component<physics::rigidbody>().collision_detection = static_cast<physics::collision_detection_mode>(mode);
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

  auto instance = scenes::instantiate_prefab_prototype(scene, prefab, scenes_module.prefab_prototypes());

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

auto interop::scene_save(managed::string path) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& scene = scenes_module.active_scene();

  const auto scene_path = std::filesystem::path{std::string{path}};

  auto handle = assets_module.create_scene(scenes::scene_serializer::build(scene, scenes_module.active_scene_handle()), scene.name());
  assets_module.save_scene(handle, scene_path);

  scenes_module.set_scene_path(scenes_module.active_scene_handle(), scene_path);
}

auto interop::scene_new() -> void {
  core::engine::get_module<scenes::scenes_module>().new_scene();
}

static auto load_scene_mode_of(std::uint32_t mode) -> scenes::load_scene_mode {
  return mode == static_cast<std::uint32_t>(scenes::load_scene_mode::additive) ? scenes::load_scene_mode::additive : scenes::load_scene_mode::single;
}

// A scene-list name or path; outside the list it's still tried as a path, which the runtime then rejects in scenes_module.
static auto requested_scene_path(managed::string name_or_path) -> std::filesystem::path {
  const auto requested = std::string{name_or_path};

  if (auto path = core::engine::project().resolve_scene(requested)) {
    return *path;
  }

  auto path = std::filesystem::path{requested};

  if (path.extension() != ".scene") {
    path += ".scene";
  }

  return path;
}

static auto report(const scenes::queued_load& queued, std::uint32_t* out_handle) -> std::uint32_t {
  if (out_handle) {
    *out_handle = queued.handle;
  }

  return queued.operation;
}

static auto build_index_scene_path(std::int32_t build_index, std::uint32_t* out_handle) -> std::optional<std::filesystem::path> {
  if (out_handle) {
    *out_handle = scenes::invalid_scene_handle;
  }

  auto path = build_index >= 0 ? core::engine::project().scene_at(static_cast<std::size_t>(build_index)) : std::nullopt;

  if (!path) {
    utility::logger<"scripting">::error("SceneManager.LoadScene({}): no enabled scene with that build index", build_index);
  }

  return path;
}

auto interop::scene_load_by_name(managed::string name_or_path, std::uint32_t mode, std::uint32_t* out_handle) -> std::uint32_t {
  return report(core::engine::get_module<scenes::scenes_module>().load_scene(requested_scene_path(name_or_path), load_scene_mode_of(mode)), out_handle);
}

auto interop::scene_load_by_index(std::int32_t build_index, std::uint32_t mode, std::uint32_t* out_handle) -> std::uint32_t {
  const auto path = build_index_scene_path(build_index, out_handle);

  return path ? report(core::engine::get_module<scenes::scenes_module>().load_scene(*path, load_scene_mode_of(mode)), out_handle) : 0u;
}

auto interop::scene_load_async_by_name(managed::string name_or_path, std::uint32_t mode, std::uint32_t* out_handle) -> std::uint32_t {
  return report(core::engine::get_module<scenes::scenes_module>().load_scene_async(requested_scene_path(name_or_path), load_scene_mode_of(mode)), out_handle);
}

auto interop::scene_load_async_by_index(std::int32_t build_index, std::uint32_t mode, std::uint32_t* out_handle) -> std::uint32_t {
  const auto path = build_index_scene_path(build_index, out_handle);

  return path ? report(core::engine::get_module<scenes::scenes_module>().load_scene_async(*path, load_scene_mode_of(mode)), out_handle) : 0u;
}

auto interop::scene_get_operation_progress(std::uint32_t operation) -> std::float_t {
  return core::engine::get_module<scenes::scenes_module>().operation_progress(operation);
}

auto interop::scene_get_allow_activation(std::uint32_t operation) -> managed::bool32 {
  return core::engine::get_module<scenes::scenes_module>().allow_scene_activation(operation);
}

auto interop::scene_set_allow_activation(std::uint32_t operation, managed::bool32 allow) -> void {
  core::engine::get_module<scenes::scenes_module>().set_allow_scene_activation(operation, allow != 0u);
}

auto interop::scene_unload(std::uint32_t handle) -> std::uint32_t {
  return core::engine::get_module<scenes::scenes_module>().unload_scene(handle);
}

auto interop::scene_is_operation_done(std::uint32_t operation) -> managed::bool32 {
  return core::engine::get_module<scenes::scenes_module>().is_operation_done(operation);
}

// The loaded scenes scripts see: everything but the persistent scene, like Unity's SceneManager.
static auto script_visible_scenes() -> std::vector<scenes::loaded_scene> {
  auto visible = std::vector<scenes::loaded_scene>{};

  for (const auto& loaded : core::engine::get_module<scenes::scenes_module>().loaded_scenes()) {
    if (loaded.handle != scenes::persistent_scene_handle) {
      visible.push_back(loaded);
    }
  }

  return visible;
}

auto interop::scene_get_count() -> std::int32_t {
  return static_cast<std::int32_t>(script_visible_scenes().size());
}

auto interop::scene_get_at(std::int32_t index) -> std::uint32_t {
  const auto visible = script_visible_scenes();

  return (index >= 0 && static_cast<std::size_t>(index) < visible.size()) ? visible[static_cast<std::size_t>(index)].handle : scenes::invalid_scene_handle;
}

auto interop::scene_get_active() -> std::uint32_t {
  return core::engine::get_module<scenes::scenes_module>().active_scene_handle();
}

auto interop::scene_set_active(std::uint32_t handle) -> managed::bool32 {
  return core::engine::get_module<scenes::scenes_module>().set_active_scene(handle);
}

auto interop::scene_find_by_name(managed::string name) -> std::uint32_t {
  const auto lowercase = [](std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return text;
  };

  const auto wanted = lowercase(std::string{name});

  for (const auto& loaded : script_visible_scenes()) {
    if (lowercase(loaded.name) == wanted) {
      return loaded.handle;
    }
  }

  return scenes::invalid_scene_handle;
}

auto interop::scene_find_by_path(managed::string path) -> std::uint32_t {
  auto wanted = std::filesystem::path{std::string{path}};

  if (wanted.extension() != ".scene") {
    wanted += ".scene";
  }

  for (const auto& loaded : script_visible_scenes()) {
    if (loaded.path.lexically_normal() == wanted.lexically_normal()) {
      return loaded.handle;
    }
  }

  return scenes::invalid_scene_handle;
}

auto interop::scene_find_by_build_index(std::int32_t build_index) -> std::uint32_t {
  for (const auto& loaded : script_visible_scenes()) {
    if (loaded.build_index >= 0 && loaded.build_index == build_index) {
      return loaded.handle;
    }
  }

  return scenes::invalid_scene_handle;
}

auto interop::scene_get_name(std::uint32_t handle) -> managed::string {
  const auto loaded = core::engine::get_module<scenes::scenes_module>().find_loaded_scene(handle);

  return managed::string::create(loaded ? loaded->name.c_str() : "");
}

auto interop::scene_get_path(std::uint32_t handle) -> managed::string {
  const auto loaded = core::engine::get_module<scenes::scenes_module>().find_loaded_scene(handle);

  return managed::string::create(loaded ? loaded->path.generic_string().c_str() : "");
}

auto interop::scene_get_build_index(std::uint32_t handle) -> std::int32_t {
  const auto loaded = core::engine::get_module<scenes::scenes_module>().find_loaded_scene(handle);

  return loaded ? loaded->build_index : -1;
}

auto interop::scene_is_loaded(std::uint32_t handle) -> managed::bool32 {
  return static_cast<bool>(core::engine::get_module<scenes::scenes_module>().find_loaded_scene(handle));
}

auto interop::scene_get_root_nodes(std::uint32_t handle, std::uint64_t* out_node_uuids, std::uint32_t capacity) -> std::uint32_t {
  auto& scene = core::engine::get_module<scenes::scenes_module>().active_scene();

  auto roots = std::vector<std::uint64_t>{};

  for (const auto entity : scene.root().get_component<scenes::relationship>().children) {
    if (const auto node = scene.node_of(entity); scene.scene_of(node) == handle) {
      roots.push_back(node.id().value());
    }
  }

  if (out_node_uuids) {
    std::ranges::copy(roots | std::views::take(capacity), out_node_uuids);
  }

  return static_cast<std::uint32_t>(roots.size());
}

auto interop::scene_move_node(std::uint64_t node_uuid, std::uint32_t handle) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  return scenes_module.move_node_to_scene(scenes_module.active_scene().find(math::uuid::from_value(node_uuid)), handle);
}

auto interop::node_dont_destroy_on_load(std::uint64_t node_uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto node = scenes_module.active_scene().find(math::uuid::from_value(node_uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Node.DontDestroyOnLoad: invalid node {}", node_uuid);
    return;
  }

  scenes_module.dont_destroy_on_load(node);
}

auto interop::scene_count_in_build_settings() -> std::int32_t {
  return static_cast<std::int32_t>(core::engine::project().scene_count_in_build());
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
