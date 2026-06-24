// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene.hpp>

#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/component_serializer.hpp>

#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>
#include <libsbx/scenes/components/transform.hpp>

namespace sbx::scenes {

scene::scene(const std::filesystem::path& path)
: _name{"Scene"},
  _graph{},
  _environment{_graph, scenes::node::null, math::vector3{-1.0f, -1.0f, -1.0f}, math::color{1.0f, 0.92f, 0.78f, 8.0f}} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  const auto& serializer = scenes_module.component_serializer();

  if (std::filesystem::exists(path)) {
    const auto root_node = YAML::LoadFile(path.string());

    _name = root_node["name"].as<std::string>("Scene");

    if (const auto assets_node = root_node["assets"]; assets_node) {
      for (const auto& asset_node : assets_node) {
        assets_module.load_asset(asset_node["source"].as<std::string>());
      }
    }

    auto node_by_id = std::unordered_map<math::uuid, scenes::node>{};
    auto pending_parents = std::vector<std::pair<scenes::node, math::uuid>>{};

    if (const auto nodes_node = root_node["nodes"]; nodes_node) {
      for (const auto& node_node : nodes_node) {
        const auto id = node_node["id"].as<math::uuid>();

        auto node = _graph.create_node(node_node["tag"].as<std::string>("Node"));

        _graph.reassign_node_id(node, id);

        node_by_id.emplace(id, node);

        if (const auto transform_node = node_node["transform"]; transform_node) {
          auto& transform = _graph.get_component<scenes::transform>(node);

          transform.set_position(transform_node["position"].as<math::vector3>());
          transform.set_rotation(transform_node["rotation"].as<math::quaternion>());
          transform.set_scale(transform_node["scale"].as<math::vector3>());
        }

        if (const auto components_node = node_node["components"]; components_node) {
          serializer.deserialize(components_node, _graph, node);
        }

        if (const auto parent_node = node_node["parent"]; parent_node) {
          pending_parents.emplace_back(node, parent_node.as<math::uuid>());
        }
      }
    }

    for (const auto& [node, parent_id] : pending_parents) {
      if (const auto entry = node_by_id.find(parent_id); entry != node_by_id.end()) {
        _graph.make_child_of(node, entry->second);
      }
    }
  }

  auto camera_node = scenes::node::null;

  if (auto view = _graph.query<scenes::camera>(); view.begin() != view.end()) {
    camera_node = *view.begin();
  }

  if (camera_node == scenes::node::null) {
    camera_node = _graph.create_node("CAMERA");

    utility::logger<"scenes">::debug("No camera node found, creating default");
  }

  if (!_graph.has_component<scenes::camera>(camera_node)) {
    _graph.add_component<scenes::camera>(camera_node, math::angle{math::degree{60.0f}}, 0.1f, 1000.0f);
  }

  _environment.set_active_camera(camera_node);
}

scene::scene(const std::string& name)
: _name{name},
  _graph{},
  _environment{_graph, scenes::node::null, math::vector3{-1.0f, -1.0f, -1.0f}, math::color{1.0f, 1.0f, 1.0f, 1.0f}} {
  auto camera_node = _graph.create_node("Camera");

  _graph.add_component<scenes::camera>(camera_node, math::angle{math::degree{60.0f}}, 0.1f, 1000.0f);

  _environment.set_active_camera(camera_node);
}

auto scene::save(const std::filesystem::path& path) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  const auto& serializer = scenes_module.component_serializer();

  auto assets = component_serializer::asset_set{};
  auto nodes_node = YAML::Node{};

  const auto root = _graph.root();

  for (const auto node : _graph.query<scenes::id>()) {
    if (node == root) {
      continue;
    }

    auto node_node = YAML::Node{};

    node_node["id"] = math::uuid{_graph.get_component<scenes::id>(node)};
    node_node["tag"] = fmt::format("{}", _graph.get_component<scenes::tag>(node));

    if (const auto parent = _graph.get_component<scenes::relationship>(node).parent(); parent != scenes::node::null && parent != root) {
      node_node["parent"] = math::uuid{_graph.get_component<scenes::id>(parent)};
    }

    const auto& transform = _graph.get_component<scenes::transform>(node);

    auto transform_node = YAML::Node{};

    transform_node["position"] = transform.position();
    transform_node["rotation"] = transform.rotation();
    transform_node["scale"] = transform.scale();

    node_node["transform"] = transform_node;

    auto components_node = YAML::Node{};

    serializer.serialize(components_node, _graph, node, assets);

    if (components_node.size() > 0u) {
      node_node["components"] = components_node;
    }

    nodes_node.push_back(node_node);
  }

  auto assets_node = YAML::Node{};

  for (const auto& id : assets) {
    auto source = assets_module.source_of(id);

    if (!source || source->empty()) {
      const auto target = fmt::format("res://materials/{}/{}.material.yaml", _name, id);

      if (!assets_module.save_asset(id, target)) {
        utility::logger<"scenes">::warn("Skipping runtime asset '{}' (no writable serializer)", id);
        continue;
      }

      source = assets_module.source_of(id);
    }

    auto asset_node = YAML::Node{};
    asset_node["id"] = id;
    asset_node["source"] = source->generic_string();
    assets_node.push_back(asset_node);
  }

  auto root_node = YAML::Node{};

  root_node["name"] = _name;
  root_node["assets"] = assets_node;
  root_node["nodes"] = nodes_node;

  auto file = std::ofstream{path};

  file << YAML::Dump(root_node);
}

} // namespace sbx::scenes
