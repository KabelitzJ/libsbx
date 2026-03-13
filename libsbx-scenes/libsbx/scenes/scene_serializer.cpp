// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene_serializer.hpp>

#include <fstream>
#include <unordered_set>

#include <libsbx/utility/logger.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>

namespace sbx::scenes {

// --- Save ---

auto scene_serializer::save(const std::filesystem::path& path, const std::string& name, scene_graph& graph, const scenes::node camera) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto resolved_path = assets_module.resolve_path(path);

  utility::logger<"scenes">::debug("Serializing scene '{}' to {}", name, resolved_path.string());

  auto emitter = YAML::Emitter{};

  emitter << YAML::BeginMap;

  // Metadata
  emitter << YAML::Key << "metadata";
  emitter << YAML::Value << YAML::BeginMap;

  emitter << YAML::Key << "name" << YAML::Value << (!name.empty() ? name : "Scene");

  if (camera != scenes::node::null && graph.has_component<scenes::id>(camera)) {
    emitter << YAML::Key << "camera" << YAML::Value << graph.get_component<scenes::id>(camera).value();
  }

  emitter << YAML::EndMap;

  // Assets (dependency manifest)
  emitter << YAML::Key << "assets";
  emitter << YAML::Value << YAML::BeginMap;

  _save_assets(emitter);

  emitter << YAML::EndMap;

  // Nodes
  emitter << YAML::Key << "nodes";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto node : graph.query<const scenes::id>()) {
    _save_node(emitter, graph, node);
  }

  emitter << YAML::EndSeq;

  emitter << YAML::EndMap;

  auto stream = std::ofstream{resolved_path};

  stream << emitter.c_str();

  stream.flush();
  stream.close();
}

auto scene_serializer::_save_assets(YAML::Emitter& emitter) -> void {
  auto written = std::unordered_set<std::string>{};

  emitter << YAML::Key << "images";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [handle, metadata] : _registry.images()) {
    if (!written.insert(metadata.path.string()).second) {
      continue;
    }

    emitter << YAML::Anchor(metadata.name);
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  written.clear();

  emitter << YAML::Key << "cube_images";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [handle, metadata] : _registry.cube_images()) {
    if (!written.insert(metadata.path.string()).second) {
      continue;
    }

    emitter << YAML::Anchor(metadata.name);
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  written.clear();

  emitter << YAML::Key << "static_meshes";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [handle, metadata] : _registry.meshes()) {
    if (!written.insert(metadata.path.string()).second) {
      continue;
    }

    emitter << YAML::Anchor(metadata.name);
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();
    emitter << YAML::Key << "source" << YAML::Value << metadata.source;
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  written.clear();

  emitter << YAML::Key << "materials";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [handle, metadata] : _registry.materials()) {
    if (!written.insert(metadata.name).second) {
      continue;
    }

    emitter << YAML::Anchor(metadata.name);
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;
}

auto scene_serializer::_save_node(YAML::Emitter& emitter, scene_graph& graph, const scenes::node node) -> void {
  if (node == graph.root()) {
    return;
  }

  emitter << YAML::BeginMap;

  const auto& tag = graph.get_component<scenes::tag>(node);

  emitter << YAML::Key << "tag";
  emitter << YAML::Value << tag.str();

  const auto& id = graph.get_component<scenes::id>(node);

  emitter << YAML::Key << "id";
  emitter << YAML::Value << id.value();

  const auto& relationship = graph.get_component<scenes::relationship>(node);

  if (relationship.parent() != graph.root()) {
    emitter << YAML::Key << "parent";
    emitter << YAML::Value << graph.get_component<scenes::id>(relationship.parent()).value();
  }

  emitter << YAML::Key << "components";
  emitter << YAML::Value << YAML::BeginSeq;

  _save_components(emitter, graph, node);

  emitter << YAML::EndSeq;

  emitter << YAML::EndMap;
}

auto scene_serializer::_save_components(YAML::Emitter& emitter, scene_graph& graph, const scenes::node node) -> void {
  for (auto&& [type, container] : graph.registry().storage()) {
    if (!container.contains(node) || !_component_io.has(type)) {
      continue;
    }

    auto& io = _component_io.get(type);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "type";
    emitter << YAML::Value << io.name;

    io.save(emitter, graph, _registry, node);

    emitter << YAML::EndMap;
  }
}

// --- Load ---

auto scene_serializer::load(const std::filesystem::path& path, scene_graph& graph) -> scene_data {
  const auto scene = YAML::LoadFile(path.string());

  if (!scene || scene.IsNull() || scene.size() == 0) {
    utility::logger<"scenes">::warn("Scene '{}' is empty", path.string());
    return {};
  }

  auto result = scene_data{};

  const auto& metadata = scene["metadata"] ? scene["metadata"] : scene;

  result.name = metadata["name"].as<std::string>("Scene");

  if (metadata["camera"]) {
    result.camera_id = metadata["camera"].as<math::uuid>();
  }

  if (scene["assets"]) {
    _load_assets(scene["assets"]);
  }

  if (scene["nodes"]) {
    _load_nodes(scene["nodes"], graph);
  }

  return result;
}

auto scene_serializer::_load_assets(const YAML::Node& assets_node) -> void {
  static constexpr auto category_keys = std::array<std::pair<const char*, const char*>, 4u>{{
    {"images", "images"},
    {"cube_images", "cube_images"},
    {"static_meshes", "static_meshes"},
    {"materials", "materials"}
  }};

  for (const auto& [yaml_key, category] : category_keys) {
    if (assets_node[yaml_key]) {
      _load_asset_category(category, assets_node[yaml_key]);
    }
  }
}

auto scene_serializer::_load_asset_category(const std::string& category, const YAML::Node& entries) -> void {
  if (!entries.IsSequence()) {
    return;
  }

  if (!_asset_io.has(category)) {
    utility::logger<"scenes">::warn("No asset loader registered for category '{}'", category);
    return;
  }

  for (const auto& entry : entries) {
    const auto name = entry["name"].as<std::string>();

    _asset_io.load(category, _registry, utility::hashed_string{name}, entry);
  }
}

auto scene_serializer::_load_nodes(const YAML::Node& nodes_node, scene_graph& graph) -> void {
  if (!nodes_node.IsSequence()) {
    return;
  }

  // Pass 1: Create all nodes
  auto id_to_node = std::unordered_map<std::uint64_t, scenes::node>{};
  auto node_yaml = std::vector<std::pair<scenes::node, YAML::Node>>{};

  for (const auto& entry : nodes_node) {
    const auto tag = entry["tag"].as<std::string>("Node");

    auto node = graph.create_node(tag);

    if (entry["id"]) {
      const auto yaml_id = math::uuid::from_value(entry["id"].as<std::uint64_t>());

      graph.reassign_node_id(node, yaml_id);
      id_to_node[entry["id"].as<std::uint64_t>()] = node;
    }

    node_yaml.emplace_back(node, entry);
  }

  // Pass 2: Reparent
  for (const auto& [node, entry] : node_yaml) {
    if (!entry["parent"]) {
      continue;
    }

    const auto parent_id = entry["parent"].as<std::uint64_t>();

    if (auto it = id_to_node.find(parent_id); it != id_to_node.end()) {
      graph.make_child_of(node, it->second);
    } else {
      utility::logger<"scenes">::warn("Node '{}' references unknown parent id {}", graph.get_component<scenes::tag>(node).str(), parent_id);
    }
  }

  // Pass 3: Components
  for (const auto& [node, entry] : node_yaml) {
    if (!entry["components"] || !entry["components"].IsSequence()) {
      continue;
    }

    for (const auto& component_node : entry["components"]) {
      const auto type = component_node["type"].as<std::string>();

      if (!_component_io.has(type)) {
        utility::logger<"scenes">::warn("No component loader registered for type '{}'", type);
        continue;
      }

      auto& io = _component_io.get(type);

      io.load(component_node, graph, _registry, node);
    }
  }
}

} // namespace sbx::scenes
