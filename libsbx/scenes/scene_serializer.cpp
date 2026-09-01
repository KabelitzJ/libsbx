// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene_serializer.hpp>

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/overload.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/shapes.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>

namespace sbx::scenes {

// Assigns each referenced mesh/material/environment-map/particle-effect a short, unique, stable
// (within one serialize) name, so nodes referencing the same asset just repeat its key instead of
// its full uuid — shared by both the whole-scene build and a single-subtree snapshot.
struct asset_key_table {
  std::unordered_map<math::uuid, std::string> mesh_keys{};
  std::unordered_map<math::uuid, std::string> material_keys{};
  std::unordered_set<std::string> used_keys{};
  std::unordered_map<math::uuid, std::string> environment_keys{};
  std::unordered_map<math::uuid, std::string> particle_effect_keys{};
  YAML::Node meshes_table{YAML::NodeType::Sequence};
  YAML::Node materials_table{YAML::NodeType::Sequence};
  YAML::Node environments_table{YAML::NodeType::Sequence};
  YAML::Node particle_effects_table{YAML::NodeType::Sequence};
}; // struct asset_key_table

auto make_asset_key(asset_key_table& keys, const std::string& base) -> std::string {
  auto key = base.empty() ? std::string{"asset"} : base;
  auto suffix = 1;

  while (keys.used_keys.contains(key)) {
    key = fmt::format("{}_{}", base, suffix++);
  }

  keys.used_keys.insert(key);

  return key;
}

// Pre-pass: every mesh/material referenced by a mesh_renderer among entities gets a table entry
// and a key, before any node is written (so a node can always look its references up by key).
auto collect_mesh_material_keys(ecs::registry& registry, const std::vector<ecs::entity>& entities, assets::assets_module& assets_module, asset_key_table& keys) -> void {
  for (const auto entity : entities) {
    if (!registry.all_of<mesh_renderer>(entity)) {
      continue;
    }

    const auto& renderer = registry.get<mesh_renderer>(entity);

    if (renderer.mesh.is_valid() && !keys.mesh_keys.contains(renderer.mesh->id())) {
      const auto id = renderer.mesh->id();
      const auto name = assets_module.path_of(id).stem().string();
      const auto key = make_asset_key(keys, name);

      keys.mesh_keys.emplace(id, key);

      auto entry = YAML::Node{};
      entry["key"] = key;
      entry["name"] = name;
      entry["uuid"] = id.value();

      keys.meshes_table.push_back(entry);
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

      if (!keys.material_keys.contains(id)) {
        const auto key = make_asset_key(keys, material->name());

        keys.material_keys.emplace(id, key);

        auto entry = YAML::Node{};
        entry["key"] = key;
        entry["name"] = material->name();
        entry["uuid"] = id.value();

        keys.materials_table.push_back(entry);
      }
    }
  }

  // A mesh_collider references the same kind of asset a mesh_renderer does — share one table
  // entry/key if a node (or another node) already registered the same mesh via either component.
  for (const auto entity : entities) {
    if (!registry.all_of<physics::mesh_collider>(entity)) {
      continue;
    }

    const auto& collider = registry.get<physics::mesh_collider>(entity);

    if (collider.mesh.is_valid() && !keys.mesh_keys.contains(collider.mesh->id())) {
      const auto id = collider.mesh->id();
      const auto name = assets_module.path_of(id).stem().string();
      const auto key = make_asset_key(keys, name);

      keys.mesh_keys.emplace(id, key);

      auto entry = YAML::Node{};
      entry["key"] = key;
      entry["name"] = name;
      entry["uuid"] = id.value();

      keys.meshes_table.push_back(entry);
    }
  }
}

// Writes one node's full YAML entry (tag/id/parent/components). write_parent_key is false only
// for a subtree snapshot's own root — its real parent (if any) isn't part of the snapshot, so it
// must come back attached under scene::root() until the caller repositions it.
auto write_node(YAML::Node& node_yaml, ecs::registry& registry, ecs::entity entity, assets::assets_module& assets_module, asset_key_table& keys, bool write_parent_key) -> void {
  node_yaml["tag"] = registry.get<tag>(entity).str();
  node_yaml["id"] = registry.get<id>(entity).value();

  const auto& relationship_component = registry.get<relationship>(entity);

  // parent is target._root for a top-level node — that's the sentinel, not a real node, so it
  // has no id to write (and no "parent" key means "top-level" on load).
  if (write_parent_key && relationship_component.parent != ecs::null_entity && registry.all_of<id>(relationship_component.parent)) {
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
      component["mesh"] = keys.mesh_keys.at(renderer.mesh->id());

      auto submeshes = YAML::Node{YAML::NodeType::Sequence};

      for (auto index = std::size_t{0u}; index < renderer.materials.size(); ++index) {
        const auto& material = renderer.materials[index];

        if (material.is_valid() && material->id() != math::uuid::nil()) {
          auto submesh = YAML::Node{};
          submesh["index"] = index;
          submesh["material"] = keys.material_keys.at(material->id());

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

      if (!keys.environment_keys.contains(id)) {
        const auto name = assets_module.path_of(id).stem().string();
        const auto key = make_asset_key(keys, name);
        keys.environment_keys.emplace(id, key);

        auto entry = YAML::Node{};
        entry["key"] = key;
        entry["name"] = name;
        entry["uuid"] = id.value();
        keys.environments_table.push_back(entry);
      }

      auto component = YAML::Node{};
      component["type"] = "skybox";
      component["environment"] = keys.environment_keys.at(id);
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
        if (!keys.particle_effect_keys.contains(effect_id)) {
          const auto name = assets_module.path_of(effect_id).stem().string();
          const auto key = make_asset_key(keys, name);
          keys.particle_effect_keys.emplace(effect_id, key);

          auto entry = YAML::Node{};
          entry["key"] = key;
          entry["name"] = name;
          entry["uuid"] = effect_id.value();
          keys.particle_effects_table.push_back(entry);
        }

        auto component = YAML::Node{};
        component["type"] = "particle_effect";
        component["effect"] = keys.particle_effect_keys.at(effect_id);
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

  if (registry.all_of<physics::rigidbody>(entity)) {
    const auto& body = registry.get<physics::rigidbody>(entity);

    auto component = YAML::Node{};
    component["type"] = "rigidbody";
    component["body_type"] = (body.type == physics::body_type::dynamic_body) ? "dynamic" : (body.type == physics::body_type::kinematic) ? "kinematic" : "static";
    component["inverse_mass"] = body.inverse_mass;
    component["linear_velocity"] = body.linear_velocity;
    component["angular_velocity"] = body.angular_velocity;
    component["linear_damping"] = body.linear_damping;
    component["angular_damping"] = body.angular_damping;
    component["gravity_scale"] = body.gravity_scale;

    components.push_back(component);
  }

  if (registry.all_of<physics::shape_collider>(entity)) {
    const auto& collider = registry.get<physics::shape_collider>(entity);

    auto component = YAML::Node{};
    component["type"] = "shape_collider";
    component["offset"] = collider.offset;
    component["rotation"] = collider.rotation;
    component["friction"] = collider.friction;
    component["restitution"] = collider.restitution;

    std::visit(utility::overload(
      [&](const physics::sphere& shape) {
        component["shape"] = "sphere";
        component["radius"] = shape.radius;
      },
      [&](const physics::cylinder& shape) {
        component["shape"] = "cylinder";
        component["radius"] = shape.radius;
        component["half_height"] = shape.half_height;
      },
      [&](const physics::capsule& shape) {
        component["shape"] = "capsule";
        component["radius"] = shape.radius;
        component["half_height"] = shape.half_height;
      },
      [&](const physics::box& shape) {
        component["shape"] = "box";
        component["half_extents"] = shape.half_extents;
      },
      [&]([[maybe_unused]] const physics::triangle& shape) {
        // Never authored directly on a shape_collider — only appears internally as a
        // mesh_collider narrowphase candidate — so there's nothing meaningful to write.
      },
      [&]([[maybe_unused]] const physics::convex_hull& shape) {
        // Never authored directly on a shape_collider either — only ever constructed transiently
        // by narrowphase for a mesh_collider with convex == true — nothing meaningful to write.
      }
    ), collider.shape);

    components.push_back(component);
  }

  if (registry.all_of<physics::mesh_collider>(entity)) {
    const auto& collider = registry.get<physics::mesh_collider>(entity);

    if (collider.mesh.is_valid()) {
      auto component = YAML::Node{};
      component["type"] = "mesh_collider";
      component["mesh"] = keys.mesh_keys.at(collider.mesh->id());
      component["offset"] = collider.offset;
      component["rotation"] = collider.rotation;
      component["friction"] = collider.friction;
      component["restitution"] = collider.restitution;
      component["convex"] = collider.is_convex;

      components.push_back(component);
    }
  }

  node_yaml["components"] = components;
}

// Reads a "assets_module" YAML node's 4 category tables into one key -> uuid lookup.
auto register_asset_keys(const YAML::Node& assets_node) -> std::unordered_map<std::string, math::uuid> {
  auto key_to_uuid = std::unordered_map<std::string, math::uuid>{};

  const auto register_category = [&](const char* category) {
    if (const auto sequence = assets_node[category]) {
      for (const auto entry : sequence) {
        key_to_uuid.emplace(entry["key"].as<std::string>(), entry["uuid"].as<math::uuid>());
      }
    }
  };

  register_category("static_meshes");
  register_category("materials");
  register_category("environment_maps");
  register_category("particle_effects");

  return key_to_uuid;
}

// Reads one node's "components" sequence and applies it to the already-created target_node.
auto read_node_components(node& target_node, const YAML::Node& node_yaml, assets::assets_module& assets_module, const std::unordered_map<std::string, math::uuid>& key_to_uuid) -> void {
  for (const auto component : node_yaml["components"]) {
    const auto type = component["type"].as<std::string>();

    if (type == "transform") {
      auto& transform = target_node.transform();
      transform.position = component["position"].as<math::vector3f>();
      transform.rotation = component["rotation"].as<math::quaternion>();
      transform.scale = component["scale"].as<math::vector3f>();
    } else if (type == "static_mesh") {
      auto& renderer = target_node.add_component<mesh_renderer>();
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
      auto& c = target_node.add_component<camera>();
      c.fov_degrees = component["fov_degrees"].as<std::float_t>();
      c.near_plane = component["near_plane"].as<std::float_t>();
      c.far_plane = component["far_plane"].as<std::float_t>();

      if (component["exposure"]) {
        c.exposure = component["exposure"].as<std::float_t>();
      }
    } else if (type == "directional_light") {
      auto& light = target_node.add_component<directional_light>();
      light.color = component["color"].as<math::color>();
      light.intensity = component["intensity"].as<std::float_t>();
    } else if (type == "point_light") {
      auto& light = target_node.add_component<point_light>();
      light.color = component["color"].as<math::color>();
      light.intensity = component["intensity"].as<std::float_t>();
      light.range = component["range"].as<std::float_t>();
    } else if (type == "spot_light") {
      auto& light = target_node.add_component<spot_light>();
      light.color = component["color"].as<math::color>();
      light.intensity = component["intensity"].as<std::float_t>();
      light.range = component["range"].as<std::float_t>();
      light.inner_angle = component["inner_angle"].as<std::float_t>();
      light.outer_angle = component["outer_angle"].as<std::float_t>();
    } else if (type == "skybox") {
      auto& sky = target_node.add_component<skybox>();

      sky.environment = assets_module.load_environment_map(key_to_uuid.at(component["environment"].as<std::string>()));

      if (component["intensity"]) {
        sky.intensity = component["intensity"].as<std::float_t>();
      }

      // Older scene files predate the ambient/background split and only wrote "intensity",
      // which used to drive both — fall back to that value so those scenes keep rendering the
      // same instead of silently losing ambient brightness to the 1.0f struct default.
      sky.ambient_intensity = component["ambient_intensity"] ? component["ambient_intensity"].as<std::float_t>() : sky.intensity;
    } else if (type == "particle_effect") {
      auto& instance = target_node.add_component<particle_effect>();

      instance.effect = assets_module.load_particle_effect(key_to_uuid.at(component["effect"].as<std::string>()));

      if (component["loop"]) {
        instance.loop = component["loop"].as<bool>();
      }

      if (const auto playback = component["playback"]) {
        const auto value = playback.as<std::string>();
        instance.playback = (value == "paused") ? particle_playback_state::paused : (value == "stopped") ? particle_playback_state::stopped : particle_playback_state::playing;
      }
    } else if (type == "script") {
      auto& scripts = target_node.get_or_add_component<script_component>();

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
    } else if (type == "rigidbody") {
      auto& body = target_node.add_component<physics::rigidbody>();

      const auto body_type_string = component["body_type"].as<std::string>();
      body.type = (body_type_string == "kinematic") ? physics::body_type::kinematic : (body_type_string == "static") ? physics::body_type::static_body : physics::body_type::dynamic_body;

      body.inverse_mass = component["inverse_mass"].as<std::float_t>();
      body.linear_velocity = component["linear_velocity"].as<math::vector3f>();
      body.angular_velocity = component["angular_velocity"].as<math::vector3f>();
      body.linear_damping = component["linear_damping"].as<std::float_t>();
      body.angular_damping = component["angular_damping"].as<std::float_t>();
      body.gravity_scale = component["gravity_scale"].as<std::float_t>();
    } else if (type == "shape_collider") {
      auto& collider = target_node.add_component<physics::shape_collider>();

      const auto shape_kind = component["shape"].as<std::string>();

      if (shape_kind == "sphere") {
        collider.shape = physics::sphere{component["radius"].as<std::float_t>()};
      } else if (shape_kind == "cylinder") {
        collider.shape = physics::cylinder{component["radius"].as<std::float_t>(), component["half_height"].as<std::float_t>()};
      } else if (shape_kind == "capsule") {
        collider.shape = physics::capsule{component["radius"].as<std::float_t>(), component["half_height"].as<std::float_t>()};
      } else if (shape_kind == "box") {
        collider.shape = physics::box{component["half_extents"].as<math::vector3f>()};
      } else {
        utility::logger<"scenes">::warn("Unknown shape_collider shape '{}'", shape_kind);
      }

      collider.offset = component["offset"].as<math::vector3f>();
      collider.rotation = component["rotation"].as<math::quaternion>();
      collider.friction = component["friction"].as<std::float_t>();
      collider.restitution = component["restitution"].as<std::float_t>();
    } else if (type == "mesh_collider") {
      auto& collider = target_node.add_component<physics::mesh_collider>();

      collider.mesh = assets_module.load_mesh(key_to_uuid.at(component["mesh"].as<std::string>()));
      collider.offset = component["offset"].as<math::vector3f>();
      collider.rotation = component["rotation"].as<math::quaternion>();
      collider.friction = component["friction"].as<std::float_t>();
      collider.restitution = component["restitution"].as<std::float_t>();
      collider.is_convex = component["convex"].as<bool>(false); // absent in scenes saved before convex mesh colliders existed
    } else {
      utility::logger<"scenes">::warn("Unknown component type '{}'", type);
    }
  }

  // local_inverse_inertia is transient (not serialized — see write_node) and depends on both the
  // body's mass and its collider's shape, so it's only computable once every component on this
  // node has been read, regardless of which order they appeared in the YAML.
  if (target_node.has_component<physics::rigidbody>() && target_node.has_component<physics::shape_collider>()) {
    auto& body = target_node.get_component<physics::rigidbody>();
    const auto& collider = target_node.get_component<physics::shape_collider>();

    const auto mass = (body.inverse_mass > 0.0f) ? (1.0f / body.inverse_mass) : 0.0f;
    body.local_inverse_inertia = physics::local_inverse_inertia(collider.shape, mass);
  }

  // Same idea, for a dynamic convex mesh_collider (see collider.hpp's doc comment: only a convex
  // one can ever be dynamic). A throwaway convex_hull_cache is fine here -- this only runs once per
  // node per scene load, and physics_module builds its own cache for the same mesh independently
  // the first time it actually processes this node.
  if (target_node.has_component<physics::rigidbody>() && target_node.has_component<physics::mesh_collider>()) {
    auto& body = target_node.get_component<physics::rigidbody>();
    const auto& collider = target_node.get_component<physics::mesh_collider>();

    if (collider.is_convex && collider.mesh.is_valid()) {
      auto hull_cache = physics::convex_hull_cache{};
      const auto& hull_data = hull_cache.get_or_build(assets_module, collider.mesh->id());

      const auto mass = (body.inverse_mass > 0.0f) ? (1.0f / body.inverse_mass) : 0.0f;
      body.local_inverse_inertia = physics::local_inverse_inertia(physics::convex_shape{physics::convex_hull{hull_data.points}}, mass);
    }
  }
}

auto scene_serializer::_build(scene& target) -> YAML::Node {
  auto& registry = target._registry;
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  // Registry/view iteration order is unspecified (and reshuffles across create/destroy churn), so
  // every node-order-sensitive pass below walks this single depth-first traversal of the scene
  // graph instead — rooted at target._root, whose relationship::children is the one place
  // top-level order is actually persisted.
  auto ordered_nodes = std::vector<ecs::entity>{};

  const auto collect = [&](this const auto& self, ecs::entity entity) -> void {
    ordered_nodes.push_back(entity);

    for (const auto child : registry.get<relationship>(entity).children) {
      self(child);
    }
  };

  for (const auto entity : registry.get<relationship>(target._root).children) {
    collect(entity);
  }

  auto keys = asset_key_table{};
  collect_mesh_material_keys(registry, ordered_nodes, assets_module, keys);

  auto nodes_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto entity : ordered_nodes) {
    auto node_yaml = YAML::Node{};
    write_node(node_yaml, registry, entity, assets_module, keys, true);
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
  assets_node["static_meshes"] = keys.meshes_table;
  assets_node["materials"] = keys.materials_table;
  assets_node["environment_maps"] = keys.environments_table;
  assets_node["particle_effects"] = keys.particle_effects_table;

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

  target._registry.clear(); // also destroys target._root — recreate it before anything else runs
  target._root = target._registry.create();
  target._registry.emplace<relationship>(target._root);
  target._entities_by_id.clear();
  target._entities_by_name.clear();
  target._active_camera = ecs::null_entity;
  target._primary_light = ecs::null_entity;

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  if (const auto metadata = root["metadata"]; metadata && metadata["name"]) {
    target.set_name(metadata["name"].as<std::string>());
  }

  const auto key_to_uuid = register_asset_keys(root["assets_module"]);

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

    read_node_components(node, node_yaml, assets_module, key_to_uuid);
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

auto scene_serializer::serialize_subtree(scene& target, node subtree_root) -> YAML::Node {
  auto& registry = target._registry;
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto ordered_nodes = std::vector<ecs::entity>{};

  const auto collect = [&](this const auto& self, ecs::entity entity) -> void {
    ordered_nodes.push_back(entity);

    for (const auto child : registry.get<relationship>(entity).children) {
      self(child);
    }
  };

  collect(subtree_root._entity);

  auto keys = asset_key_table{};
  collect_mesh_material_keys(registry, ordered_nodes, assets_module, keys);

  auto nodes_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto entity : ordered_nodes) {
    auto node_yaml = YAML::Node{};
    write_node(node_yaml, registry, entity, assets_module, keys, entity != subtree_root._entity);
    nodes_node.push_back(node_yaml);
  }

  auto assets_node = YAML::Node{};
  assets_node["static_meshes"] = keys.meshes_table;
  assets_node["materials"] = keys.materials_table;
  assets_node["environment_maps"] = keys.environments_table;
  assets_node["particle_effects"] = keys.particle_effects_table;

  auto root = YAML::Node{};
  root["assets_module"] = assets_node;
  root["nodes"] = nodes_node;

  return root;
}

auto scene_serializer::deserialize_subtree(scene& target, const YAML::Node& snapshot) -> node {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto key_to_uuid = register_asset_keys(snapshot["assets_module"]);
  const auto nodes_node = snapshot["nodes"];

  // Pass 1: create every node with its id (so parent/reference ids resolve) — index 0 is always
  // the subtree root (see serialize_subtree's DFS, which visits it before any descendant).
  for (const auto node_yaml : nodes_node) {
    target._create_node(node_yaml["tag"].as<std::string>(), local_transform{}, node_yaml["id"].as<math::uuid>());
  }

  const auto root_id = nodes_node[0]["id"].as<math::uuid>();

  // Pass 2: tag, parent, components. The root's entry never has a "parent" key (see
  // serialize_subtree), so it's left attached under target._root — the caller repositions it.
  for (const auto node_yaml : nodes_node) {
    auto node = target.find(node_yaml["id"].as<math::uuid>());

    if (const auto parent = node_yaml["parent"]) {
      auto parent_node = target.find(parent.as<math::uuid>());

      node.set_parent(parent_node);
    }

    read_node_components(node, node_yaml, assets_module, key_to_uuid);
  }

  return target.find(root_id);
}

} // namespace sbx::scenes
