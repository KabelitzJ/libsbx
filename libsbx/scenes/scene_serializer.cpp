// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene_serializer.hpp>

#include <fstream>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::scenes {

auto scene_serializer::save(scene& target, const std::filesystem::path& path) -> void {
  auto& registry = target._registry;

  auto nodes_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto entity : registry.view<id>()) {
    auto node_yaml = YAML::Node{};

    node_yaml["id"] = registry.get<id>(entity).value();
    node_yaml["tag"] = registry.get<tag>(entity).str();

    const auto& relationship_component = registry.get<relationship>(entity);

    if (relationship_component.parent != ecs::null_entity) {
      node_yaml["parent"] = registry.get<id>(relationship_component.parent).value();
    }

    const auto& transform = registry.get<local_transform>(entity);
    node_yaml["transform"]["position"] = transform.position;
    node_yaml["transform"]["rotation"] = transform.rotation;
    node_yaml["transform"]["scale"] = transform.scale;

    if (registry.all_of<camera>(entity)) {
      const auto& c = registry.get<camera>(entity);
      node_yaml["camera"]["fov_degrees"] = c.fov_degrees;
      node_yaml["camera"]["near_plane"] = c.near_plane;
      node_yaml["camera"]["far_plane"] = c.far_plane;
    }

    if (registry.all_of<mesh_renderer>(entity)) {
      const auto& renderer = registry.get<mesh_renderer>(entity);
      if (renderer.mesh.is_valid()) {
        node_yaml["mesh_renderer"]["mesh"] = renderer.mesh->id();
      }
    }

    if (registry.all_of<directional_light>(entity)) {
      const auto& light = registry.get<directional_light>(entity);
      node_yaml["directional_light"]["color"] = light.color;
      node_yaml["directional_light"]["intensity"] = light.intensity;
    }

    if (registry.all_of<point_light>(entity)) {
      const auto& light = registry.get<point_light>(entity);
      node_yaml["point_light"]["color"] = light.color;
      node_yaml["point_light"]["intensity"] = light.intensity;
      node_yaml["point_light"]["range"] = light.range;
    }

    if (registry.all_of<spot_light>(entity)) {
      const auto& light = registry.get<spot_light>(entity);
      node_yaml["spot_light"]["color"] = light.color;
      node_yaml["spot_light"]["intensity"] = light.intensity;
      node_yaml["spot_light"]["range"] = light.range;
      node_yaml["spot_light"]["inner_angle"] = light.inner_angle;
      node_yaml["spot_light"]["outer_angle"] = light.outer_angle;
    }

    nodes_node.push_back(node_yaml);
  }

  auto root = YAML::Node{};
  root["nodes"] = nodes_node;

  if (target._active_camera != ecs::null_entity) {
    root["active_camera"] = registry.get<id>(target._active_camera).value();
  }

  if (target._primary_light != ecs::null_entity) {
    root["primary_light"] = registry.get<id>(target._primary_light).value();
  }

  auto out = std::ofstream{path};
  out << root;

  utility::logger<"scenes">::info("Saved scene '{}' ({} nodes)", path.generic_string(), nodes_node.size());
}

auto scene_serializer::load(scene& target, const std::filesystem::path& path) -> void {
  if (!std::filesystem::exists(path)) {
    utility::logger<"scenes">::warn("Scene '{}' does not exist", path.generic_string());
    return;
  }

  const auto root = YAML::LoadFile(path.string());

  target._registry.clear();
  target._entities_by_id.clear();
  target._active_camera = ecs::null_entity;
  target._primary_light = ecs::null_entity;

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto nodes_node = root["nodes"];

  // Pass 1: create all nodes with their stable ids.
  for (const auto node_yaml : nodes_node) {
    target._create_node("", {}, node_yaml["id"].as<math::uuid>());
  }

  // Pass 2: fill data + wire parents (all ids now resolvable).
  for (const auto node_yaml : nodes_node) {
    auto node = target.find(node_yaml["id"].as<math::uuid>());

    if (const auto name_node = node_yaml["name"]) {
      node.name() = name_node.as<std::string>();
    }

    if (const auto parent_node = node_yaml["parent"]) {
      node.set_parent(target.find(parent_node.as<math::uuid>()));
    }

    if (const auto transform_node = node_yaml["transform"]) {
      auto& transform = node.transform();
      transform.position = transform_node["position"].as<math::vector3f>();
      transform.rotation = transform_node["rotation"].as<math::quaternion>();
      transform.scale = transform_node["scale"].as<math::vector3f>();
    }

    if (const auto camera_node = node_yaml["camera"]) {
      auto& c = node.add_component<camera>();
      c.fov_degrees = camera_node["fov_degrees"].as<std::float_t>();
      c.near_plane = camera_node["near_plane"].as<std::float_t>();
      c.far_plane = camera_node["far_plane"].as<std::float_t>();
    }

    if (const auto renderer_node = node_yaml["mesh_renderer"]) {
      auto& renderer = node.add_component<mesh_renderer>();
      if (const auto mesh_node = renderer_node["mesh"]) {
        renderer.mesh = assets_module.load_mesh(mesh_node.as<math::uuid>());
      }
    }

    if (const auto light_node = node_yaml["directional_light"]) {
      auto& light = node.add_component<directional_light>();
      light.color = light_node["color"].as<math::color>();
      light.intensity = light_node["intensity"].as<std::float_t>();
    }

    if (const auto light_node = node_yaml["point_light"]) {
      auto& light = node.add_component<point_light>();
      light.color = light_node["color"].as<math::color>();
      light.intensity = light_node["intensity"].as<std::float_t>();
      light.range = light_node["range"].as<std::float_t>();
    }

    if (const auto light_node = node_yaml["spot_light"]) {
      auto& light = node.add_component<spot_light>();
      light.color = light_node["color"].as<math::color>();
      light.intensity = light_node["intensity"].as<std::float_t>();
      light.range = light_node["range"].as<std::float_t>();
      light.inner_angle = light_node["inner_angle"].as<std::float_t>();
      light.outer_angle = light_node["outer_angle"].as<std::float_t>();
    }
  }

  if (const auto active_camera_node = root["active_camera"]) {
    target.set_active_camera(target.find(active_camera_node.as<math::uuid>()));
  }

  if (const auto primary_light_node = root["primary_light"]) {
    target.set_primary_light(target.find(primary_light_node.as<math::uuid>()));
  }

  utility::logger<"scenes">::info("Loaded scene '{}' ({} nodes)", path.generic_string(), nodes_node.size());
}

} // namespace sbx::scenes
