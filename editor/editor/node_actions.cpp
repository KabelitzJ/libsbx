// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/node_actions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include <fmt/format.h>

#include <imgui.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <editor/editor_module.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/composite_command.hpp>
#include <editor/commands/scene_commands.hpp>

namespace editor {

struct node_location {
  std::optional<sbx::math::uuid> parent_id{}; // nullopt = top-level
  std::size_t index{0u};
}; // struct node_location

auto real_parent(sbx::scenes::scene& scene, const sbx::scenes::node& node) -> sbx::scenes::node {
  auto parent = scene.node_of(node.get_component<sbx::scenes::relationship>().parent);

  return parent.has_component<sbx::scenes::id>() ? parent : sbx::scenes::node{};
}

auto location_of(sbx::scenes::scene& scene, const sbx::scenes::node& node) -> node_location {
  auto location = node_location{};
  auto parent = real_parent(scene, node);

  if (parent.is_valid()) {
    location.parent_id = parent.id();
  }

  const auto& siblings = (parent.is_valid() ? parent : scene.root()).get_component<sbx::scenes::relationship>().children;

  for (auto index = std::size_t{0u}; index < siblings.size(); ++index) {
    if (scene.node_of(siblings[index]).id() == node.id()) {
      location.index = index;
      break;
    }
  }

  return location;
}

// Keeps prefab membership only where the copy contains the instance root, or the copy becomes a second member with the same member_id.
auto strip_partial_prefab_membership(YAML::Node& snapshot) -> void {
  auto covered = std::unordered_set<sbx::math::uuid>{};

  for (auto node_yaml : snapshot["nodes"]) {
    const auto parent = node_yaml["parent"];
    auto is_covered = parent && covered.contains(parent.as<sbx::math::uuid>());

    for (const auto component : node_yaml["components"]) {
      is_covered |= component["type"].as<std::string>() == "prefab_instance";
    }

    if (is_covered) {
      covered.insert(node_yaml["id"].as<sbx::math::uuid>());
      continue;
    }

    auto kept = YAML::Node{YAML::NodeType::Sequence};

    for (const auto component : node_yaml["components"]) {
      if (component["type"].as<std::string>() != "prefab_member") {
        kept.push_back(component);
      }
    }

    node_yaml["components"] = kept;
  }
}

auto fresh_copy(const YAML::Node& snapshot) -> YAML::Node {
  auto copy = sbx::scenes::scene_serializer::with_fresh_ids(snapshot);
  strip_partial_prefab_membership(copy);

  return copy;
}

auto push_inserts(editor_state& state, sbx::scenes::scene& scene, std::vector<std::unique_ptr<command>> commands, std::string label) -> void {
  if (commands.empty()) {
    return;
  }

  auto created_ids = std::vector<sbx::math::uuid>{};

  for (const auto& entry : commands) {
    created_ids.push_back(static_cast<const insert_subtree_command&>(*entry).id());
  }

  if (commands.size() == 1u) {
    state.push_command(scene, std::move(commands.front()));
  } else {
    state.push_command(scene, std::make_unique<composite_command>(std::move(commands), std::move(label)));
  }

  state.set_node_selection(std::move(created_ids));
}

auto selection_roots(sbx::scenes::scene& scene, const std::vector<sbx::math::uuid>& ids) -> std::vector<sbx::math::uuid> {
  auto roots = std::vector<sbx::math::uuid>{};

  for (const auto id : ids) {
    auto node = scene.find(id);

    if (!node.is_valid()) {
      continue;
    }

    auto has_selected_ancestor = false;

    for (auto ancestor = real_parent(scene, node); ancestor.is_valid(); ancestor = real_parent(scene, ancestor)) {
      if (std::ranges::find(ids, ancestor.id()) != ids.end()) {
        has_selected_ancestor = true;
        break;
      }
    }

    if (!has_selected_ancestor) {
      roots.push_back(id);
    }
  }

  return roots;
}

auto copy_selection(editor_state& state, sbx::scenes::scene& scene) -> void {
  const auto roots = selection_roots(scene, state.selected_node_ids());

  if (roots.empty()) {
    return;
  }

  state.node_clipboard.clear();

  for (const auto id : roots) {
    state.node_clipboard.push_back(sbx::scenes::scene_serializer::serialize_subtree(scene, scene.find(id)));
  }
}

auto paste_clipboard(editor_state& state, sbx::scenes::scene& scene) -> void {
  if (state.node_clipboard.empty()) {
    return;
  }

  auto location = node_location{.parent_id = std::nullopt, .index = std::numeric_limits<std::size_t>::max()};

  if (auto anchor = state.selected_node(scene); anchor.is_valid()) {
    location = location_of(scene, anchor);
    location.index += 1u;
  }

  auto commands = std::vector<std::unique_ptr<command>>{};

  for (auto offset = std::size_t{0u}; offset < state.node_clipboard.size(); ++offset) {
    const auto index = (location.index == std::numeric_limits<std::size_t>::max()) ? location.index : location.index + offset;
    commands.push_back(std::make_unique<insert_subtree_command>(fresh_copy(state.node_clipboard[offset]), location.parent_id, index, "Paste"));
  }

  push_inserts(state, scene, std::move(commands), "Paste");
}

auto duplicate_selection(editor_state& state, sbx::scenes::scene& scene) -> void {
  auto originals = std::vector<std::pair<sbx::math::uuid, node_location>>{};

  for (const auto id : selection_roots(scene, state.selected_node_ids())) {
    originals.emplace_back(id, location_of(scene, scene.find(id)));
  }

  // Highest index first, so inserts don't shift siblings still waiting to be duplicated.
  std::ranges::sort(originals, std::greater{}, [](const auto& entry) { return entry.second.index; });

  auto commands = std::vector<std::unique_ptr<command>>{};

  for (const auto& [id, location] : originals) {
    auto snapshot = fresh_copy(sbx::scenes::scene_serializer::serialize_subtree(scene, scene.find(id)));
    commands.push_back(std::make_unique<insert_subtree_command>(std::move(snapshot), location.parent_id, location.index + 1u, "Duplicate"));
  }

  push_inserts(state, scene, std::move(commands), "Duplicate");
}

auto delete_selection(editor_state& state, sbx::scenes::scene& scene) -> void {
  auto commands = std::vector<std::unique_ptr<command>>{};

  for (const auto id : selection_roots(scene, state.selected_node_ids())) {
    commands.push_back(std::make_unique<delete_node_command>(scene, scene.find(id)));
  }

  if (commands.empty()) {
    return;
  }

  const auto label = (commands.size() == 1u) ? std::string{"Delete Node"} : fmt::format("Delete {} Nodes", commands.size());

  if (commands.size() == 1u) {
    state.push_command(scene, std::move(commands.front()));
  } else {
    state.push_command(scene, std::make_unique<composite_command>(std::move(commands), label));
  }

  state.clear_selection();
}

auto selection_bounds(editor_state& state, sbx::scenes::scene& scene) -> sbx::math::volume {
  return subtree_bounds(scene, selection_roots(scene, state.selected_node_ids()));
}

auto subtree_bounds(sbx::scenes::scene& scene, std::span<const sbx::math::uuid> roots) -> sbx::math::volume {
  auto bounds = sbx::math::volume{};

  const auto include = [&](this const auto& self, sbx::scenes::node node) -> void {
    const auto& world = node.world_matrix();

    if (node.has_component<sbx::scenes::mesh_renderer>()) {
      if (const auto& renderer = node.get_component<sbx::scenes::mesh_renderer>(); renderer.mesh.is_valid()) {
        bounds.include(sbx::math::volume::transformed(renderer.mesh->bounds(), world));
      }
    }

    bounds.include(sbx::math::vector3{world * sbx::math::vector4{0.0f, 0.0f, 0.0f, 1.0f}});

    for (const auto child : node.get_component<sbx::scenes::relationship>().children) {
      self(scene.node_of(child));
    }
  };

  for (const auto id : roots) {
    if (auto node = scene.find(id); node.is_valid()) {
      include(node);
    }
  }

  return bounds;
}

auto focus_selection(editor_state& state, sbx::scenes::scene& scene) -> void {
  const auto bounds = selection_bounds(state, scene);

  if (bounds.is_empty()) {
    return;
  }

  auto& camera = sbx::core::engine::get_module<editor_module>().editor_camera();

  const auto radius = std::max(bounds.diagonal_length() * 0.5f, 0.5f);
  const auto half_fov = sbx::math::to_radians(sbx::math::degree{camera.params().fov_degrees}).value() * 0.5f;
  const auto forward = camera.transform().rotation * sbx::math::vector3{0.0f, 0.0f, -1.0f};

  camera.transform().position = bounds.center() - forward * (radius / std::sin(half_fov));
}

auto focus_or_follow_selection(editor_state& state, sbx::scenes::scene& scene) -> void {
  // Godot: Focus Selection pressed twice follows the selection until the camera is moved by hand.
  constexpr auto double_press_seconds = 0.5;

  const auto now = ImGui::GetTime();
  const auto is_second_press = now - state.last_focus_time <= double_press_seconds;

  state.last_focus_time = now;

  if (!is_second_press) {
    state.is_following_selection = false;
    focus_selection(state, scene);
    return;
  }

  const auto bounds = selection_bounds(state, scene);

  if (bounds.is_empty()) {
    return;
  }

  const auto& camera = sbx::core::engine::get_module<editor_module>().editor_camera();

  state.is_following_selection = true;
  state.follow_offset = camera.transform().position - bounds.center();
  state.follow_camera_position = camera.transform().position;
}

auto update_selection_follow(editor_state& state, sbx::scenes::scene& scene) -> void {
  if (!state.is_following_selection) {
    return;
  }

  auto& camera = sbx::core::engine::get_module<editor_module>().editor_camera();
  const auto bounds = selection_bounds(state, scene);

  // Moving the camera by hand, or losing the selection, ends the follow.
  if (bounds.is_empty() || camera.transform().position != state.follow_camera_position) {
    state.is_following_selection = false;
    return;
  }

  camera.transform().position = bounds.center() + state.follow_offset;
  state.follow_camera_position = camera.transform().position;
}

auto align_selection_with_view(editor_state& state, sbx::scenes::scene& scene, bool include_position) -> void {
  const auto& camera = sbx::core::engine::get_module<editor_module>().editor_camera();
  const auto& camera_transform = camera.transform();

  auto commands = std::vector<std::unique_ptr<command>>{};

  for (const auto id : selection_roots(scene, state.selected_node_ids())) {
    auto node = scene.find(id);

    if (!node.is_valid()) {
      continue;
    }

    // The node keeps its own world scale (and position, unless aligning the whole transform).
    const auto world = sbx::math::decompose(node.world_matrix());
    const auto target_world = sbx::scenes::local_transform{
      .position = include_position ? camera_transform.position : world.position,
      .rotation = camera_transform.rotation,
      .scale = world.scale
    }.matrix();

    const auto parent = scene.node_of(node.get_component<sbx::scenes::relationship>().parent);
    const auto& parent_inverse = parent.has_component<sbx::scenes::world_transform>() ? parent.get_component<sbx::scenes::world_transform>().inverse : sbx::math::matrix4x4::identity;
    const auto local = sbx::math::decompose(parent_inverse * target_world);

    const auto before = node.get_component<sbx::scenes::local_transform>();
    const auto after = sbx::scenes::local_transform{.position = local.position, .rotation = local.rotation, .scale = local.scale};

    commands.push_back(std::make_unique<modify_component_command<sbx::scenes::local_transform>>(node.id(), before, after, "Align with View"));
  }

  if (!commands.empty()) {
    state.push_command(scene, std::make_unique<composite_command>(std::move(commands), include_position ? "Align Transform with View" : "Align Rotation with View"));
  }
}

} // namespace editor
