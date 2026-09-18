// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scene_serializer.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/overload.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/primitive_meshes.hpp>

#include <libsbx/canvas/components.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/shapes.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>

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
  std::unordered_map<math::uuid, std::string> animation_graph_keys{};
  std::unordered_map<math::uuid, std::string> texture_keys{};
  std::unordered_map<math::uuid, std::string> font_keys{};
  std::unordered_map<math::uuid, std::string> prefab_keys{};
  YAML::Node meshes_table{YAML::NodeType::Sequence};
  YAML::Node materials_table{YAML::NodeType::Sequence};
  YAML::Node environments_table{YAML::NodeType::Sequence};
  YAML::Node particle_effects_table{YAML::NodeType::Sequence};
  YAML::Node animation_graphs_table{YAML::NodeType::Sequence};
  YAML::Node textures_table{YAML::NodeType::Sequence};
  YAML::Node fonts_table{YAML::NodeType::Sequence};
  YAML::Node prefabs_table{YAML::NodeType::Sequence};
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

// A primitive mesh has no entry in the asset manifest -- path_of(id) comes back empty, so it needs
// its own name (used for both the yaml key and the table entry's cosmetic "name" field).
auto mesh_asset_name(assets::assets_module& assets_module, const math::uuid& id) -> std::string {
  if (const auto kind = assets::primitive_mesh_kind_of(id); kind.has_value()) {
    return std::string{assets::primitive_mesh_name(*kind)};
  }

  return assets_module.path_of(id).stem().string();
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
      const auto name = mesh_asset_name(assets_module, id);
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

  {
    // A real trackable component (not a bare top-level key like tag/id/parent) so a prefab's
    // layer propagates to its instances -- and can be per-instance overridden/reverted -- through
    // the exact same mark_prefab_override/apply_component_key machinery every other component
    // uses (see prefab_override.hpp's component_key<layer>).
    auto component = YAML::Node{};
    component["type"] = "layer";
    component["index"] = static_cast<std::uint32_t>(registry.get<layer>(entity).index);

    components.push_back(component);
  }

  if (registry.all_of<inactive>(entity)) {
    auto component = YAML::Node{};
    component["type"] = "inactive";

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

  // skeleton_pose isn't written -- it's fully auto-derived from mesh_renderer.mesh->skeleton() on
  // load (see read_node_components' "static_mesh" branch) and holds nothing but per-frame scratch
  // state otherwise.
  if (registry.all_of<animator>(entity)) {
    const auto& anim = registry.get<animator>(entity);

    if (anim.graph.is_valid()) {
      const auto graph_id = anim.graph->id();

      if (graph_id == math::uuid::nil()) {
        utility::logger<"scenes">::warn("Skipping a transient animation_graph override (no file asset — save it first)");
      } else {
        if (!keys.animation_graph_keys.contains(graph_id)) {
          const auto name = assets_module.path_of(graph_id).stem().string();
          const auto key = make_asset_key(keys, name);
          keys.animation_graph_keys.emplace(graph_id, key);

          auto entry = YAML::Node{};
          entry["key"] = key;
          entry["name"] = name;
          entry["uuid"] = graph_id.value();
          keys.animation_graphs_table.push_back(entry);
        }

        auto component = YAML::Node{};
        component["type"] = "animator";
        component["graph"] = keys.animation_graph_keys.at(graph_id);
        component["playing"] = anim.playing;

        // Parameter values aren't persisted per-instance -- every scene load starts from the
        // graph's own defaults (see animator::set_graph).
        components.push_back(component);
      }
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
    component["bloom_enabled"] = c.bloom_enabled;
    component["bloom_intensity"] = c.bloom_intensity;
    component["bloom_threshold"] = c.bloom_threshold;
    component["bloom_knee"] = c.bloom_knee;

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
        component["duration"] = instance.duration;
        component["playback"] = std::string{reflection::to_string(instance.playback)};

        components.push_back(component);
      }
    }
  }

  if (registry.all_of<canvas::canvas>(entity)) {
    const auto& canvas_component = registry.get<canvas::canvas>(entity);

    auto component = YAML::Node{};
    component["type"] = "canvas";
    component["mode"] = std::string{reflection::to_string(canvas_component.mode)};
    component["camera"] = canvas_component.camera.value();
    component["sort_order"] = canvas_component.sort_order;
    component["plane_distance"] = canvas_component.plane_distance;
    component["world_scale"] = canvas_component.world_scale;
    component["billboard"] = canvas_component.billboard;

    components.push_back(component);
  }

  if (registry.all_of<canvas::canvas_scaler>(entity)) {
    const auto& scaler = registry.get<canvas::canvas_scaler>(entity);

    auto component = YAML::Node{};
    component["type"] = "canvas_scaler";
    component["mode"] = std::string{reflection::to_string(scaler.mode)};
    component["reference_resolution"] = scaler.reference_resolution;
    component["match_width_or_height"] = scaler.match_width_or_height;

    components.push_back(component);
  }

  if (registry.all_of<canvas::rect_transform>(entity)) {
    const auto& rect = registry.get<canvas::rect_transform>(entity);

    auto component = YAML::Node{};
    component["type"] = "rect_transform";
    component["anchor_min"] = rect.anchor_min;
    component["anchor_max"] = rect.anchor_max;
    component["anchored_position"] = rect.anchored_position;
    component["size_delta"] = rect.size_delta;
    component["pivot"] = rect.pivot;

    components.push_back(component);
  }

  if (registry.all_of<canvas::canvas_group>(entity)) {
    const auto& group = registry.get<canvas::canvas_group>(entity);

    auto component = YAML::Node{};
    component["type"] = "canvas_group";
    component["alpha"] = group.alpha;
    component["interactable"] = group.interactable;
    component["blocks_raycasts"] = group.blocks_raycasts;
    component["ignore_parent_groups"] = group.ignore_parent_groups;

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_image>(entity)) {
    const auto& image = registry.get<canvas::ui_image>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_image";
    component["tint"] = image.tint;
    component["uv_rect"] = image.uv_rect;
    component["raycast_target"] = image.raycast_target;

    if (image.sprite.is_valid() && image.sprite->id() != math::uuid::nil()) {
      const auto id = image.sprite->id();

      if (!keys.texture_keys.contains(id)) {
        const auto name = assets_module.path_of(id).stem().string();
        const auto key = make_asset_key(keys, name);
        keys.texture_keys.emplace(id, key);

        auto entry = YAML::Node{};
        entry["key"] = key;
        entry["name"] = name;
        entry["uuid"] = id.value();
        keys.textures_table.push_back(entry);
      }

      component["sprite"] = keys.texture_keys.at(id);
    }

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_text>(entity)) {
    const auto& text = registry.get<canvas::ui_text>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_text";
    component["text"] = text.text;
    component["font_size"] = text.font_size;
    component["color"] = text.color;
    component["horizontal_align"] = std::string{reflection::to_string(text.horizontal_align)};
    component["vertical_align"] = std::string{reflection::to_string(text.vertical_align)};
    component["line_spacing"] = text.line_spacing;
    component["raycast_target"] = text.raycast_target;

    if (text.font.is_valid() && text.font->id() != math::uuid::nil()) {
      const auto id = text.font->id();

      if (!keys.font_keys.contains(id)) {
        const auto name = assets_module.path_of(id).stem().string();
        const auto key = make_asset_key(keys, name);
        keys.font_keys.emplace(id, key);

        auto entry = YAML::Node{};
        entry["key"] = key;
        entry["name"] = name;
        entry["uuid"] = id.value();
        keys.fonts_table.push_back(entry);
      }

      component["font"] = keys.font_keys.at(id);
    }

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_button>(entity)) {
    const auto& button = registry.get<canvas::ui_button>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_button";
    component["interactable"] = button.interactable;
    component["normal_color"] = button.normal_color;
    component["hovered_color"] = button.hovered_color;
    component["pressed_color"] = button.pressed_color;

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_toggle>(entity)) {
    const auto& toggle = registry.get<canvas::ui_toggle>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_toggle";
    component["is_on"] = toggle.is_on;
    component["interactable"] = toggle.interactable;
    component["on_color"] = toggle.on_color;
    component["off_color"] = toggle.off_color;
    component["group"] = toggle.group.value();

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_slider>(entity)) {
    const auto& slider = registry.get<canvas::ui_slider>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_slider";
    component["value"] = slider.value;
    component["min_value"] = slider.min_value;
    component["max_value"] = slider.max_value;
    component["whole_numbers"] = slider.whole_numbers;
    component["interactable"] = slider.interactable;
    component["direction"] = std::string{reflection::to_string(slider.direction)};
    component["track_color"] = slider.track_color;
    component["fill_color"] = slider.fill_color;

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_scrollbar>(entity)) {
    const auto& scrollbar = registry.get<canvas::ui_scrollbar>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_scrollbar";
    component["value"] = scrollbar.value;
    component["size"] = scrollbar.size;
    component["interactable"] = scrollbar.interactable;
    component["direction"] = std::string{reflection::to_string(scrollbar.direction)};
    component["track_color"] = scrollbar.track_color;
    component["handle_color"] = scrollbar.handle_color;

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_scroll_rect>(entity)) {
    const auto& scroll = registry.get<canvas::ui_scroll_rect>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_scroll_rect";
    component["content"] = scroll.content.value();
    component["horizontal"] = scroll.horizontal;
    component["vertical"] = scroll.vertical;
    component["normalized_position"] = scroll.normalized_position;

    components.push_back(component);
  }

  if (registry.all_of<canvas::layout_element>(entity)) {
    const auto& element = registry.get<canvas::layout_element>(entity);

    auto component = YAML::Node{};
    component["type"] = "layout_element";
    component["min_width"] = element.min_width;
    component["min_height"] = element.min_height;
    component["preferred_width"] = element.preferred_width;
    component["preferred_height"] = element.preferred_height;
    component["flexible_width"] = element.flexible_width;
    component["flexible_height"] = element.flexible_height;
    component["ignore_layout"] = element.ignore_layout;

    components.push_back(component);
  }

  if (registry.all_of<canvas::content_size_fitter>(entity)) {
    const auto& fitter = registry.get<canvas::content_size_fitter>(entity);

    auto component = YAML::Node{};
    component["type"] = "content_size_fitter";
    component["horizontal_fit"] = std::string{reflection::to_string(fitter.horizontal_fit)};
    component["vertical_fit"] = std::string{reflection::to_string(fitter.vertical_fit)};

    components.push_back(component);
  }

  if (registry.all_of<canvas::horizontal_layout_group>(entity)) {
    const auto& group = registry.get<canvas::horizontal_layout_group>(entity);

    auto component = YAML::Node{};
    component["type"] = "horizontal_layout_group";
    component["spacing"] = group.spacing;
    component["padding"] = group.padding;
    component["child_alignment"] = std::string{reflection::to_string(group.child_alignment)};
    component["control_child_width"] = group.control_child_width;
    component["control_child_height"] = group.control_child_height;
    component["child_force_expand_width"] = group.child_force_expand_width;
    component["child_force_expand_height"] = group.child_force_expand_height;

    components.push_back(component);
  }

  if (registry.all_of<canvas::vertical_layout_group>(entity)) {
    const auto& group = registry.get<canvas::vertical_layout_group>(entity);

    auto component = YAML::Node{};
    component["type"] = "vertical_layout_group";
    component["spacing"] = group.spacing;
    component["padding"] = group.padding;
    component["child_alignment"] = std::string{reflection::to_string(group.child_alignment)};
    component["control_child_width"] = group.control_child_width;
    component["control_child_height"] = group.control_child_height;
    component["child_force_expand_width"] = group.child_force_expand_width;
    component["child_force_expand_height"] = group.child_force_expand_height;

    components.push_back(component);
  }

  if (registry.all_of<canvas::grid_layout_group>(entity)) {
    const auto& group = registry.get<canvas::grid_layout_group>(entity);

    auto component = YAML::Node{};
    component["type"] = "grid_layout_group";
    component["cell_size"] = group.cell_size;
    component["spacing"] = group.spacing;
    component["padding"] = group.padding;
    component["child_alignment"] = std::string{reflection::to_string(group.child_alignment)};
    component["start_corner"] = std::string{reflection::to_string(group.start_corner)};
    component["start_axis"] = std::string{reflection::to_string(group.start_axis)};
    component["constraint"] = std::string{reflection::to_string(group.constraint)};
    component["constraint_count"] = group.constraint_count;

    components.push_back(component);
  }

  if (registry.all_of<canvas::ui_mask>(entity)) {
    const auto& mask = registry.get<canvas::ui_mask>(entity);

    auto component = YAML::Node{};
    component["type"] = "ui_mask";
    component["show_mask_graphic"] = mask.show_mask_graphic;

    components.push_back(component);
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
          case script_field_type::vector3: field_node["kind"] = "vector3"; field_node["value"] = field.vector3_value; break;
          case script_field_type::node:    field_node["kind"] = "node"; field_node["value"] = field.node_value.value(); break;
          case script_field_type::layer_mask: field_node["kind"] = "layer_mask"; field_node["value"] = field.layer_mask_value; break;
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
    component["body_type"] = std::string{reflection::to_string(body.type)};
    component["inverse_mass"] = body.inverse_mass;
    component["linear_velocity"] = body.linear_velocity;
    component["angular_velocity"] = body.angular_velocity;
    component["linear_damping"] = body.linear_damping;
    component["angular_damping"] = body.angular_damping;
    component["gravity_scale"] = body.gravity_scale;

    components.push_back(component);
  }

  if (registry.all_of<physics::nav_agent>(entity)) {
    const auto& agent = registry.get<physics::nav_agent>(entity);

    auto component = YAML::Node{};
    component["type"] = "nav_agent";
    component["radius"] = agent.radius;
    component["height"] = agent.height;
    component["base_offset"] = agent.base_offset;
    component["max_acceleration"] = agent.max_acceleration;
    component["max_speed"] = agent.max_speed;
    component["collision_query_range"] = agent.collision_query_range;
    component["path_optimization_range"] = agent.path_optimization_range;
    component["separation_weight"] = agent.separation_weight;
    component["obstacle_avoidance_enabled"] = agent.obstacle_avoidance_enabled;
    component["separation_enabled"] = agent.separation_enabled;

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
    component["is_trigger"] = collider.is_trigger;

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
      component["is_trigger"] = collider.is_trigger;

      components.push_back(component);
    }
  }

  if (registry.all_of<prefab_member>(entity)) {
    const auto& member = registry.get<prefab_member>(entity);

    auto component = YAML::Node{};
    component["type"] = "prefab_member";
    component["member_id"] = member.member_id.value();

    components.push_back(component);
  }

  if (registry.all_of<prefab_instance>(entity)) {
    const auto& instance = registry.get<prefab_instance>(entity);

    if (instance.source.is_valid()) {
      const auto prefab_id = instance.source->id();

      if (prefab_id == math::uuid::nil()) {
        utility::logger<"scenes">::warn("Skipping a transient prefab_instance override (no file asset — save it first)");
      } else {
        if (!keys.prefab_keys.contains(prefab_id)) {
          const auto name = assets_module.path_of(prefab_id).stem().string();
          const auto key = make_asset_key(keys, name);
          keys.prefab_keys.emplace(prefab_id, key);

          auto entry = YAML::Node{};
          entry["key"] = key;
          entry["name"] = name;
          entry["uuid"] = prefab_id.value();
          keys.prefabs_table.push_back(entry);
        }

        auto component = YAML::Node{};
        component["type"] = "prefab_instance";
        component["prefab"] = keys.prefab_keys.at(prefab_id);
        component["applied_generation"] = instance.applied_generation;

        if (!instance.overrides.empty()) {
          auto overrides = YAML::Node{YAML::NodeType::Sequence};

          for (const auto& override_entry : instance.overrides) {
            auto override_yaml = YAML::Node{};
            override_yaml["member_id"] = override_entry.member_id.value();
            override_yaml["component_key"] = override_entry.component_key;
            override_yaml["kind"] = (override_entry.kind == prefab_override_kind::component_removed) ? "component_removed" : (override_entry.kind == prefab_override_kind::node_removed) ? "node_removed" : "component_value";

            overrides.push_back(override_yaml);
          }

          component["overrides"] = overrides;
        }

        components.push_back(component);
      }
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
  register_category("animation_graphs");
  register_category("textures");
  register_category("fonts");
  register_category("prefabs");

  return key_to_uuid;
}

// Reads one node's "components" sequence and applies it to the already-created target_node.
auto read_node_components(node& target_node, const YAML::Node& node_yaml, assets::assets_module& assets_module, const std::unordered_map<std::string, math::uuid>& key_to_uuid) -> void {
  for (const auto component : node_yaml["components"]) {
    const auto type = component["type"].as<std::string>();

    if (type == "layer") {
      // Older scene/prefab files have no "layer" entry -- the node keeps the default layer 0
      // ("Default") scene::_create_node already gave it.
      target_node.get_component<scenes::layer>().index = static_cast<std::uint8_t>(component["index"].as<std::uint32_t>());
    } else if (type == "transform") {
      auto& transform = target_node.transform();
      transform.position = component["position"].as<math::vector3>();
      transform.rotation = component["rotation"].as<math::quaternion>();
      transform.scale = component["scale"].as<math::vector3>();
    } else if (type == "inactive") {
      // Absence means active -- scene::_create_node's freshly-created node already starts active,
      // so there's nothing to do for a node without this entry.
      target_node.set_active(false);
    } else if (type == "static_mesh") {
      auto& renderer = target_node.get_or_add_component<mesh_renderer>();
      renderer.mesh = assets_module.load_mesh(key_to_uuid.at(component["mesh"].as<std::string>()));

      sync_materials_with_mesh(renderer);

      // Auto-derived, not read from the file -- see the writer's comment above "animator" for why
      // skeleton_pose itself is never serialized.
      if (renderer.mesh.is_valid() && renderer.mesh->skeleton().is_valid()) {
        target_node.get_or_add_component<skeleton_pose>().skeleton = renderer.mesh->skeleton();
      }

      if (const auto submeshes = component["submeshes"]) {
        for (const auto submesh : submeshes) {
          const auto index = submesh["index"].as<std::size_t>();

          if (renderer.materials.size() <= index) {
            renderer.materials.resize(index + 1u);
          }

          renderer.materials[index] = assets_module.load_material(key_to_uuid.at(submesh["material"].as<std::string>()));
        }
      }
    } else if (type == "animator") {
      auto& anim = target_node.get_or_add_component<animator>();

      if (component["graph"]) {
        anim.set_graph(assets_module.load_animation_graph(key_to_uuid.at(component["graph"].as<std::string>())));
      }

      anim.playing = component["playing"] ? component["playing"].as<bool>() : true;
    } else if (type == "camera") {
      auto& c = target_node.get_or_add_component<camera>();
      c.fov_degrees = component["fov_degrees"].as<std::float_t>();
      c.near_plane = component["near_plane"].as<std::float_t>();
      c.far_plane = component["far_plane"].as<std::float_t>();

      if (component["exposure"]) {
        c.exposure = component["exposure"].as<std::float_t>();
      }

      if (component["bloom_enabled"]) {
        c.bloom_enabled = component["bloom_enabled"].as<bool>();
      }

      if (component["bloom_intensity"]) {
        c.bloom_intensity = component["bloom_intensity"].as<std::float_t>();
      }

      if (component["bloom_threshold"]) {
        c.bloom_threshold = component["bloom_threshold"].as<std::float_t>();
      }

      if (component["bloom_knee"]) {
        c.bloom_knee = component["bloom_knee"].as<std::float_t>();
      }
    } else if (type == "directional_light") {
      auto& light = target_node.get_or_add_component<directional_light>();
      light.color = component["color"].as<math::color>();
      light.intensity = component["intensity"].as<std::float_t>();
    } else if (type == "point_light") {
      auto& light = target_node.get_or_add_component<point_light>();
      light.color = component["color"].as<math::color>();
      light.intensity = component["intensity"].as<std::float_t>();
      light.range = component["range"].as<std::float_t>();
    } else if (type == "spot_light") {
      auto& light = target_node.get_or_add_component<spot_light>();
      light.color = component["color"].as<math::color>();
      light.intensity = component["intensity"].as<std::float_t>();
      light.range = component["range"].as<std::float_t>();
      light.inner_angle = component["inner_angle"].as<std::float_t>();
      light.outer_angle = component["outer_angle"].as<std::float_t>();
    } else if (type == "skybox") {
      auto& sky = target_node.get_or_add_component<skybox>();

      sky.environment = assets_module.load_environment_map(key_to_uuid.at(component["environment"].as<std::string>()));

      if (component["intensity"]) {
        sky.intensity = component["intensity"].as<std::float_t>();
      }

      // Older scene files predate the ambient/background split and only wrote "intensity",
      // which used to drive both — fall back to that value so those scenes keep rendering the
      // same instead of silently losing ambient brightness to the 1.0f struct default.
      sky.ambient_intensity = component["ambient_intensity"] ? component["ambient_intensity"].as<std::float_t>() : sky.intensity;
    } else if (type == "particle_effect") {
      auto& instance = target_node.get_or_add_component<particle_effect>();

      instance.effect = assets_module.load_particle_effect(key_to_uuid.at(component["effect"].as<std::string>()));

      if (component["loop"]) {
        instance.loop = component["loop"].as<bool>();
      }

      if (component["duration"]) {
        instance.duration = component["duration"].as<std::float_t>();
      }

      if (const auto playback = component["playback"]) {
        instance.playback = reflection::from_string_or<particle_playback_state>(playback.as<std::string>(), particle_playback_state::playing);
      }
    } else if (type == "canvas") {
      auto& canvas_component = target_node.get_or_add_component<canvas::canvas>();

      if (component["mode"]) {
        canvas_component.mode = reflection::from_string_or<canvas::render_mode>(component["mode"].as<std::string>(), canvas::render_mode::screen_space_overlay);
      }

      if (component["camera"]) {
        canvas_component.camera = component["camera"].as<math::uuid>();
      }

      if (component["sort_order"]) {
        canvas_component.sort_order = component["sort_order"].as<std::int32_t>();
      }

      if (component["plane_distance"]) {
        canvas_component.plane_distance = component["plane_distance"].as<std::float_t>();
      }

      if (component["world_scale"]) {
        canvas_component.world_scale = component["world_scale"].as<std::float_t>();
      }

      if (component["billboard"]) {
        canvas_component.billboard = component["billboard"].as<bool>();
      }
    } else if (type == "canvas_scaler") {
      auto& scaler = target_node.get_or_add_component<canvas::canvas_scaler>();

      if (component["mode"]) {
        scaler.mode = reflection::from_string_or<canvas::canvas_scale_mode>(component["mode"].as<std::string>(), canvas::canvas_scale_mode::constant_pixel_size);
      }

      if (component["reference_resolution"]) {
        scaler.reference_resolution = component["reference_resolution"].as<math::vector2>();
      }

      if (component["match_width_or_height"]) {
        scaler.match_width_or_height = component["match_width_or_height"].as<std::float_t>();
      }
    } else if (type == "rect_transform") {
      auto& rect = target_node.get_or_add_component<canvas::rect_transform>();

      rect.anchor_min = component["anchor_min"].as<math::vector2>();
      rect.anchor_max = component["anchor_max"].as<math::vector2>();
      rect.anchored_position = component["anchored_position"].as<math::vector2>();
      rect.size_delta = component["size_delta"].as<math::vector2>();
      rect.pivot = component["pivot"].as<math::vector2>();
    } else if (type == "canvas_group") {
      auto& group = target_node.get_or_add_component<canvas::canvas_group>();

      group.alpha = component["alpha"].as<std::float_t>();
      group.interactable = component["interactable"].as<bool>();
      group.blocks_raycasts = component["blocks_raycasts"].as<bool>();

      if (component["ignore_parent_groups"]) {
        group.ignore_parent_groups = component["ignore_parent_groups"].as<bool>();
      }
    } else if (type == "ui_image") {
      auto& image = target_node.get_or_add_component<canvas::ui_image>();

      image.tint = component["tint"].as<math::color>();

      if (component["uv_rect"]) {
        image.uv_rect = component["uv_rect"].as<math::vector4>();
      }

      if (component["raycast_target"]) {
        image.raycast_target = component["raycast_target"].as<bool>();
      }

      if (component["sprite"]) {
        image.sprite = assets_module.load_texture(key_to_uuid.at(component["sprite"].as<std::string>()));
      }
    } else if (type == "ui_text") {
      auto& text = target_node.get_or_add_component<canvas::ui_text>();

      text.text = component["text"].as<std::string>();
      text.font_size = component["font_size"].as<std::float_t>();
      text.color = component["color"].as<math::color>();

      if (const auto horizontal_align = component["horizontal_align"]) {
        text.horizontal_align = reflection::from_string_or<canvas::text_align>(horizontal_align.as<std::string>(), canvas::text_align::start);
      }

      if (const auto vertical_align = component["vertical_align"]) {
        text.vertical_align = reflection::from_string_or<canvas::text_align>(vertical_align.as<std::string>(), canvas::text_align::start);
      }

      if (component["line_spacing"]) {
        text.line_spacing = component["line_spacing"].as<std::float_t>();
      }

      if (component["font"]) {
        text.font = assets_module.load_font(key_to_uuid.at(component["font"].as<std::string>()));
      }

      if (component["raycast_target"]) {
        text.raycast_target = component["raycast_target"].as<bool>();
      }
    } else if (type == "ui_button") {
      auto& button = target_node.get_or_add_component<canvas::ui_button>();

      button.interactable = component["interactable"].as<bool>();
      button.normal_color = component["normal_color"].as<math::color>();
      button.hovered_color = component["hovered_color"].as<math::color>();
      button.pressed_color = component["pressed_color"].as<math::color>();
    } else if (type == "ui_toggle") {
      auto& toggle = target_node.get_or_add_component<canvas::ui_toggle>();

      toggle.is_on = component["is_on"].as<bool>();
      toggle.interactable = component["interactable"].as<bool>();
      toggle.on_color = component["on_color"].as<math::color>();
      toggle.off_color = component["off_color"].as<math::color>();

      if (component["group"]) {
        toggle.group = component["group"].as<math::uuid>();
      }
    } else if (type == "ui_slider") {
      auto& slider = target_node.get_or_add_component<canvas::ui_slider>();

      slider.value = component["value"].as<std::float_t>();
      slider.min_value = component["min_value"].as<std::float_t>();
      slider.max_value = component["max_value"].as<std::float_t>();
      slider.whole_numbers = component["whole_numbers"].as<bool>();
      slider.interactable = component["interactable"].as<bool>();
      slider.track_color = component["track_color"].as<math::color>();
      slider.fill_color = component["fill_color"].as<math::color>();

      if (component["direction"]) {
        slider.direction = reflection::from_string_or<canvas::slider_direction>(component["direction"].as<std::string>(), canvas::slider_direction::horizontal);
      }
    } else if (type == "ui_scrollbar") {
      auto& scrollbar = target_node.get_or_add_component<canvas::ui_scrollbar>();

      scrollbar.value = component["value"].as<std::float_t>();
      scrollbar.size = component["size"].as<std::float_t>();
      scrollbar.interactable = component["interactable"].as<bool>();
      scrollbar.track_color = component["track_color"].as<math::color>();
      scrollbar.handle_color = component["handle_color"].as<math::color>();

      if (component["direction"]) {
        scrollbar.direction = reflection::from_string_or<canvas::slider_direction>(component["direction"].as<std::string>(), canvas::slider_direction::horizontal);
      }
    } else if (type == "ui_scroll_rect") {
      auto& scroll = target_node.get_or_add_component<canvas::ui_scroll_rect>();

      scroll.horizontal = component["horizontal"].as<bool>();
      scroll.vertical = component["vertical"].as<bool>();
      scroll.normalized_position = component["normalized_position"].as<math::vector2>();

      if (component["content"]) {
        scroll.content = component["content"].as<math::uuid>();
      }
    } else if (type == "layout_element") {
      auto& element = target_node.get_or_add_component<canvas::layout_element>();

      element.min_width = component["min_width"].as<std::float_t>();
      element.min_height = component["min_height"].as<std::float_t>();
      element.preferred_width = component["preferred_width"].as<std::float_t>();
      element.preferred_height = component["preferred_height"].as<std::float_t>();
      element.flexible_width = component["flexible_width"].as<std::float_t>();
      element.flexible_height = component["flexible_height"].as<std::float_t>();

      if (component["ignore_layout"]) {
        element.ignore_layout = component["ignore_layout"].as<bool>();
      }
    } else if (type == "content_size_fitter") {
      auto& fitter = target_node.get_or_add_component<canvas::content_size_fitter>();

      if (component["horizontal_fit"]) {
        fitter.horizontal_fit = reflection::from_string_or<canvas::content_fit_mode>(component["horizontal_fit"].as<std::string>(), canvas::content_fit_mode::unconstrained);
      }

      if (component["vertical_fit"]) {
        fitter.vertical_fit = reflection::from_string_or<canvas::content_fit_mode>(component["vertical_fit"].as<std::string>(), canvas::content_fit_mode::unconstrained);
      }
    } else if (type == "horizontal_layout_group") {
      auto& group = target_node.get_or_add_component<canvas::horizontal_layout_group>();

      group.spacing = component["spacing"].as<std::float_t>();
      group.padding = component["padding"].as<math::vector4>();
      group.control_child_width = component["control_child_width"].as<bool>();
      group.control_child_height = component["control_child_height"].as<bool>();
      group.child_force_expand_width = component["child_force_expand_width"].as<bool>();
      group.child_force_expand_height = component["child_force_expand_height"].as<bool>();

      if (component["child_alignment"]) {
        group.child_alignment = reflection::from_string_or<canvas::layout_alignment>(component["child_alignment"].as<std::string>(), canvas::layout_alignment::upper_left);
      }
    } else if (type == "vertical_layout_group") {
      auto& group = target_node.get_or_add_component<canvas::vertical_layout_group>();

      group.spacing = component["spacing"].as<std::float_t>();
      group.padding = component["padding"].as<math::vector4>();
      group.control_child_width = component["control_child_width"].as<bool>();
      group.control_child_height = component["control_child_height"].as<bool>();
      group.child_force_expand_width = component["child_force_expand_width"].as<bool>();
      group.child_force_expand_height = component["child_force_expand_height"].as<bool>();

      if (component["child_alignment"]) {
        group.child_alignment = reflection::from_string_or<canvas::layout_alignment>(component["child_alignment"].as<std::string>(), canvas::layout_alignment::upper_left);
      }
    } else if (type == "grid_layout_group") {
      auto& group = target_node.get_or_add_component<canvas::grid_layout_group>();

      group.cell_size = component["cell_size"].as<math::vector2>();
      group.spacing = component["spacing"].as<math::vector2>();
      group.padding = component["padding"].as<math::vector4>();
      group.constraint_count = component["constraint_count"].as<std::int32_t>();

      if (component["child_alignment"]) {
        group.child_alignment = reflection::from_string_or<canvas::layout_alignment>(component["child_alignment"].as<std::string>(), canvas::layout_alignment::upper_left);
      }

      if (component["start_corner"]) {
        group.start_corner = reflection::from_string_or<canvas::grid_start_corner>(component["start_corner"].as<std::string>(), canvas::grid_start_corner::upper_left);
      }

      if (component["start_axis"]) {
        group.start_axis = reflection::from_string_or<canvas::grid_start_axis>(component["start_axis"].as<std::string>(), canvas::grid_start_axis::horizontal);
      }

      if (component["constraint"]) {
        group.constraint = reflection::from_string_or<canvas::grid_constraint>(component["constraint"].as<std::string>(), canvas::grid_constraint::flexible);
      }
    } else if (type == "ui_mask") {
      auto& mask = target_node.get_or_add_component<canvas::ui_mask>();

      if (component["show_mask_graphic"]) {
        mask.show_mask_graphic = component["show_mask_graphic"].as<bool>();
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
          } else if (kind == "vector3") {
            field.type = script_field_type::vector3;
            field.vector3_value = field_yaml["value"].as<math::vector3>();
          } else if (kind == "node") {
            field.type = script_field_type::node;
            field.node_value = field_yaml["value"].as<math::uuid>();
          } else if (kind == "layer_mask") {
            field.type = script_field_type::layer_mask;
            field.layer_mask_value = field_yaml["value"].as<std::uint32_t>();
          }

          entry.field_overrides.push_back(std::move(field));
        }
      }

      scripts.scripts.push_back(std::move(entry));
    } else if (type == "rigidbody") {
      auto& body = target_node.get_or_add_component<physics::rigidbody>();

      body.type = reflection::from_string_or<physics::body_type>(component["body_type"].as<std::string>(), physics::body_type::dynamic_body);

      body.inverse_mass = component["inverse_mass"].as<std::float_t>();
      body.linear_velocity = component["linear_velocity"].as<math::vector3>();
      body.angular_velocity = component["angular_velocity"].as<math::vector3>();
      body.linear_damping = component["linear_damping"].as<std::float_t>();
      body.angular_damping = component["angular_damping"].as<std::float_t>();
      body.gravity_scale = component["gravity_scale"].as<std::float_t>();
    } else if (type == "nav_agent") {
      auto& agent = target_node.get_or_add_component<physics::nav_agent>();

      agent.radius = component["radius"].as<std::float_t>();
      agent.height = component["height"].as<std::float_t>();
      agent.base_offset = component["base_offset"].as<std::float_t>(agent.height * 0.5f);
      agent.max_acceleration = component["max_acceleration"].as<std::float_t>();
      agent.max_speed = component["max_speed"].as<std::float_t>();
      agent.collision_query_range = component["collision_query_range"].as<std::float_t>();
      agent.path_optimization_range = component["path_optimization_range"].as<std::float_t>();
      agent.separation_weight = component["separation_weight"].as<std::float_t>();
      agent.obstacle_avoidance_enabled = component["obstacle_avoidance_enabled"].as<bool>();
      agent.separation_enabled = component["separation_enabled"].as<bool>();
    } else if (type == "shape_collider") {
      auto& collider = target_node.get_or_add_component<physics::shape_collider>();

      const auto shape_kind = component["shape"].as<std::string>();

      if (shape_kind == "sphere") {
        collider.shape = physics::sphere{component["radius"].as<std::float_t>()};
      } else if (shape_kind == "cylinder") {
        collider.shape = physics::cylinder{component["radius"].as<std::float_t>(), component["half_height"].as<std::float_t>()};
      } else if (shape_kind == "capsule") {
        collider.shape = physics::capsule{component["radius"].as<std::float_t>(), component["half_height"].as<std::float_t>()};
      } else if (shape_kind == "box") {
        collider.shape = physics::box{component["half_extents"].as<math::vector3>()};
      } else {
        utility::logger<"scenes">::warn("Unknown shape_collider shape '{}'", shape_kind);
      }

      collider.offset = component["offset"].as<math::vector3>();
      collider.rotation = component["rotation"].as<math::quaternion>();
      collider.friction = component["friction"].as<std::float_t>();
      collider.restitution = component["restitution"].as<std::float_t>();
      collider.is_trigger = component["is_trigger"].as<bool>(false); // absent in scenes saved before triggers existed
    } else if (type == "mesh_collider") {
      auto& collider = target_node.get_or_add_component<physics::mesh_collider>();

      collider.mesh = assets_module.load_mesh(key_to_uuid.at(component["mesh"].as<std::string>()));
      collider.offset = component["offset"].as<math::vector3>();
      collider.rotation = component["rotation"].as<math::quaternion>();
      collider.friction = component["friction"].as<std::float_t>();
      collider.restitution = component["restitution"].as<std::float_t>();
      collider.is_convex = component["convex"].as<bool>(false); // absent in scenes saved before convex mesh colliders existed
      collider.is_trigger = component["is_trigger"].as<bool>(false); // absent in scenes saved before triggers existed
    } else if (type == "prefab_member") {
      auto& member = target_node.get_or_add_component<prefab_member>();
      member.member_id = component["member_id"].as<math::uuid>();
    } else if (type == "prefab_instance") {
      auto& instance = target_node.get_or_add_component<prefab_instance>();

      instance.source = assets_module.load_prefab(key_to_uuid.at(component["prefab"].as<std::string>()));
      instance.applied_generation = component["applied_generation"] ? component["applied_generation"].as<std::uint64_t>() : 0u;
      instance.overrides.clear();

      if (const auto overrides = component["overrides"]) {
        for (const auto override_yaml : overrides) {
          auto override_entry = prefab_override{};
          override_entry.member_id = override_yaml["member_id"].as<math::uuid>();
          override_entry.component_key = override_yaml["component_key"].as<std::string>();

          const auto kind = override_yaml["kind"].as<std::string>();
          override_entry.kind = (kind == "component_removed") ? prefab_override_kind::component_removed : (kind == "node_removed") ? prefab_override_kind::node_removed : prefab_override_kind::component_value;

          instance.overrides.push_back(std::move(override_entry));
        }
      }
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

  // Mirrors the shape_collider case above; only a convex mesh_collider can be dynamic (see
  // collider.hpp).
  if (target_node.has_component<physics::rigidbody>() && target_node.has_component<physics::mesh_collider>()) {
    auto& body = target_node.get_component<physics::rigidbody>();
    const auto& collider = target_node.get_component<physics::mesh_collider>();

    if (collider.is_convex && collider.mesh.is_valid()) {
      auto hull_cache = physics::convex_hull_cache{};
      const auto& hull_data = hull_cache.get_or_build(assets_module, collider.mesh->id());

      const auto mass = (body.inverse_mass > 0.0f) ? (1.0f / body.inverse_mass) : 0.0f;
      body.local_inverse_inertia = physics::local_inverse_inertia(physics::convex_shape{physics::convex_hull{hull_data.points, hull_data.faces}}, mass);
    }
  }
}

auto scene_serializer::build(scene& target) -> YAML::Node {
  auto& registry = target._registry;
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  // Registry/view iteration order is unspecified, so node-order-sensitive passes below walk this
  // depth-first traversal instead, rooted at target._root (relationship::children is where
  // top-level order is persisted).
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
  assets_node["animation_graphs"] = keys.animation_graphs_table;
  assets_node["textures"] = keys.textures_table;
  assets_node["fonts"] = keys.fonts_table;
  assets_node["prefabs"] = keys.prefabs_table;

  auto root = YAML::Node{};
  root["metadata"] = metadata;
  root["assets_module"] = assets_node;
  root["nodes"] = nodes_node;

  return root;
}

auto scene_serializer::serialize(scene& target) -> std::string {
  auto stream = std::ostringstream{};
  stream << build(target);
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

  load(target, root);

  utility::logger<"scenes">::info("Loaded scene '{}'", resolved_path.generic_string());
}

auto scene_serializer::load(scene& target, const YAML::Node& root) -> void {
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

  // Any prefab_instance in this scene may have gone stale (its prefab edited) since the scene was
  // last saved -- resync once up front instead of leaving it stale until the next per-frame call.
  sync_prefab_instances(target);
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
  assets_node["animation_graphs"] = keys.animation_graphs_table;
  assets_node["textures"] = keys.textures_table;
  assets_node["fonts"] = keys.fonts_table;
  assets_node["prefabs"] = keys.prefabs_table;

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

// Reverse of register_asset_keys: seeds a fresh asset_key_table from an existing prefab's
// "assets_module" tables (uuid -> key, plus the sequence nodes themselves, cloned so appends never
// mutate the prefab's own stored snapshot before update_prefab commits) so re-serializing one
// component of an already-saved prefab reuses existing keys instead of minting duplicates, only
// minting a fresh key for an asset the prefab didn't reference yet.
auto load_asset_key_table(const YAML::Node& assets_node) -> asset_key_table {
  auto keys = asset_key_table{};

  const auto load_category = [&](const char* category, std::unordered_map<math::uuid, std::string>& uuid_to_key, YAML::Node& table) {
    if (const auto sequence = assets_node[category]) {
      table = YAML::Clone(sequence);

      for (const auto entry : sequence) {
        const auto key = entry["key"].as<std::string>();

        uuid_to_key.emplace(entry["uuid"].as<math::uuid>(), key);
        keys.used_keys.insert(key);
      }
    }
  };

  load_category("static_meshes", keys.mesh_keys, keys.meshes_table);
  load_category("materials", keys.material_keys, keys.materials_table);
  load_category("environment_maps", keys.environment_keys, keys.environments_table);
  load_category("particle_effects", keys.particle_effect_keys, keys.particle_effects_table);
  load_category("animation_graphs", keys.animation_graph_keys, keys.animation_graphs_table);
  load_category("textures", keys.texture_keys, keys.textures_table);
  load_category("fonts", keys.font_keys, keys.fonts_table);
  load_category("prefabs", keys.prefab_keys, keys.prefabs_table);

  return keys;
}

auto find_node_entry(const YAML::Node& nodes_sequence, const math::uuid& member_id) -> YAML::Node {
  for (const auto entry : nodes_sequence) {
    if (entry["id"].as<math::uuid>() == member_id) {
      return entry;
    }
  }

  return YAML::Node{};
}

// Replaces every "components" entry of type component_key in target_components with whatever
// entries of that same type source_components holds (0, 1, or -- "script" only -- several). Both
// are "components" sequences in the write_node/serialize_subtree shape.
auto replace_component_entries(YAML::Node target_components, const YAML::Node& source_components, std::string_view component_key) -> void {
  for (auto index = target_components.size(); index-- > 0u;) {
    if (target_components[index]["type"].as<std::string>() == component_key) {
      target_components.remove(index);
    }
  }

  if (!source_components) {
    return;
  }

  for (const auto entry : source_components) {
    if (entry["type"].as<std::string>() == component_key) {
      target_components.push_back(entry);
    }
  }
}

// Applies just node_entry's component_key component(s) onto target_node, leaving every other
// component untouched -- the per-key merge primitive behind sync_prefab_instances and
// revert_prefab_override. "script" is the one type that can repeat (several Behaviors on one node),
// so it's the one case that needs its whole script_component cleared first; every other type is a
// single get_or_add-and-overwrite in read_node_components, already idempotent against a
// pre-existing value of the same type.
auto apply_component_key(node target_node, const YAML::Node& node_entry, const std::unordered_map<std::string, math::uuid>& key_to_uuid, assets::assets_module& assets_module, std::string_view component_key) -> void {
  if (component_key == "script") {
    target_node.remove_component<script_component>();
  }

  auto matches = YAML::Node{YAML::NodeType::Sequence};

  if (const auto components = node_entry["components"]) {
    for (const auto entry : components) {
      if (entry["type"].as<std::string>() == component_key) {
        matches.push_back(entry);
      }
    }
  }

  auto scratch = YAML::Node{};
  scratch["components"] = matches;

  read_node_components(target_node, scratch, assets_module, key_to_uuid);
}

// Walks up from a prefab_member node to the instance root carrying prefab_instance -- every node
// under a prefab instance has prefab_member, but only the root also has prefab_instance. Returns an
// invalid node if member_node isn't part of any prefab instance.
auto find_prefab_instance_root(scene& target, node member_node) -> node {
  if (!member_node.has_component<prefab_member>()) {
    return node{};
  }

  auto current = member_node;

  while (current.is_valid() && !current.has_component<prefab_instance>()) {
    auto parent = target.node_of(current.get_component<relationship>().parent);
    current = parent;
  }

  return (current.is_valid() && current.has_component<prefab_instance>()) ? current : node{};
}

// Shared tail of apply_prefab_override and update_prefab_from_node: commit snapshot as prefab's
// new content and persist it, if it's already a saved (not merely in-memory) asset.
auto save_prefab_snapshot(assets::assets_module& assets_module, assets::prefab_handle& prefab, YAML::Node snapshot) -> void {
  assets_module.update_prefab(prefab, std::move(snapshot));

  if (const auto path = assets_module.path_of(prefab->id()); !path.empty()) {
    assets_module.save_prefab(prefab, path);
  }
}

auto scene_serializer::create_prefab_from_node(scene& source, node subtree_root, std::string name) -> assets::prefab_handle {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto snapshot = serialize_subtree(source, subtree_root);

  return assets_module.create_prefab(std::move(snapshot), std::move(name));
}

auto scene_serializer::attach_prefab_instance(scene& target, node subtree_root, assets::prefab_handle prefab) -> void {
  const auto tag_member = [&](this const auto& self, node current) -> void {
    current.add_component<prefab_member>(current.id());

    for (const auto child : current.get_component<relationship>().children) {
      auto child_node = target.node_of(child);
      self(child_node);
    }
  };

  tag_member(subtree_root);

  subtree_root.add_component<prefab_instance>(prefab, prefab.generation());
}

auto scene_serializer::instantiate_prefab(scene& target, const assets::prefab_handle& prefab, std::optional<math::uuid> root_id) -> node {
  if (!prefab.is_valid()) {
    return node{};
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto& snapshot = prefab->snapshot();
  const auto key_to_uuid = register_asset_keys(snapshot["assets_module"]);
  const auto nodes_node = snapshot["nodes"];

  // Fresh scenes::id per node -- two instances (or an instance and its own prefab) can't share
  // ids -- except the root, which keeps root_id when given (see this method's doc comment).
  auto id_remap = std::unordered_map<math::uuid, math::uuid>{};
  auto is_first = true;

  for (const auto node_yaml : nodes_node) {
    const auto old_id = node_yaml["id"].as<math::uuid>();
    const auto new_id = (is_first && root_id) ? *root_id : math::uuid::create();

    id_remap.emplace(old_id, new_id);
    is_first = false;
  }

  // Pass 1: create every node under its remapped id, tagged with its prefab-local id.
  for (const auto node_yaml : nodes_node) {
    const auto old_id = node_yaml["id"].as<math::uuid>();

    auto fresh = target._create_node(node_yaml["tag"].as<std::string>(), local_transform{}, id_remap.at(old_id));
    fresh.add_component<prefab_member>(old_id);
  }

  // Pass 2: parent + components, same two-pass shape as deserialize_subtree.
  for (const auto node_yaml : nodes_node) {
    const auto old_id = node_yaml["id"].as<math::uuid>();
    auto instance_node = target.find(id_remap.at(old_id));

    if (const auto parent = node_yaml["parent"]) {
      auto parent_node = target.find(id_remap.at(parent.as<math::uuid>()));

      instance_node.set_parent(parent_node);
    }

    read_node_components(instance_node, node_yaml, assets_module, key_to_uuid);
  }

  const auto root_old_id = nodes_node[0]["id"].as<math::uuid>();
  auto root = target.find(id_remap.at(root_old_id));

  root.add_component<prefab_instance>(prefab, prefab.generation());

  return root;
}

auto scene_serializer::sync_prefab_instances(scene& target) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto stale_roots = std::vector<ecs::entity>{};

  for (auto&& [entity, instance] : target.query<prefab_instance>().each()) {
    if (instance.source.is_valid() && instance.source.generation() != instance.applied_generation) {
      stale_roots.push_back(entity);
    }
  }

  for (const auto root_entity : stale_roots) {
    auto root = target.node_of(root_entity);

    if (!root.is_valid()) {
      continue;
    }

    auto& instance = root.get_component<prefab_instance>();
    const auto& snapshot = instance.source->snapshot();
    const auto key_to_uuid = register_asset_keys(snapshot["assets_module"]);
    const auto nodes_node = snapshot["nodes"];

    // Index the instance's current subtree by its nodes' prefab-local (member) id.
    auto by_member = std::unordered_map<math::uuid, ecs::entity>{};

    const auto collect = [&](this const auto& self, ecs::entity current) -> void {
      auto current_node = target.node_of(current);

      if (const auto member = current_node.try_get_component<prefab_member>()) {
        by_member.emplace(member->member_id, current);
      }

      for (const auto child : current_node.get_component<relationship>().children) {
        self(child);
      }
    };

    collect(root_entity);

    const auto is_overridden = [&](const math::uuid& member_id, std::string_view key) {
      return std::ranges::any_of(instance.overrides, [&](const prefab_override& override_entry) {
        return override_entry.member_id == member_id && override_entry.component_key == key;
      });
    };

    const auto is_node_removed = [&](const math::uuid& member_id) {
      return std::ranges::any_of(instance.overrides, [&](const prefab_override& override_entry) {
        return override_entry.member_id == member_id && override_entry.kind == prefab_override_kind::node_removed;
      });
    };

    for (const auto node_yaml : nodes_node) {
      const auto member_id = node_yaml["id"].as<math::uuid>();
      const auto existing = by_member.find(member_id);

      if (existing == by_member.end()) {
        if (is_node_removed(member_id)) {
          // This instance deliberately deleted this member (see delete_node_command's
          // node_removed hook, which marks every prefab_member in a deleted subtree, not just
          // its root) -- stays gone across resyncs instead of being resurrected.
          continue;
        }

        // The prefab gained a node since this instance's last sync -- instantiate just this one
        // under its already-resolved instance parent (parents always precede children in this
        // sequence, see serialize_subtree's DFS, so a multi-level addition resolves in one pass).
        const auto parent_member = node_yaml["parent"] ? node_yaml["parent"].as<math::uuid>() : math::uuid::nil();
        const auto parent_entity = by_member.contains(parent_member) ? by_member.at(parent_member) : root_entity;

        auto fresh = target._create_node(node_yaml["tag"].as<std::string>(), local_transform{}, math::uuid::create());
        fresh.add_component<prefab_member>(member_id);
        read_node_components(fresh, node_yaml, assets_module, key_to_uuid);

        auto parent_node = target.node_of(parent_entity);
        fresh.set_parent(parent_node);

        by_member.emplace(member_id, fresh._entity);

        continue;
      }

      auto live_node = target.node_of(existing->second);

      if (const auto components = node_yaml["components"]) {
        auto seen_keys = std::unordered_set<std::string>{};

        for (const auto component : components) {
          const auto key = component["type"].as<std::string>();

          if (!seen_keys.insert(key).second || is_overridden(member_id, key)) {
            continue; // "script" can list several entries of the same type in one node -- apply_component_key already replays every match in a single call
          }

          // The instance root's own placement is never prefab content, regardless of whether an
          // override was ever recorded for it -- see scene_serializer.hpp's prefab_override doc
          // comment. Every other node's transform is ordinary trackable/syncable content.
          if (key == "transform" && existing->second == root_entity) {
            continue;
          }

          apply_component_key(live_node, node_yaml, key_to_uuid, assets_module, key);
        }
      }
    }

    instance.applied_generation = instance.source.generation();
  }
}

auto scene_serializer::apply_prefab_override(scene& target, node source_node, std::string_view component_key) -> void {
  auto root = find_prefab_instance_root(target, source_node);

  if (!root.is_valid()) {
    return;
  }

  // Defensive: mark_prefab_override already refuses to ever record a "transform" override for
  // the root, so prefab_overrides_of (and thus the Hierarchy/Inspector menus) never offer this in
  // the first place -- this just guards any other future caller.
  if (component_key == "transform" && source_node == root) {
    return;
  }

  auto& instance = root.get_component<prefab_instance>();

  if (!instance.source.is_valid()) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto snapshot = YAML::Clone(instance.source->snapshot());
  auto keys = load_asset_key_table(snapshot["assets_module"]);

  // Seeds keys.mesh_keys/material_keys for source_node's own mesh/material references, mirroring
  // serialize_subtree's prepass -- write_node's "static_mesh"/"mesh_collider" branches assume
  // their mesh is already keyed and would throw otherwise (collect_mesh_material_keys is the only
  // place that registers a mesh key; every other asset kind registers its own key inline).
  collect_mesh_material_keys(target._registry, {source_node._entity}, assets_module, keys);

  const auto member_id = source_node.get_component<prefab_member>().member_id;
  auto node_entry = find_node_entry(snapshot["nodes"], member_id);

  if (!node_entry) {
    return; // shouldn't happen -- member_id came from this same instance's own prefab_member
  }

  auto scratch = YAML::Node{};
  write_node(scratch, target._registry, source_node._entity, assets_module, keys, false);

  replace_component_entries(node_entry["components"], scratch["components"], component_key);

  snapshot["assets_module"]["static_meshes"] = keys.meshes_table;
  snapshot["assets_module"]["materials"] = keys.materials_table;
  snapshot["assets_module"]["environment_maps"] = keys.environments_table;
  snapshot["assets_module"]["particle_effects"] = keys.particle_effects_table;
  snapshot["assets_module"]["animation_graphs"] = keys.animation_graphs_table;
  snapshot["assets_module"]["textures"] = keys.textures_table;
  snapshot["assets_module"]["fonts"] = keys.fonts_table;
  snapshot["assets_module"]["prefabs"] = keys.prefabs_table;

  save_prefab_snapshot(assets_module, instance.source, snapshot);

  std::erase_if(instance.overrides, [&](const prefab_override& override_entry) {
    return override_entry.member_id == member_id && override_entry.component_key == component_key;
  });
}

auto scene_serializer::revert_prefab_override(scene& target, node target_node, std::string_view component_key) -> void {
  auto root = find_prefab_instance_root(target, target_node);

  if (!root.is_valid()) {
    return;
  }

  // See the matching guard in apply_prefab_override.
  if (component_key == "transform" && target_node == root) {
    return;
  }

  auto& instance = root.get_component<prefab_instance>();

  if (!instance.source.is_valid()) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto& snapshot = instance.source->snapshot();
  const auto key_to_uuid = register_asset_keys(snapshot["assets_module"]);

  const auto member_id = target_node.get_component<prefab_member>().member_id;
  const auto node_entry = find_node_entry(snapshot["nodes"], member_id);

  if (node_entry) {
    apply_component_key(target_node, node_entry, key_to_uuid, assets_module, component_key);
  }

  std::erase_if(instance.overrides, [&](const prefab_override& override_entry) {
    return override_entry.member_id == member_id && override_entry.component_key == component_key;
  });
}

auto scene_serializer::mark_prefab_override(scene& target, node member_node, std::string_view component_key, prefab_override_kind kind) -> void {
  auto root = find_prefab_instance_root(target, member_node);

  if (!root.is_valid()) {
    return;
  }

  // The instance root's own placement is never prefab content -- see prefab_override's doc
  // comment and sync_prefab_instances' matching skip. A descendant's transform is unaffected.
  if (component_key == "transform" && member_node == root) {
    return;
  }

  auto& instance = root.get_component<prefab_instance>();
  const auto member_id = member_node.get_component<prefab_member>().member_id;

  for (auto& existing : instance.overrides) {
    if (existing.member_id == member_id && existing.component_key == component_key) {
      existing.kind = kind;
      return;
    }
  }

  instance.overrides.push_back(prefab_override{member_id, std::string{component_key}, kind});
}

auto scene_serializer::prefab_overrides_of(scene& target, node member_node) -> std::vector<prefab_override> {
  auto root = find_prefab_instance_root(target, member_node);

  if (!root.is_valid()) {
    return {};
  }

  const auto member_id = member_node.get_component<prefab_member>().member_id;
  const auto& instance = root.get_component<prefab_instance>();

  auto result = std::vector<prefab_override>{};

  for (const auto& override_entry : instance.overrides) {
    if (override_entry.member_id == member_id && override_entry.kind != prefab_override_kind::node_removed) {
      result.push_back(override_entry);
    }
  }

  return result;
}

auto scene_serializer::update_prefab_from_node(scene& source, node instance_root) -> void {
  if (!instance_root.has_component<prefab_instance>()) {
    return;
  }

  auto& instance = instance_root.get_component<prefab_instance>();

  if (!instance.source.is_valid()) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  save_prefab_snapshot(assets_module, instance.source, serialize_subtree(source, instance_root));

  instance.overrides.clear();
  instance.applied_generation = instance.source.generation();
}

} // namespace sbx::scenes
