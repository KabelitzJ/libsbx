// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene_serializer.hpp>

#include <fstream>

#include <libsbx/utility/logger.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>

namespace sbx::scenes {

auto scene_serializer::save(const std::filesystem::path& path, const std::string& name, scene_graph& graph, scene_asset_table& assets) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto resolved_path = assets_module.resolve_path(path);

  utility::logger<"scenes">::debug("Serializing scene '{}' to {}", name, resolved_path.string());

  auto emitter = YAML::Emitter{};

  emitter << YAML::BeginMap;

  emitter << YAML::Key << "name";
  emitter << YAML::Value << (!name.empty() ? name : "Scene");

  emitter << YAML::Key << "assets";
  emitter << YAML::Value << YAML::BeginMap;

  _save_assets(emitter, assets);

  emitter << YAML::EndMap;

  emitter << YAML::Key << "nodes";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto node : graph.query<const scenes::node>()) {
    _save_node(emitter, graph, assets, node);
  }

  emitter << YAML::EndSeq;

  emitter << YAML::EndMap;

  auto stream = std::ofstream{resolved_path};

  stream << emitter.c_str();

  stream.flush();
  stream.close();
}

auto scene_serializer::load(const std::filesystem::path& path, scene_graph& graph, scene_asset_table& assets) -> std::string {
  const auto scene = YAML::LoadFile(path.string());

  if (!scene || scene.IsNull() || scene.size() == 0) {
    utility::logger<"scenes">::warn("Scene '{}' is empty", path.string());
    return "";
  }

  const auto name = scene["name"].as<std::string>();

  _load_assets(scene["assets"], assets);
  _load_nodes(scene["nodes"], graph, assets);

  return name;
}

auto scene_serializer::_save_assets(YAML::Emitter& emitter, const scene_asset_table& assets) -> void {
  emitter << YAML::Key << "images";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : assets.images()) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  emitter << YAML::Key << "cube_images";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : assets.cube_images()) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  emitter << YAML::Key << "static_meshes";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : assets.meshes()) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();
    emitter << YAML::Key << "source" << YAML::Value << metadata.source;

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;

  emitter << YAML::Key << "materials";
  emitter << YAML::Value << YAML::BeginSeq;

  for (const auto& [id, metadata] : assets.materials()) {
    emitter << YAML::Anchor(metadata.name);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "name" << YAML::Value << metadata.name;
    emitter << YAML::Key << "path" << YAML::Value << metadata.path.string();

    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;
}

auto scene_serializer::_save_node(YAML::Emitter& emitter, scene_graph& graph, scene_asset_table& assets, const scenes::node node) -> void {
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

  _save_components(emitter, graph, assets, node);

  emitter << YAML::EndSeq;

  emitter << YAML::EndMap;
}

auto scene_serializer::_save_components(YAML::Emitter& emitter, scene_graph& graph, scene_asset_table& assets, const scenes::node node) -> void {
  for (auto&& [type, container] : graph.registry().storage()) {
    if (!container.contains(node) || !_component_io.has(type)) {
      continue;
    }

    auto& io = _component_io.get(type);

    emitter << YAML::BeginMap;

    emitter << YAML::Key << "type";
    emitter << YAML::Value << io.name;

    io.save(emitter, graph, assets, node);

    emitter << YAML::EndMap;
  }
}

auto scene_serializer::_load_assets([[maybe_unused]] const YAML::Node& assets, [[maybe_unused]] scene_asset_table& table) -> void {
  // @todo: implement asset loading from YAML
}

auto scene_serializer::_load_nodes([[maybe_unused]] const YAML::Node& nodes, [[maybe_unused]] scene_graph& graph, [[maybe_unused]] scene_asset_table& assets) -> void {
  // @todo: implement node loading from YAML
}

} // namespace sbx::scenes
