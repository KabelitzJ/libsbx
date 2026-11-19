// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

// Reads the "type"+"value" pair written by save_animation_parameter_value in asset_residency.cpp.
static auto load_animation_parameter_value(const YAML::Node& node) -> animation_parameter_value {
  const auto type = node["type"] ? node["type"].as<std::string>() : std::string{"float"};

  if (type == "bool") {
    return animation_parameter_value{node["value"] ? node["value"].as<bool>() : false};
  }

  if (type == "int") {
    return animation_parameter_value{node["value"] ? node["value"].as<std::int32_t>() : std::int32_t{0}};
  }

  if (type == "trigger") {
    return animation_parameter_value{animation_trigger{}};
  }

  return animation_parameter_value{node["value"] ? node["value"].as<std::float_t>() : 0.0f};
}

static auto load_animation_condition_comparator(const std::string& value) -> animation_condition_comparator {
  if (value == "not_equals") return animation_condition_comparator::not_equals;
  if (value == "greater") return animation_condition_comparator::greater;
  if (value == "greater_or_equal") return animation_condition_comparator::greater_or_equal;
  if (value == "less") return animation_condition_comparator::less;
  if (value == "less_or_equal") return animation_condition_comparator::less_or_equal;
  return animation_condition_comparator::equals;
}
static auto load_animation_blend_type(const std::string& value) -> animation_blend_type {
  if (value == "freeform_directional_2d") return animation_blend_type::freeform_directional_2d;
  if (value == "freeform_cartesian_2d") return animation_blend_type::freeform_cartesian_2d;
  if (value == "direct") return animation_blend_type::direct;
  return animation_blend_type::linear_1d;
}

// Reads the tree written by save_animation_motion in asset_residency.cpp.
static auto load_animation_motion(const YAML::Node& node) -> animation_motion {
  auto motion = animation_motion{};

  if (node["kind"] && node["kind"].as<std::string>() == "blend") {
    motion.kind = animation_motion_kind::blend;
  }

  if (node["clip_name"]) motion.clip_name = node["clip_name"].as<std::string>();
  if (node["blend_type"]) motion.blend_type = load_animation_blend_type(node["blend_type"].as<std::string>());
  if (node["parameter_x"]) motion.parameter_x = node["parameter_x"].as<std::string>();
  if (node["parameter_y"]) motion.parameter_y = node["parameter_y"].as<std::string>();
  if (node["threshold"]) motion.threshold = node["threshold"].as<std::float_t>();
  if (node["direct_parameter"]) motion.direct_parameter = node["direct_parameter"].as<std::string>();

  if (const auto position_node = node["position"]) {
    if (position_node["x"]) motion.position.x() = position_node["x"].as<std::float_t>();
    if (position_node["y"]) motion.position.y() = position_node["y"].as<std::float_t>();
  }

  if (const auto children = node["children"]) {
    motion.children.reserve(children.size());

    for (const auto child : children) {
      motion.children.push_back(load_animation_motion(child));
    }
  }

  return motion;
}

// One layer's state machine, from a "layers" entry, or from the root of a graph saved before layers.
static auto load_animation_layer(const YAML::Node& node) -> animation_layer {
  auto layer = animation_layer{};

  if (node["layer_name"]) layer.name = node["layer_name"].as<std::string>();
  if (node["entry_state_id"]) layer.entry_state_id = node["entry_state_id"].as<std::uint32_t>();
  if (node["weight"]) layer.weight = node["weight"].as<std::float_t>();

  if (node["blend_mode"] && node["blend_mode"].as<std::string>() == "additive") {
    layer.blend_mode = animation_layer_blend_mode::additive;
  }

  if (const auto mask = node["mask"]) {
    layer.mask.reserve(mask.size());

    for (const auto joint : mask) {
      layer.mask.push_back(joint.as<std::string>());
    }
  }

  if (const auto states = node["states"]) {
    layer.states.reserve(states.size());

    for (const auto state_node : states) {
      auto state = animation_state{};

      if (state_node["id"]) state.id = state_node["id"].as<std::uint32_t>();
      if (state_node["name"]) state.name = state_node["name"].as<std::string>();
      // Graphs saved before blend trees name one clip on the state itself.
      if (const auto motion_node = state_node["motion"]) {
        state.motion = load_animation_motion(motion_node);
      } else if (state_node["clip_name"]) {
        state.motion.clip_name = state_node["clip_name"].as<std::string>();
      }

      if (state_node["speed"]) state.speed = state_node["speed"].as<std::float_t>();
      if (state_node["speed_parameter"]) state.speed_parameter = state_node["speed_parameter"].as<std::string>();
      if (state_node["loop"]) state.loop = state_node["loop"].as<bool>();

      if (const auto position_node = state_node["editor_position"]) {
        if (position_node["x"]) state.editor_position.x() = position_node["x"].as<std::float_t>();
        if (position_node["y"]) state.editor_position.y() = position_node["y"].as<std::float_t>();
      }

      layer.states.push_back(state);
    }
  }

  if (const auto transitions = node["transitions"]) {
    layer.transitions.reserve(transitions.size());

    for (const auto transition_node : transitions) {
      auto transition = animation_transition{};

      if (transition_node["from_state"]) transition.from_state = transition_node["from_state"].as<std::uint32_t>();
      if (transition_node["to_state"]) transition.to_state = transition_node["to_state"].as<std::uint32_t>();
      if (transition_node["duration"]) transition.duration = transition_node["duration"].as<std::float_t>();
      if (transition_node["has_exit_time"]) transition.has_exit_time = transition_node["has_exit_time"].as<bool>();
      if (transition_node["exit_time"]) transition.exit_time = transition_node["exit_time"].as<std::float_t>();

      if (const auto conditions_node = transition_node["conditions"]) {
        for (const auto condition_node : conditions_node) {
          auto condition = animation_condition{};

          if (condition_node["parameter_name"]) condition.parameter_name = condition_node["parameter_name"].as<std::string>();
          if (condition_node["comparator"]) condition.comparator = load_animation_condition_comparator(condition_node["comparator"].as<std::string>());
          condition.expected = load_animation_parameter_value(condition_node);

          transition.conditions.push_back(condition);
        }
      }

      layer.transitions.push_back(transition);
    }
  }

  return layer;
}

auto asset_cooker::parse_animation_graph_file(const std::filesystem::path& source) -> std::optional<animation_graph::create_info> {
  auto root = YAML::Node{};

  try {
    root = YAML::LoadFile(source.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not parse animation_graph '{}' ({})", source.generic_string(), exception.what());
    return std::nullopt;
  }

  auto info = animation_graph::create_info{};

  if (root["name"]) info.name = root["name"].as<std::string>();

  if (const auto parameters = root["parameters"]) {
    info.parameters.reserve(parameters.size());

    for (const auto parameter_node : parameters) {
      auto parameter = animation_parameter{};

      if (parameter_node["name"]) parameter.name = parameter_node["name"].as<std::string>();
      parameter.default_value = load_animation_parameter_value(parameter_node);

      info.parameters.push_back(parameter);
    }
  }

  if (const auto layers = root["layers"]) {
    info.layers.reserve(layers.size());

    for (const auto layer_node : layers) {
      info.layers.push_back(load_animation_layer(layer_node));
    }
  } else {
    info.layers.push_back(load_animation_layer(root));
  }

  return info;
}

} // namespace sbx::assets
