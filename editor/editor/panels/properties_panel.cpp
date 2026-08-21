// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/properties_panel.hpp>

#include <array>
#include <cstring>

#include <imgui.h>

#include <editor/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

namespace {

auto path_text(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> std::string {
  const auto path = assets_module.path_of(id);
  return path.empty() ? std::string{"(unknown)"} : path.string();
}

auto alpha_mode_name(sbx::assets::alpha_mode mode) -> const char* {
  switch (mode) {
    case sbx::assets::alpha_mode::opaque: return "Opaque";
    case sbx::assets::alpha_mode::mask: return "Mask";
    case sbx::assets::alpha_mode::blend: return "Blend";
  }

  return "Unknown";
}

// A compact, color-coded X/Y/Z row: a label, then three tinted axis buttons (click to reset that
// axis to reset_value) each immediately followed by its own drag field. Returns true if any axis
// changed this frame.
auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, std::float_t reset_value, std::float_t speed) -> bool {
  static constexpr auto axis_labels = std::array<const char*, 3u>{"X", "Y", "Z"};
  static constexpr auto axis_ids = std::array<const char*, 3u>{"##X", "##Y", "##Z"};
  static constexpr auto axis_colors = std::array<ImVec4, 3u>{
    ImVec4{0.75f, 0.20f, 0.25f, 1.0f}, // X - red
    ImVec4{0.30f, 0.65f, 0.30f, 1.0f}, // Y - green
    ImVec4{0.20f, 0.45f, 0.80f, 1.0f}, // Z - blue
  };

  auto changed = false;

  ImGui::PushID(label);

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(90.0f);

  const auto line_height = ImGui::GetFrameHeight();
  const auto button_size = ImVec2{line_height, line_height};
  const auto item_width = (ImGui::GetContentRegionAvail().x - 3.0f * button_size.x) / 3.0f - 2.0f * ImGui::GetStyle().ItemSpacing.x;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.0f, 0.0f});

  for (auto axis = std::size_t{0u}; axis < 3u; ++axis) {
    if (axis != 0u) {
      ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, axis_colors[axis]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, axis_colors[axis]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, axis_colors[axis]);

    if (ImGui::Button(axis_labels[axis], button_size)) {
      values[axis] = reset_value;
      changed = true;
    }

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(item_width);
    changed |= ImGui::DragFloat(axis_ids[axis], &values[axis], speed);
  }

  ImGui::PopStyleVar();
  ImGui::PopID();

  return changed;
}

auto draw_color_field(const char* label, sbx::math::color& color) -> void {
  auto value = std::array<std::float_t, 4u>{color.r(), color.g(), color.b(), color.a()};
  if (ImGui::ColorEdit4(label, value.data())) {
    color.r() = value[0];
    color.g() = value[1];
    color.b() = value[2];
    color.a() = value[3];
  }
}

auto draw_camera_section(sbx::scenes::node& node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_CAMERA_OUTLINE " Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& camera = node.get_component<sbx::scenes::camera>();

  ImGui::DragFloat("FOV (degrees)", &camera.fov_degrees, 0.5f, 1.0f, 179.0f);
  ImGui::DragFloat("Near Plane", &camera.near_plane, 0.01f, 0.001f, camera.far_plane);
  ImGui::DragFloat("Far Plane", &camera.far_plane, 1.0f, camera.near_plane, 100000.0f);
}

auto draw_mesh_renderer_section(sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_CUBE_OUTLINE " Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  const auto& renderer = node.get_component<sbx::scenes::mesh_renderer>();

  if (renderer.mesh.is_valid()) {
    ImGui::Text("Mesh: %s", path_text(assets_module, renderer.mesh->id()).c_str());
    ImGui::Text("Submeshes: %zu", renderer.mesh->submeshes().size());
  } else {
    ImGui::TextDisabled("Mesh: (none)");
  }

  for (auto index = std::size_t{0u}; index < renderer.materials.size(); ++index) {
    const auto& material = renderer.materials[index];

    if (material.is_valid()) {
      ImGui::Text("Material %zu: %s", index, path_text(assets_module, material->id()).c_str());
    } else {
      ImGui::TextDisabled("Material %zu: (none)", index);
    }
  }
}

auto draw_directional_light_section(sbx::scenes::node& node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::directional_light>();

  draw_color_field("Color", light.color);
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
}

auto draw_point_light_section(sbx::scenes::node& node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_LIGHTBULB_OUTLINE " Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::point_light>();

  draw_color_field("Color", light.color);
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 10000.0f);
}

auto draw_spot_light_section(sbx::scenes::node& node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_FLASHLIGHT " Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::spot_light>();

  draw_color_field("Color", light.color);
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 10000.0f);

  // inner_angle/outer_angle are stored in radians; SliderAngle operates on a radians pointer
  // while displaying/editing in degrees, so these bind directly — no manual conversion.
  ImGui::SliderAngle("Inner Angle", &light.inner_angle, 0.0f, 90.0f);
  ImGui::SliderAngle("Outer Angle", &light.outer_angle, 0.0f, 90.0f);
}

auto draw_skybox_section(sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_EARTH " Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& sky = node.get_component<sbx::scenes::skybox>();

  if (sky.environment.is_valid()) {
    ImGui::Text("Environment: %s", path_text(assets_module, sky.environment->id()).c_str());

    const auto* environment = sky.environment.get();
    const auto is_baked = environment->radiance_index() != sbx::assets::environment_map::invalid_index &&
                           environment->irradiance_index() != sbx::assets::environment_map::invalid_index &&
                           environment->prefiltered_index() != sbx::assets::environment_map::invalid_index;
    ImGui::Text("Baked: %s", is_baked ? "yes" : "no");
  } else {
    ImGui::TextDisabled("Environment: (none)");
  }

  ImGui::DragFloat("Intensity", &sky.intensity, 0.05f, 0.0f, 100.0f);
}

} // namespace

auto properties_panel::_draw_name_field(sbx::scenes::node& node) -> void {
  const auto id = node.id();

  if (id.value() != _name_buffer_id.value()) {
    const auto& current_name = node.name();
    std::strncpy(_name_buffer.data(), current_name.c_str(), _name_buffer.size() - 1u);
    _name_buffer[_name_buffer.size() - 1u] = '\0';
    _name_buffer_id = id;
  }

  if (ImGui::InputText("Name", _name_buffer.data(), _name_buffer.size())) {
    // Live-edited into the buffer; committed below once editing finishes.
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    // Renaming here does not update scene::_entities_by_name (populated at creation only), so
    // scene::find(name) can go stale for renamed nodes. Fine: selection/hierarchy key on
    // entity/id, never name.
    node.name() = sbx::scenes::tag{std::string{_name_buffer.data()}};
  }

  ImGui::Text("UUID: %llu", static_cast<unsigned long long>(id.value()));
}

auto properties_panel::_draw_transform_section(sbx::scenes::node& node) -> void {
  ImGui::SeparatorText("Transform");

  auto& transform = node.transform();

  auto position = std::array<std::float_t, 3u>{transform.position.x(), transform.position.y(), transform.position.z()};

  if (draw_vector3_control("Position", position, 0.0f, 0.05f)) {
    transform.position = sbx::math::vector3f{position[0], position[1], position[2]};
  }

  // See _rotation_node_id/_rotation_cache/_rotation's declarations for why this is cached rather
  // than re-derived from the quaternion every frame.
  if (node.id().value() != _rotation_node_id.value() || !(transform.rotation == _rotation_cache)) {
    const auto euler = sbx::math::quaternion::euler_angles(transform.rotation);
    _rotation = {euler.x(), euler.y(), euler.z()};
    _rotation_node_id = node.id();
    _rotation_cache = transform.rotation;
  }

  if (draw_vector3_control("Rotation", _rotation, 0.0f, 0.5f)) {
    transform.rotation = sbx::math::quaternion{sbx::math::vector3f{_rotation[0], _rotation[1], _rotation[2]}};
    _rotation_cache = transform.rotation;
  }

  auto scale = std::array<std::float_t, 3u>{transform.scale.x(), transform.scale.y(), transform.scale.z()};
  if (draw_vector3_control("Scale", scale, 1.0f, 0.05f)) {
    transform.scale = sbx::math::vector3f{scale[0], scale[1], scale[2]};
  }
}

auto properties_panel::_draw_node_properties(sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  _draw_name_field(node);
  _draw_transform_section(node);

  if (node.has_component<sbx::scenes::camera>()) {
    draw_camera_section(node);
  }

  if (node.has_component<sbx::scenes::mesh_renderer>()) {
    draw_mesh_renderer_section(node, assets_module);
  }

  if (node.has_component<sbx::scenes::directional_light>()) {
    draw_directional_light_section(node);
  }

  if (node.has_component<sbx::scenes::point_light>()) {
    draw_point_light_section(node);
  }

  if (node.has_component<sbx::scenes::spot_light>()) {
    draw_spot_light_section(node);
  }

  if (node.has_component<sbx::scenes::skybox>()) {
    draw_skybox_section(node, assets_module);
  }
}

auto properties_panel::_draw_asset_properties(const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (_asset_cache.id.value() != asset.id.value()) {
    _asset_cache = asset_property_cache{};
    _asset_cache.id = asset.id;

    switch (asset.kind) {
      case asset_kind::texture: _asset_cache.texture = assets_module.load_texture(asset.id); break;
      case asset_kind::mesh: _asset_cache.mesh = assets_module.load_mesh(asset.id); break;
      case asset_kind::material: _asset_cache.material = assets_module.load_material(asset.id); break;
      case asset_kind::environment_map: _asset_cache.environment_map = assets_module.load_environment_map(asset.id); break;
      case asset_kind::scene:
      case asset_kind::unknown:
        break;
    }
  }

  ImGui::Text("Path: %s", asset.path.string().c_str());
  ImGui::Text("UUID: %llu", static_cast<unsigned long long>(asset.id.value()));

  switch (asset.kind) {
    case asset_kind::texture: {
      ImGui::Text("Type: Texture");
      const auto& handle = _asset_cache.texture;
      if (handle.is_valid()) {
        ImGui::Text("Bindless Index: %u", handle->index());
        ImGui::Text("Resident: %s", assets_module.is_resident(handle) ? "yes" : "no");
      }
      break;
    }
    case asset_kind::mesh: {
      ImGui::Text("Type: Mesh");
      const auto& handle = _asset_cache.mesh;
      if (handle.is_valid()) {
        ImGui::Text("Submeshes: %zu", handle->submeshes().size());
        ImGui::Text("Uploaded: %s", handle->is_uploaded() ? "yes" : "no");
      }
      break;
    }
    case asset_kind::material: {
      ImGui::Text("Type: Material");
      const auto& handle = _asset_cache.material;
      if (handle.is_valid()) {
        ImGui::Text("Name: %s", handle->name().c_str());

        auto base_color = handle->base_color_factor();
        ImGui::BeginDisabled();
        draw_color_field("Base Color", base_color);
        ImGui::EndDisabled();

        ImGui::Text("Metallic: %.2f", static_cast<double>(handle->metallic_factor()));
        ImGui::Text("Roughness: %.2f", static_cast<double>(handle->roughness_factor()));
        ImGui::Text("Alpha Mode: %s", alpha_mode_name(handle->alpha()));
        ImGui::Text("Double Sided: %s", handle->is_double_sided() ? "yes" : "no");

        if (handle->albedo().is_valid()) {
          ImGui::Text("Albedo: %s", path_text(assets_module, handle->albedo()->id()).c_str());
        }
        if (handle->normal().is_valid()) {
          ImGui::Text("Normal: %s", path_text(assets_module, handle->normal()->id()).c_str());
        }
        if (handle->metallic_roughness().is_valid()) {
          ImGui::Text("Metallic/Roughness: %s", path_text(assets_module, handle->metallic_roughness()->id()).c_str());
        }
        if (handle->occlusion().is_valid()) {
          ImGui::Text("Occlusion: %s", path_text(assets_module, handle->occlusion()->id()).c_str());
        }
        if (handle->emissive().is_valid()) {
          ImGui::Text("Emissive: %s", path_text(assets_module, handle->emissive()->id()).c_str());
        }
      }
      break;
    }
    case asset_kind::environment_map: {
      ImGui::Text("Type: Environment Map");
      const auto& handle = _asset_cache.environment_map;
      if (handle.is_valid()) {
        const auto is_baked = handle->radiance_index() != sbx::assets::environment_map::invalid_index &&
                               handle->irradiance_index() != sbx::assets::environment_map::invalid_index &&
                               handle->prefiltered_index() != sbx::assets::environment_map::invalid_index;
        ImGui::Text("Baked: %s", is_baked ? "yes" : "no");
        ImGui::Text("Prefiltered Mips: %u", handle->prefiltered_mip_count());
      }
      break;
    }
    case asset_kind::scene: {
      ImGui::Text("Type: Scene (not imported)");
      break;
    }
    case asset_kind::unknown: {
      ImGui::Text("Type: Unknown");
      break;
    }
  }
}

auto properties_panel::draw(editor_state& state) -> void {
  ImGui::Begin(ICON_MDI_TUNE " Properties###properties_panel");

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (std::holds_alternative<node_selection>(state.current_selection)) {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    if (auto node = state.selected_node(scenes_module.active_scene()); node.is_valid()) {
      _draw_node_properties(node, assets_module);
    } else {
      // The selected entity no longer exists (e.g. deleted); fall back to the empty state.
      state.clear_selection();
      ImGui::TextDisabled("Nothing selected.");
    }
  } else if (const auto* asset = std::get_if<asset_selection>(&state.current_selection); asset != nullptr) {
    _draw_asset_properties(*asset, assets_module);
  } else {
    ImGui::TextDisabled("Nothing selected.");
  }

  ImGui::End();
}

} // namespace editor
