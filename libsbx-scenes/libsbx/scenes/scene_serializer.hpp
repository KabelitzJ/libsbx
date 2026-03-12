// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
#define LIBSBX_SCENES_SCENE_SERIALIZER_HPP_

#include <filesystem>
#include <string>

#include <yaml-cpp/yaml.h>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene_graph.hpp>
#include <libsbx/scenes/scene_asset_table.hpp>
#include <libsbx/scenes/component_io.hpp>

namespace sbx::scenes {

class scene_serializer {

public:

  explicit scene_serializer(component_io_registry& component_io)
  : _component_io{component_io} { }

  auto save(const std::filesystem::path& path, const std::string& name, scene_graph& graph, scene_asset_table& assets) -> void;

  auto load(const std::filesystem::path& path, scene_graph& graph, scene_asset_table& assets) -> std::string;

private:

  auto _save_assets(YAML::Emitter& emitter, const scene_asset_table& assets) -> void;

  auto _save_node(YAML::Emitter& emitter, scene_graph& graph, scene_asset_table& assets, const scenes::node node) -> void;

  auto _save_components(YAML::Emitter& emitter, scene_graph& graph, scene_asset_table& assets, const scenes::node node) -> void;

  auto _load_assets(const YAML::Node& assets, scene_asset_table& table) -> void;

  auto _load_nodes(const YAML::Node& nodes, scene_graph& graph, scene_asset_table& assets) -> void;

  component_io_registry& _component_io;

}; // class scene_serializer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
