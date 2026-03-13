// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
#define LIBSBX_SCENES_SCENE_SERIALIZER_HPP_

#include <filesystem>
#include <string>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene_graph.hpp>
#include <libsbx/scenes/asset_registry.hpp>
#include <libsbx/scenes/component_io.hpp>
#include <libsbx/scenes/asset_io.hpp>

namespace sbx::scenes {

struct scene_data {
  std::string name;
  math::uuid camera_id;
}; // struct scene_data

class scene_serializer {

public:

  scene_serializer(component_io_registry& component_io, asset_io_registry& asset_io, asset_registry& registry)
  : _component_io{component_io},
    _asset_io{asset_io},
    _registry{registry} { }

  auto save(const std::filesystem::path& path, const std::string& name, scene_graph& graph, const scenes::node camera) -> void;

  auto load(const std::filesystem::path& path, scene_graph& graph) -> scene_data;

private:

  auto _save_assets(YAML::Emitter& emitter) -> void;

  auto _save_node(YAML::Emitter& emitter, scene_graph& graph, const scenes::node node) -> void;

  auto _save_components(YAML::Emitter& emitter, scene_graph& graph, const scenes::node node) -> void;

  auto _load_assets(const YAML::Node& assets_node) -> void;

  auto _load_asset_category(const std::string& category, const YAML::Node& entries) -> void;

  auto _load_nodes(const YAML::Node& nodes_node, scene_graph& graph) -> void;

  component_io_registry& _component_io;
  asset_io_registry& _asset_io;
  asset_registry& _registry;

}; // class scene_serializer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
