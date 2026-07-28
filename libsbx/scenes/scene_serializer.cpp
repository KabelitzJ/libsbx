// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene_serializer.hpp>

#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <concepts>
#include <type_traits>
#include <meta>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/reflection/enum.hpp>
#include <libsbx/reflection/struct.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::scenes {

consteval auto display_name_of(std::meta::info member) -> std::string_view {
  auto renames = std::meta::annotations_of_with_type(member, ^^reflection::detail::rename);

  return (!renames.empty()) ? std::meta::extract<reflection::detail::rename>(renames.front()).view() : std::meta::identifier_of(member);
}

consteval auto type_name_of(std::meta::info type) -> std::string_view {
  return display_name_of(type);
}

consteval auto serializable_components_of(std::meta::info namespace_reflection) -> std::vector<std::meta::info> {
  auto result = std::vector<std::meta::info>{};

  for (auto member : std::meta::members_of(namespace_reflection, std::meta::access_context::unchecked())) {
    if (std::meta::is_type(member) && !std::meta::annotations_of_with_type(member, ^^reflection::detail::serializable).empty()) {
      result.push_back(member);
    }
  }

  return result;
}

template<typename Component>
auto serialize_component(const Component& value) -> YAML::Node {
  auto node = YAML::Node{};

  template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^Component, std::meta::access_context::unchecked()))) {
    if constexpr (std::meta::annotations_of_with_type(member, ^^reflection::detail::skip).empty()) {
      const auto name = std::string{display_name_of(member)};

      using field_type = std::remove_cvref_t<typename [:std::meta::type_of(member):]>;

      if constexpr (reflection::named_enum<field_type>) {
        const auto& entry = value.[:member:];

        node[name] = std::string{reflection::to_string(entry)};
      } else {
        node[name] = value.[:member:];
      }
    }
  }

  return node;
}

template<typename Component>
auto deserialize_component(const YAML::Node& node) -> Component {
  auto value = Component{};

  template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^Component, std::meta::access_context::unchecked()))) {
    if constexpr (std::meta::annotations_of_with_type(member, ^^reflection::detail::skip).empty()) {
      if (const auto field = node[std::string{display_name_of(member)}]) {
        using field_type = std::remove_cvref_t<typename [:std::meta::type_of(member):]>;

        if constexpr (reflection::named_enum<field_type>) {
          value.[:member:] = reflection::from_string_or<field_type>(field.as<std::string>(), field_type{});
        } else {
          value.[:member:] = field.as<field_type>();
        }
      }
    }
  }

  return value;
}

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
  auto environment_keys = std::unordered_map<math::uuid, std::string>{};

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
  auto environments_table = YAML::Node{YAML::NodeType::Sequence};

  for (const auto entity : registry.view<mesh_renderer>()) {
    const auto& renderer = registry.get<mesh_renderer>(entity);

    if (renderer.mesh.is_valid() && !mesh_keys.contains(renderer.mesh->id())) {
      const auto id = renderer.mesh->id();
      const auto name = assets_module.path_of(id).stem().string();
      const auto key = make_key(name);

      mesh_keys.emplace(id, key);

      auto entry = YAML::Node{};
      entry["key"] = key;
      entry["name"] = name;
      entry["uuid"] = id.value();

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
        const auto key = make_key(material->name());

        material_keys.emplace(id, key);

        auto entry = YAML::Node{};
        entry["key"] = key;
        entry["name"] = material->name();
        entry["uuid"] = id.value();

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

    // Generic value components (transform, camera, lights) via reflection.
    template for (constexpr auto component_type : std::define_static_array(serializable_components_of(^^sbx::scenes))) {
      using Component = typename [:component_type:];

      if (registry.all_of<Component>(entity)) {
        auto component_yaml = serialize_component(registry.get<Component>(entity));

        component_yaml["type"] = std::string{type_name_of(component_type)};

        components.push_back(component_yaml);
      }
    }

    // Custom: mesh_renderer (asset keys + submesh material overrides).
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

    // Custom: skybox (environment asset key, lazily registered).
    if (registry.all_of<skybox>(entity)) {
      const auto& sky = registry.get<skybox>(entity);

      if (sky.environment.is_valid()) {
        const auto id = sky.environment->id();

        if (!environment_keys.contains(id)) {
          const auto name = assets_module.path_of(id).stem().string();
          const auto key = make_key(name);
          environment_keys.emplace(id, key);

          auto entry = YAML::Node{};
          entry["key"] = key;
          entry["name"] = name;
          entry["uuid"] = id.value();
          environments_table.push_back(entry);
        }

        auto component = YAML::Node{};
        component["type"] = "skybox";
        component["environment"] = environment_keys.at(id);
        component["intensity"] = sky.intensity;
        components.push_back(component);
      }
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
  assets_node["environment_maps"] = environments_table;

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

  // Asset table: key -> uuid
  auto key_to_uuid = std::unordered_map<std::string, math::uuid>{};

  const auto register_category = [&](const char* category) {
    if (const auto sequence = root["assets_module"][category]) {
      for (const auto entry : sequence) {
        key_to_uuid.emplace(entry["key"].as<std::string>(), entry["uuid"].as<math::uuid>());
      }
    }
  };

  register_category("static_meshes");
  register_category("materials");
  register_category("environment_maps");

  const auto nodes_node = root["nodes"];

  // Pass 1: create every node with its id (so parent/reference ids resolve).
  for (const auto node_yaml : nodes_node) {
    target._create_node(node_yaml["tag"].as<std::string>(), local_transform{}, node_yaml["id"].as<math::uuid>());
  }

  // Pass 2: parent, components.
  for (const auto node_yaml : nodes_node) {
    auto node = target.find(node_yaml["id"].as<math::uuid>());

    if (const auto parent = node_yaml["parent"]) {
      node.set_parent(target.find(parent.as<math::uuid>()));
    }

    for (const auto component_yaml : node_yaml["components"]) {
      const auto type = component_yaml["type"].as<std::string>();

      auto handled = false;

      // Generic value components via reflection.
      template for (constexpr auto component_type : std::define_static_array(serializable_components_of(^^sbx::scenes))) {
        using Component = typename [:component_type:];

        if (!handled && type == type_name_of(component_type)) {
          node.add_or_update<Component>(deserialize_component<Component>(component_yaml));

          handled = true;
        }
      }

      if (handled) {
        continue;
      }

      // Custom: mesh_renderer.
      if (type == "static_mesh") {
        auto& renderer = node.add_component<mesh_renderer>();
        renderer.mesh = assets_module.load_mesh(key_to_uuid.at(component_yaml["mesh"].as<std::string>()));

        if (const auto submeshes = component_yaml["submeshes"]) {
          for (const auto submesh : submeshes) {
            const auto index = submesh["index"].as<std::size_t>();

            if (renderer.materials.size() <= index) {
              renderer.materials.resize(index + 1u);
            }

            renderer.materials[index] = assets_module.load_material(key_to_uuid.at(submesh["material"].as<std::string>()));
          }
        }
      } else if (type == "skybox") {
        auto& sky = node.add_component<skybox>();

        sky.environment = assets_module.load_environment_map(key_to_uuid.at(component_yaml["environment"].as<std::string>()));

        if (component_yaml["intensity"]) {
          sky.intensity = component_yaml["intensity"].as<std::float_t>();
        }
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
