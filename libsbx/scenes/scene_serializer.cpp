// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene_serializer.hpp>

#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::scenes {

auto scene_serializer::save(scene& target, const std::filesystem::path& path) -> void {
  auto& registry = target._registry;
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  const auto resolved_path = assets_directory / path;

  // Assets table: collect referenced meshes/materials, assign unique keys
  auto mesh_keys = std::unordered_map<math::uuid, std::string>{};
  auto material_keys = std::unordered_map<math::uuid, std::string>{};
  auto used_keys = std::unordered_set<std::string>{};

  const auto make_key = [&](const std::string& base) {
    auto key = base.empty() ? std::string{"asset"} : base;
    auto suffix = 1;

    while (used_keys.contains(key)) {
      key = fmt::format("{}_{}", base, suffix++);
    }

    used_keys.insert(key);

    return key;
  };

  auto meshes_table = YAML::Node{YAML::NodeType::Sequence};
  auto materials_table = YAML::Node{YAML::NodeType::Sequence};

  for (const auto entity : registry.view<mesh_renderer>()) {
    const auto& renderer = registry.get<mesh_renderer>(entity);

    if (renderer.mesh.is_valid() && !mesh_keys.contains(renderer.mesh->id())) {
      const auto id = renderer.mesh->id();
      const auto asset_path = assets_module.path_of(id);
      const auto key = make_key(asset_path.stem().string());

      mesh_keys.emplace(id, key);

      auto entry = YAML::Node{};
      entry["key"] = key;
      entry["name"] = asset_path.stem().string();
      entry["path"] = asset_path.generic_string();

      meshes_table.push_back(entry);
    }

    for (const auto& material : renderer.materials) {
      if (!material.is_valid()) {
        continue;
      }

      const auto id = material->id();

      if (id == math::uuid::nil()) {
        utility::logger<"scenes">::warn("Skipping a transient material override (no file asset — extract it first)");
        continue;
      }

      if (!material_keys.contains(id)) {
        const auto asset_path = assets_module.path_of(id);
        const auto key = make_key(material->name());

        material_keys.emplace(id, key);

        auto entry = YAML::Node{};
        entry["key"] = key;
        entry["name"] = material->name();
        entry["path"] = asset_path.generic_string();

        materials_table.push_back(entry);
      }
    }
  }

  // Nodes
  auto nodes_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto entity : registry.view<id>()) {
    auto node_yaml = YAML::Node{};

    node_yaml["tag"] = registry.get<tag>(entity).str();
    node_yaml["id"] = registry.get<id>(entity).value();

    const auto& relationship_component = registry.get<relationship>(entity);

    if (relationship_component.parent != ecs::null_entity) {
      node_yaml["parent"] = registry.get<id>(relationship_component.parent).value();
    }

    auto components = YAML::Node{YAML::NodeType::Sequence};

    {
      const auto& transform = registry.get<local_transform>(entity);
  
      auto component = YAML::Node{};
      component["type"] = "transform";
      component["position"] = transform.position;
      component["rotation"] = transform.rotation;
      component["scale"] = transform.scale;

      components.push_back(component);
    }

    if (registry.all_of<mesh_renderer>(entity)) {
      const auto& renderer = registry.get<mesh_renderer>(entity);

      if (renderer.mesh.is_valid()) {
        auto component = YAML::Node{};
        component["type"] = "static_mesh";
        component["mesh"] = mesh_keys.at(renderer.mesh->id());

        auto submeshes = YAML::Node{YAML::NodeType::Sequence};

        for (auto index = std::size_t{0u}; index < renderer.materials.size(); ++index) {
          const auto& material = renderer.materials[index];

          if (material.is_valid() && material->id() != math::uuid::nil()) {
            auto submesh = YAML::Node{};
            submesh["index"] = index;
            submesh["material"] = material_keys.at(material->id());

            submeshes.push_back(submesh);
          }
        }

        if (submeshes.size() > 0u) {
          component["submeshes"] = submeshes;
        }

        components.push_back(component);
      }
    }

    if (registry.all_of<camera>(entity)) {
      const auto& c = registry.get<camera>(entity);

      auto component = YAML::Node{};
      component["type"] = "camera";
      component["fov_degrees"] = c.fov_degrees;
      component["near_plane"] = c.near_plane;
      component["far_plane"] = c.far_plane;

      components.push_back(component);
    }

    if (registry.all_of<directional_light>(entity)) {
      const auto& light = registry.get<directional_light>(entity);

      auto component = YAML::Node{};
      component["type"] = "directional_light";
      component["color"] = light.color;
      component["intensity"] = light.intensity;

      components.push_back(component);
    }

    if (registry.all_of<point_light>(entity)) {
      const auto& light = registry.get<point_light>(entity);
  
      auto component = YAML::Node{};
      component["type"] = "point_light";
      component["color"] = light.color;
      component["intensity"] = light.intensity;
      component["range"] = light.range;

      components.push_back(component);
    }

    if (registry.all_of<spot_light>(entity)) {
      const auto& light = registry.get<spot_light>(entity);

      auto component = YAML::Node{};
      component["type"] = "spot_light";
      component["color"] = light.color;
      component["intensity"] = light.intensity;
      component["range"] = light.range;
      component["inner_angle"] = light.inner_angle;
      component["outer_angle"] = light.outer_angle;

      components.push_back(component);
    }

    node_yaml["components"] = components;
    nodes_node.push_back(node_yaml);
  }

  // Metadata
  auto metadata = YAML::Node{};

  metadata["name"] = target.name();

  if (target._active_camera != ecs::null_entity) {
    metadata["camera"] = registry.get<id>(target._active_camera).value();
  }

  if (target._primary_light != ecs::null_entity) {
    metadata["primary_light"] = registry.get<id>(target._primary_light).value();
  }

  auto assets_node = YAML::Node{};
  assets_node["static_meshes"] = meshes_table;
  assets_node["materials"] = materials_table;

  auto root = YAML::Node{};
  root["metadata"] = metadata;
  root["assets_module"] = assets_node;
  root["nodes"] = nodes_node;

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << root;

  utility::logger<"scenes">::info("Saved scene '{}' ({} nodes)", resolved_path.generic_string(), nodes_node.size());
}

auto scene_serializer::load(scene& target, const std::filesystem::path& path) -> void {
  auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  const auto resolved_path = assets_directory / path;

  if (!std::filesystem::exists(resolved_path)) {
    utility::logger<"scenes">::warn("Scene '{}' does not exist", resolved_path.generic_string());
    return;
  }

  const auto root = YAML::LoadFile(resolved_path.string());

  target._registry.clear();
  target._entities_by_id.clear();
  target._active_camera = ecs::null_entity;
  target._primary_light = ecs::null_entity;

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  if (const auto metadata = root["metadata"]; metadata && metadata["name"]) {
    target.set_name(metadata["name"].as<std::string>());
  }

  // Asset table: key -> path
  auto key_to_path = std::unordered_map<std::string, std::filesystem::path>{};

  const auto register_category = [&](const char* category) {
    if (const auto sequence = root["assets_module"][category]) {
      for (const auto entry : sequence) {
        key_to_path.emplace(entry["key"].as<std::string>(), std::filesystem::path{entry["path"].as<std::string>()});
      }
    }
  };

  register_category("static_meshes");
  register_category("materials");

  const auto nodes_node = root["nodes"];

  // Pass 1: create every node with its id (so parent/reference ids resolve).
  for (const auto node_yaml : nodes_node) {
    target._create_node(node_yaml["tag"].as<std::string>(), local_transform{}, node_yaml["id"].as<math::uuid>());
  }

  // Pass 2: tag, parent, components.
  for (const auto node_yaml : nodes_node) {
    auto node = target.find(node_yaml["id"].as<math::uuid>());

    if (const auto parent = node_yaml["parent"]) {
      node.set_parent(target.find(parent.as<math::uuid>()));
    }

    for (const auto component : node_yaml["components"]) {
      const auto type = component["type"].as<std::string>();

      if (type == "transform") {
        auto& transform = node.transform();
        transform.position = component["position"].as<math::vector3f>();
        transform.rotation = component["rotation"].as<math::quaternion>();
        transform.scale = component["scale"].as<math::vector3f>();
      } else if (type == "static_mesh") {
        auto& renderer = node.add_component<mesh_renderer>();
        renderer.mesh = assets_module.load_mesh(key_to_path.at(component["mesh"].as<std::string>()));

        if (const auto submeshes = component["submeshes"]) {
          for (const auto submesh : submeshes) {
            const auto index = submesh["index"].as<std::size_t>();

            if (renderer.materials.size() <= index) {
              renderer.materials.resize(index + 1u);
            }

            renderer.materials[index] = assets_module.load_material(key_to_path.at(submesh["material"].as<std::string>()));
          }
        }
      } else if (type == "camera") {
        auto& c = node.add_component<camera>();
        c.fov_degrees = component["fov_degrees"].as<std::float_t>();
        c.near_plane = component["near_plane"].as<std::float_t>();
        c.far_plane = component["far_plane"].as<std::float_t>();
      } else if (type == "directional_light") {
        auto& light = node.add_component<directional_light>();
        light.color = component["color"].as<math::color>();
        light.intensity = component["intensity"].as<std::float_t>();
      } else if (type == "point_light") {
        auto& light = node.add_component<point_light>();
        light.color = component["color"].as<math::color>();
        light.intensity = component["intensity"].as<std::float_t>();
        light.range = component["range"].as<std::float_t>();
      } else if (type == "spot_light") {
        auto& light = node.add_component<spot_light>();
        light.color = component["color"].as<math::color>();
        light.intensity = component["intensity"].as<std::float_t>();
        light.range = component["range"].as<std::float_t>();
        light.inner_angle = component["inner_angle"].as<std::float_t>();
        light.outer_angle = component["outer_angle"].as<std::float_t>();
      } else {
        utility::logger<"scenes">::warn("Unknown component type '{}'", type);
      }
    }
  }

  if (const auto metadata = root["metadata"]) {
    if (const auto camera = metadata["camera"]) {
      target.set_active_camera(target.find(camera.as<math::uuid>()));
    }

    if (const auto primary_light = metadata["primary_light"]) {
      target.set_primary_light(target.find(primary_light.as<math::uuid>()));
    }
  }

  utility::logger<"scenes">::info("Loaded scene '{}' ({} nodes)", path.generic_string(), nodes_node.size());
}

} // namespace sbx::scenes
