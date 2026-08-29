// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene_serializer.hpp>

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::scenes {

auto scene_serializer::_build(scene& target) -> YAML::Node {
  auto& registry = target._registry;
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  // Assets table: collect referenced meshes/materials, assign unique keys
  auto mesh_keys = std::unordered_map<math::uuid, std::string>{};
  auto material_keys = std::unordered_map<math::uuid, std::string>{};
  auto used_keys = std::unordered_set<std::string>{};
  auto environment_keys = std::unordered_map<math::uuid, std::string>{};
  auto particle_effect_keys = std::unordered_map<math::uuid, std::string>{};

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
  auto particle_effects_table = YAML::Node{YAML::NodeType::Sequence};

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
      component["exposure"] = c.exposure;

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
        component["ambient_intensity"] = sky.ambient_intensity;
        components.push_back(component);
      }
    }

    if (registry.all_of<particle_effect>(entity)) {
      const auto& instance = registry.get<particle_effect>(entity);

      if (instance.effect.is_valid()) {
        const auto effect_id = instance.effect->id();

        if (effect_id == math::uuid::nil()) {
          utility::logger<"scenes">::warn("Skipping a transient particle_effect override (no file asset — save it first)");
        } else {
          if (!particle_effect_keys.contains(effect_id)) {
            const auto name = assets_module.path_of(effect_id).stem().string();
            const auto key = make_key(name);
            particle_effect_keys.emplace(effect_id, key);

            auto entry = YAML::Node{};
            entry["key"] = key;
            entry["name"] = name;
            entry["uuid"] = effect_id.value();
            particle_effects_table.push_back(entry);
          }

          auto component = YAML::Node{};
          component["type"] = "particle_effect";
          component["effect"] = particle_effect_keys.at(effect_id);
          component["loop"] = instance.loop;
          component["playback"] = (instance.playback == particle_playback_state::paused) ? "paused" : (instance.playback == particle_playback_state::stopped) ? "stopped" : "playing";

          components.push_back(component);
        }
      }
    }

    if (registry.all_of<script_component>(entity)) {
      const auto& scripts = registry.get<script_component>(entity);

      for (const auto& entry : scripts.scripts) {
        auto component = YAML::Node{};
        component["type"] = "script";
        component["class_name"] = entry.class_name;

        auto fields = YAML::Node{YAML::NodeType::Sequence};

        for (const auto& field : entry.field_overrides) {
          auto field_node = YAML::Node{};
          field_node["name"] = field.name;

          switch (field.type) {
            case script_field_type::float32: field_node["kind"] = "float"; field_node["value"] = field.float_value; break;
            case script_field_type::int32:   field_node["kind"] = "int";   field_node["value"] = field.int_value; break;
            case script_field_type::boolean: field_node["kind"] = "bool";  field_node["value"] = field.bool_value; break;
            case script_field_type::string:  field_node["kind"] = "string"; field_node["value"] = field.string_value; break;
          }

          fields.push_back(field_node);
        }

        component["fields"] = fields;
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
  assets_node["particle_effects"] = particle_effects_table;

  auto root = YAML::Node{};
  root["metadata"] = metadata;
  root["assets_module"] = assets_node;
  root["nodes"] = nodes_node;

  return root;
}

auto scene_serializer::serialize(scene& target) -> std::string {
  auto stream = std::ostringstream{};
  stream << _build(target);
  return stream.str();
}

auto scene_serializer::save(scene& target, const std::filesystem::path& path) -> void {
  auto& project = core::engine::project();

  // An absolute path (e.g. the editor's play-mode snapshot, living under .sbx/ rather than
  // assets/) is already fully resolved — only prefix relative, asset-directory-relative paths.
  const auto resolved_path = path.is_absolute() ? path : project.assets_directory() / path;
  const auto content = serialize(target);

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << content;

  utility::logger<"scenes">::info("Saved scene '{}'", resolved_path.generic_string());
}

auto scene_serializer::load(scene& target, const std::filesystem::path& path) -> void {
  auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  // See the matching comment in save() — an absolute path is already fully resolved.
  const auto resolved_path = path.is_absolute() ? path : assets_directory / path;

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
  register_category("particle_effects");

  const auto nodes_node = root["nodes"];

  // Pass 1: create every node with its id (so parent/reference ids resolve).
  for (const auto node_yaml : nodes_node) {
    target._create_node(node_yaml["tag"].as<std::string>(), local_transform{}, node_yaml["id"].as<math::uuid>());
  }

  // Pass 2: tag, parent, components.
  for (const auto node_yaml : nodes_node) {
    auto node = target.find(node_yaml["id"].as<math::uuid>());

    if (const auto parent = node_yaml["parent"]) {
      auto parent_node = target.find(parent.as<math::uuid>());

      node.set_parent(parent_node);
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
        renderer.mesh = assets_module.load_mesh(key_to_uuid.at(component["mesh"].as<std::string>()));

        sync_materials_with_mesh(renderer);

        if (const auto submeshes = component["submeshes"]) {
          for (const auto submesh : submeshes) {
            const auto index = submesh["index"].as<std::size_t>();

            if (renderer.materials.size() <= index) {
              renderer.materials.resize(index + 1u);
            }

            renderer.materials[index] = assets_module.load_material(key_to_uuid.at(submesh["material"].as<std::string>()));
          }
        }
      } else if (type == "camera") {
        auto& c = node.add_component<camera>();
        c.fov_degrees = component["fov_degrees"].as<std::float_t>();
        c.near_plane = component["near_plane"].as<std::float_t>();
        c.far_plane = component["far_plane"].as<std::float_t>();

        if (component["exposure"]) {
          c.exposure = component["exposure"].as<std::float_t>();
        }
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
      } else if (type == "skybox") {
        auto& sky = node.add_component<skybox>();

        sky.environment = assets_module.load_environment_map(key_to_uuid.at(component["environment"].as<std::string>()));

        if (component["intensity"]) {
          sky.intensity = component["intensity"].as<std::float_t>();
        }

        // Older scene files predate the ambient/background split and only wrote "intensity",
        // which used to drive both — fall back to that value so those scenes keep rendering the
        // same instead of silently losing ambient brightness to the 1.0f struct default.
        sky.ambient_intensity = component["ambient_intensity"] ? component["ambient_intensity"].as<std::float_t>() : sky.intensity;
      } else if (type == "particle_effect") {
        auto& instance = node.add_component<particle_effect>();

        instance.effect = assets_module.load_particle_effect(key_to_uuid.at(component["effect"].as<std::string>()));

        if (component["loop"]) {
          instance.loop = component["loop"].as<bool>();
        }

        if (const auto playback = component["playback"]) {
          const auto value = playback.as<std::string>();
          instance.playback = (value == "paused") ? particle_playback_state::paused : (value == "stopped") ? particle_playback_state::stopped : particle_playback_state::playing;
        }
      } else if (type == "script") {
        auto& scripts = node.get_or_add_component<script_component>();

        auto entry = script_entry{};
        entry.class_name = component["class_name"].as<std::string>();

        if (const auto fields = component["fields"]) {
          for (const auto field_yaml : fields) {
            auto field = script_field_override{};
            field.name = field_yaml["name"].as<std::string>();

            const auto kind = field_yaml["kind"].as<std::string>();

            if (kind == "float") {
              field.type = script_field_type::float32;
              field.float_value = field_yaml["value"].as<std::float_t>();
            } else if (kind == "int") {
              field.type = script_field_type::int32;
              field.int_value = field_yaml["value"].as<std::int32_t>();
            } else if (kind == "bool") {
              field.type = script_field_type::boolean;
              field.bool_value = field_yaml["value"].as<bool>();
            } else if (kind == "string") {
              field.type = script_field_type::string;
              field.string_value = field_yaml["value"].as<std::string>();
            }

            entry.field_overrides.push_back(std::move(field));
          }
        }

        scripts.scripts.push_back(std::move(entry));
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
