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

#include <yaml-cpp/yaml.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <editor/editor_module.hpp>

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

// A node that belongs to a prefab instance whose root isn't part of the copy would come out as a
// second member with the same member_id -- keep prefab membership only where the copy contains the
// instance root itself.
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

  // Highest index first, so an insert never shifts the position of a sibling that's still waiting to be duplicated.
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

auto focus_selection(editor_state& state, sbx::scenes::scene& scene) -> void {
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

  for (const auto id : selection_roots(scene, state.selected_node_ids())) {
    include(scene.find(id));
  }

  if (bounds.is_empty()) {
    return;
  }

  auto& camera = sbx::core::engine::get_module<editor_module>().editor_camera();

  const auto radius = std::max(bounds.diagonal_length() * 0.5f, 0.5f);
  const auto half_fov = sbx::math::to_radians(sbx::math::degree{camera.params().fov_degrees}).value() * 0.5f;
  const auto forward = camera.transform().rotation * sbx::math::vector3{0.0f, 0.0f, -1.0f};

  camera.transform().position = bounds.center() - forward * (radius / std::sin(half_fov));
}

} // namespace editor
