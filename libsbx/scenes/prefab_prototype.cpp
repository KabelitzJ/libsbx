// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/prefab_prototype.hpp>

#include <algorithm>
#include <utility>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/profiler.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/scenes/scene_serializer.hpp>

namespace sbx::scenes {

auto remap_prefab_node_references(node& instance_node, const std::unordered_map<math::uuid, math::uuid>& remap) -> void {
  const auto remapped = [&remap](math::uuid& id) {
    if (const auto entry = remap.find(id); entry != remap.end()) {
      id = entry->second;
    }
  };

  if (auto screen = instance_node.try_get_component<canvas::canvas>()) {
    remapped(screen->camera);
  }

  if (auto toggle = instance_node.try_get_component<canvas::ui_toggle>()) {
    remapped(toggle->group);
  }

  if (auto scroll_rect = instance_node.try_get_component<canvas::ui_scroll_rect>()) {
    remapped(scroll_rect->content);
  }

  if (auto scripts = instance_node.try_get_component<script_component>()) {
    for (auto& entry : scripts->scripts) {
      for (auto& field : entry.field_overrides) {
        if (field.type == script_field_type::node) {
          remapped(field.node_value);
        }
      }
    }
  }
}

auto uncopied_prefab_component_keys(const YAML::Node& node_yaml) -> std::vector<std::string> {
  auto uncopied = std::vector<std::string>{};

  for (const auto component : node_yaml["components"]) {
    const auto type = component["type"].as<std::string>("");

    if (!std::ranges::contains(prefab_copied_component_keys, std::string_view{type})) {
      uncopied.push_back(type);
    }
  }

  return uncopied;
}

auto check_prefab_copy_coverage(std::string_view prefab_name, const YAML::Node& snapshot) -> bool {
  auto covered = true;

  for (const auto node_yaml : snapshot["nodes"]) {
    for (const auto& type : uncopied_prefab_component_keys(node_yaml)) {
      utility::logger<"scenes">::error("Prefab '{}': component '{}' isn't in prefab_copied_components, so instances won't get it (add it to libsbx/scenes/prefab_prototype.hpp)", prefab_name, type);
      covered = false;
    }
  }

  return covered;
}

auto prefab_prototype_cache::prototype_of(const assets::prefab_handle& prefab) -> const prefab_prototype& {
  static const auto none = prefab_prototype{};

  if (!prefab.is_valid()) {
    return none;
  }

  auto& prototype = _prototypes[prefab.get()];

  // A recycled address (the old record is gone) or an edited prefab both rebuild.
  if (prototype.source.expired() || prototype.generation != prefab.generation() || prototype.nodes_scene == nullptr) {
    prototype = _build(prefab);
  }

  return prototype;
}

auto prefab_prototype_cache::prune() -> void {
  std::erase_if(_prototypes, [](const auto& entry) { return entry.second.source.expired(); });
}

auto prefab_prototype_cache::clear() -> void {
  _prototypes.clear();
}

auto prefab_prototype_cache::_build(const assets::prefab_handle& prefab) -> prefab_prototype {
  SBX_PROFILE_SCOPE("prefab_prototype_cache::build");

  auto prototype = prefab_prototype{};
  prototype.source = prefab.shared();
  prototype.generation = prefab.generation();

  if (!prefab->snapshot()["nodes"] || prefab->snapshot()["nodes"].size() == 0u) {
    return prototype;
  }

  check_prefab_copy_coverage(prefab->name(), prefab->snapshot());

  prototype.nodes_scene = std::make_unique<scene>("Prefab Prototype");

  auto snapshot_to_prototype = std::unordered_map<math::uuid, math::uuid>{};
  auto root = scene_serializer::instantiate_prefab(*prototype.nodes_scene, prefab, std::nullopt, memory::make_observer(snapshot_to_prototype));

  if (!root.is_valid()) {
    prototype.nodes_scene.reset();
    return prototype;
  }

  prototype.root_id = root.id();

  // The root's marker would hold the prefab itself, keeping it alive through its own prototype; each instance gets a fresh one anyway.
  static_cast<void>(root.remove_component<prefab_instance>());

  // Depth first, so instancing can parent each node to one it already created.
  auto pending = std::vector<node>{root};

  while (!pending.empty()) {
    auto current = pending.back();
    pending.pop_back();

    prototype.node_ids.push_back(current.id());

    // The YAML path leaves references between the prefab's nodes on snapshot ids; pointed at the prototype's own nodes here, instancing can remap them like every other id.
    remap_prefab_node_references(current, snapshot_to_prototype);

    const auto& children = current.get_component<relationship>().children;

    for (auto child = children.rbegin(); child != children.rend(); ++child) {
      pending.push_back(prototype.nodes_scene->node_of(*child));
    }
  }

  return prototype;
}

auto instantiate_prefab_prototype(scene& target, const assets::prefab_handle& prefab, prefab_prototype_cache& cache, std::optional<math::uuid> root_id) -> node {
  SBX_PROFILE_SCOPE("instantiate_prefab_prototype");

  const auto& prototype = cache.prototype_of(prefab);

  if (prototype.nodes_scene == nullptr) {
    return node{};
  }

  auto& source = *prototype.nodes_scene;

  // Prototype id -> instance id for every node, so parents and references resolve whatever order they're met in.
  auto remap = std::unordered_map<math::uuid, math::uuid>{};
  remap.reserve(prototype.node_ids.size());

  for (const auto& id : prototype.node_ids) {
    remap.emplace(id, (id == prototype.root_id && root_id) ? *root_id : math::uuid::create());
  }

  auto created = std::unordered_map<math::uuid, node>{};
  created.reserve(prototype.node_ids.size());

  // Parents come first in node_ids, so each node's parent already exists.
  for (const auto& id : prototype.node_ids) {
    auto from = source.find(id);
    auto copy = target.create_node(from.name(), local_transform{}, remap.at(id));

    copy_prefab_components(from, copy);

    if (id != prototype.root_id) {
      const auto parent = source.node_of(from.get_component<relationship>().parent);

      copy.set_parent(created.at(parent.id()));
    }

    created.emplace(id, copy);
  }

  for (auto& [id, copy] : created) {
    remap_prefab_node_references(copy, remap);
  }

  auto root = created.at(prototype.root_id);

  root.add_component<prefab_instance>(prefab, prefab.generation());

  return root;
}

} // namespace sbx::scenes
