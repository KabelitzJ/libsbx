// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/properties_panel.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <editor/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {


auto path_text(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> std::string {
  const auto path = assets_module.path_of(id);
  return path.empty() ? std::string{"(unknown)"} : path.string();
}

// Just the file name — for compact picker-button labels; path_text's full path is one hover away
// via a tooltip instead.
auto short_label(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> std::string {
  const auto path = assets_module.path_of(id);
  return path.empty() ? std::string{"(unknown)"} : path.filename().string();
}

// path_of() returns whatever path an asset was originally import()-ed with, which is already
// fully resolved (assets_directory() baked in) — not relative to assets_directory() the way
// asset_selection::path and every load_*(path)/save_material destination argument need. Empty
// (unknown asset, or somehow outside assets_directory() entirely) maps to an empty path.
auto relative_asset_path(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> std::filesystem::path {
  const auto path = assets_module.path_of(id);

  if (path.empty()) {
    return {};
  }

  auto& project = sbx::core::engine::project();
  const auto relative = std::filesystem::relative(path, project.assets_directory());

  if (relative.empty() || relative.begin()->string() == "..") {
    return {};
  }

  return relative;
}

// Recursively collects every file under root whose extension matches. Used by the asset pickers
// below — rescanned fresh each time a popup opens (the lists are small; this only runs while a
// popup is open, never per-frame).
auto collect_files_with_extension(const std::filesystem::path& root, std::string_view extension, std::vector<std::filesystem::path>& out) -> void {
  if (!std::filesystem::exists(root)) {
    return;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator{root}) {
    if (!entry.is_directory() && entry.path().extension() == extension) {
      out.push_back(entry.path());
    }
  }
}

// Case-insensitive substring test, for the picker popups' filter boxes below.
auto contains_ignore_case(std::string_view haystack, std::string_view needle) -> bool {
  const auto to_lower = [](std::string_view text) -> std::string {
    auto result = std::string{text};
    std::ranges::transform(result, result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
  };

  return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

// A button showing the slot's current material's file name (full path on hover — see short_label)
// — mesh_renderer.materials is the source of truth, so there's no "override vs default"
// distinction to display. Opens a popup with a filter box, "Reset to Mesh Default" (when
// mesh_default is valid — reseeds the slot from the mesh's own submesh material), and every
// ".material" file under the project's assets directory. A second small button next to it jumps
// Properties straight to that material's own editable view.
auto draw_material_picker(editor_state& state, const char* popup_id, sbx::assets::material_handle& slot, sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& mesh_default = {}) -> void {
  // popup_id (e.g. "##albedo_picker") appended so multiple pickers showing the same label — most
  // commonly several empty "(none)" slots at once — don't collide on ImGui's label-derived ID.
  const auto label = (slot.is_valid() ? short_label(assets_module, slot->id()) : std::string{"(none)"}) + popup_id;

  if (ImGui::Button(label.c_str())) {
    ImGui::OpenPopup(popup_id);
  }

  if (slot.is_valid() && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", path_text(assets_module, slot->id()).c_str());
  }

  if (slot.is_valid()) {
    ImGui::SameLine();

    if (ImGui::Button(fmt::format("{}{}_edit", std::string{ICON_MDI_FILE_EDIT_OUTLINE}, popup_id).c_str())) {
      state.select_asset(slot->id(), relative_asset_path(assets_module, slot->id()), asset_kind::material);
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Edit this material");
    }
  }

  // Fixed width so the popup doesn't reflow (and the filter box along with it) as filtering
  // changes which entries — and therefore how wide the widest visible one is — are shown.
  ImGui::SetNextWindowSize(ImVec2{320.0f, 0.0f}, ImGuiCond_Always);

  if (ImGui::BeginPopup(popup_id)) {
    // Reset whenever a *different* picker's popup opens, so leftover text doesn't carry over.
    static auto filter_buffer = std::array<char, 128u>{};
    static auto last_popup_id = std::string{};

    if (last_popup_id != popup_id) {
      filter_buffer[0] = '\0';
      last_popup_id = popup_id;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filter", "Filter...", filter_buffer.data(), filter_buffer.size());

    if (mesh_default.is_valid() && ImGui::MenuItem("Reset to Mesh Default")) {
      slot = mesh_default;
    }

    auto& project = sbx::core::engine::project();
    auto files = std::vector<std::filesystem::path>{};
    collect_files_with_extension(project.assets_directory(), ".material", files);

    for (const auto& file : files) {
      const auto relative = std::filesystem::relative(file, project.assets_directory());
      const auto relative_string = relative.string();

      if (filter_buffer[0] != '\0' && !contains_ignore_case(relative_string, filter_buffer.data())) {
        continue;
      }

      if (ImGui::MenuItem(relative_string.c_str())) {
        // load_material(path) resolves relative against assets_directory() internally and
        // reuses the file's real uuid if it's already imported — calling import(relative)
        // directly here would mint a second, broken uuid keyed on an unresolved path.
        slot = assets_module.load_material(relative);
      }
    }

    ImGui::EndPopup();
  }
}

// Forks a material into a new, independent .material asset next to the mesh — so editing the copy
// doesn't affect every other node sharing the original (materials are otherwise shared by
// reference: mesh_renderer.materials[i] usually starts out pointing at the exact same asset every
// other instance of that mesh does). Mirrors asset_browser_panel's "New Material" dedup-by-suffix
// naming; mesh_id may be nil (falls back to the assets root as the destination directory).
auto extract_material_to_asset(sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& source, const sbx::math::uuid& mesh_id) -> sbx::assets::material_handle {
  auto& project = sbx::core::engine::project();

  const auto directory = relative_asset_path(assets_module, mesh_id).parent_path();

  auto info = sbx::assets::material::create_info{};
  info.name = source->name();
  info.base_color_factor = source->base_color_factor();
  info.emissive_factor = source->emissive_factor();
  info.metallic_factor = source->metallic_factor();
  info.roughness_factor = source->roughness_factor();
  info.alpha = source->alpha();
  info.alpha_cutoff = source->alpha_cutoff();
  info.is_double_sided = source->is_double_sided();
  info.albedo = source->albedo();
  info.normal = source->normal();
  info.metallic_roughness = source->metallic_roughness();
  info.occlusion = source->occlusion();
  info.emissive = source->emissive();

  auto file_name = info.name + ".material";
  auto suffix = 1;

  while (std::filesystem::exists(project.assets_directory() / directory / file_name)) {
    file_name = fmt::format("{} {}.material", info.name, suffix++);
  }

  auto handle = assets_module.create_material(info);
  assets_module.save_material(handle, directory / file_name);

  return handle;
}

// Same idea as draw_material_picker, for a material's texture slots. format matches the same
// per-slot convention assets_module::load_material already uses (srgb for albedo/emissive, unorm
// for normal/metallic_roughness/occlusion).
auto draw_texture_picker(const char* popup_id, sbx::assets::texture_handle& slot, sbx::assets::assets_module& assets_module, sbx::graphics::format format) -> bool {
  auto changed = false;

  // popup_id appended so multiple pickers showing the same label (most commonly several empty
  // "(none)" slots at once) don't collide on ImGui's label-derived ID.
  const auto label = (slot.is_valid() ? short_label(assets_module, slot->id()) : std::string{"(none)"}) + popup_id;

  if (ImGui::Button(label.c_str())) {
    ImGui::OpenPopup(popup_id);
  }

  if (slot.is_valid() && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", path_text(assets_module, slot->id()).c_str());
  }

  // Fixed width so the popup doesn't reflow (and the filter box along with it) as filtering
  // changes which entries — and therefore how wide the widest visible one is — are shown.
  ImGui::SetNextWindowSize(ImVec2{320.0f, 0.0f}, ImGuiCond_Always);

  if (ImGui::BeginPopup(popup_id)) {
    // Reset whenever a *different* picker's popup opens, so leftover text doesn't carry over.
    static auto filter_buffer = std::array<char, 128u>{};
    static auto last_popup_id = std::string{};

    if (last_popup_id != popup_id) {
      filter_buffer[0] = '\0';
      last_popup_id = popup_id;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filter", "Filter...", filter_buffer.data(), filter_buffer.size());

    if (ImGui::MenuItem("(None)")) {
      slot = sbx::assets::texture_handle{};
      changed = true;
    }

    auto& project = sbx::core::engine::project();
    auto files = std::vector<std::filesystem::path>{};
    collect_files_with_extension(project.assets_directory(), ".png", files);
    collect_files_with_extension(project.assets_directory(), ".jpg", files);
    collect_files_with_extension(project.assets_directory(), ".jpeg", files);

    for (const auto& file : files) {
      const auto relative = std::filesystem::relative(file, project.assets_directory());
      const auto relative_string = relative.string();

      if (filter_buffer[0] != '\0' && !contains_ignore_case(relative_string, filter_buffer.data())) {
        continue;
      }

      if (ImGui::MenuItem(relative_string.c_str())) {
        slot = assets_module.load_texture(relative, format);
        changed = true;
      }
    }

    ImGui::EndPopup();
  }

  return changed;
}

// Same idea as draw_material_picker, for mesh_renderer.mesh. Doesn't touch renderer.materials
// itself when a different mesh is picked — draw_mesh_renderer_section detects the change and
// clears it so sync_materials_with_mesh reseeds cleanly from the new mesh's own submeshes, rather
// than keeping stale entries left over from whatever mesh was assigned before.
auto draw_mesh_picker(editor_state& state, const char* popup_id, sbx::assets::mesh_handle& slot, sbx::assets::assets_module& assets_module) -> void {
  const auto label = (slot.is_valid() ? short_label(assets_module, slot->id()) : std::string{"(none)"}) + popup_id;

  if (ImGui::Button(label.c_str())) {
    ImGui::OpenPopup(popup_id);
  }

  if (slot.is_valid() && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", path_text(assets_module, slot->id()).c_str());
  }

  if (slot.is_valid()) {
    ImGui::SameLine();

    if (ImGui::Button(fmt::format("{}{}_edit", std::string{ICON_MDI_FILE_EDIT_OUTLINE}, popup_id).c_str())) {
      state.select_asset(slot->id(), relative_asset_path(assets_module, slot->id()), asset_kind::mesh);
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Edit this mesh");
    }
  }

  // Fixed width so the popup doesn't reflow (and the filter box along with it) as filtering
  // changes which entries — and therefore how wide the widest visible one is — are shown.
  ImGui::SetNextWindowSize(ImVec2{320.0f, 0.0f}, ImGuiCond_Always);

  if (ImGui::BeginPopup(popup_id)) {
    // Reset whenever a *different* picker's popup opens, so leftover text doesn't carry over.
    static auto filter_buffer = std::array<char, 128u>{};
    static auto last_popup_id = std::string{};

    if (last_popup_id != popup_id) {
      filter_buffer[0] = '\0';
      last_popup_id = popup_id;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filter", "Filter...", filter_buffer.data(), filter_buffer.size());

    auto& project = sbx::core::engine::project();
    auto files = std::vector<std::filesystem::path>{};
    collect_files_with_extension(project.assets_directory(), ".gltf", files);
    collect_files_with_extension(project.assets_directory(), ".glb", files);

    for (const auto& file : files) {
      const auto relative = std::filesystem::relative(file, project.assets_directory());
      const auto relative_string = relative.string();

      if (filter_buffer[0] != '\0' && !contains_ignore_case(relative_string, filter_buffer.data())) {
        continue;
      }

      if (ImGui::MenuItem(relative_string.c_str())) {
        slot = assets_module.load_mesh(relative);
      }
    }

    ImGui::EndPopup();
  }
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

auto draw_color_field(const char* label, sbx::math::color& color) -> bool {
  auto value = std::array<std::float_t, 4u>{color.r(), color.g(), color.b(), color.a()};

  if (!ImGui::ColorEdit4(label, value.data())) {
    return false;
  }

  color.r() = value[0];
  color.g() = value[1];
  color.b() = value[2];
  color.a() = value[3];

  return true;
}

auto draw_camera_section(sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_CAMERA_OUTLINE " Camera", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    node.remove_component<sbx::scenes::camera>();
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& camera = node.get_component<sbx::scenes::camera>();

  ImGui::DragFloat("FOV (degrees)", &camera.fov_degrees, 0.5f, 1.0f, 179.0f);
  ImGui::DragFloat("Near Plane", &camera.near_plane, 0.01f, 0.001f, camera.far_plane);
  ImGui::DragFloat("Far Plane", &camera.far_plane, 1.0f, camera.near_plane, 100000.0f);
}

auto draw_mesh_renderer_section(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_CUBE_OUTLINE " Mesh Renderer", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    node.remove_component<sbx::scenes::mesh_renderer>();
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& renderer = node.get_component<sbx::scenes::mesh_renderer>();

  const auto previous_mesh_id = renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil();

  ImGui::Text("Mesh:");
  ImGui::SameLine();
  draw_mesh_picker(state, "##mesh_picker_popup", renderer.mesh, assets_module);

  const auto new_mesh_id = renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil();

  if (new_mesh_id != previous_mesh_id) {
    // A different mesh was just picked — its own submeshes' materials should take over cleanly,
    // not share slots (by index) with whatever the previous mesh happened to have.
    renderer.materials.clear();
  }

  sbx::scenes::sync_materials_with_mesh(renderer);

  if (renderer.mesh.is_valid()) {
    ImGui::Text("Submeshes: %zu", renderer.mesh->submeshes().size());
  } else {
    ImGui::TextDisabled("No mesh assigned.");
  }

  for (auto index = std::size_t{0u}; index < renderer.materials.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));

    auto& slot = renderer.materials[index];
    const auto mesh_default = (renderer.mesh.is_valid() && index < renderer.mesh->submeshes().size())
      ? renderer.mesh->submeshes()[index].material
      : sbx::assets::material_handle{};

    ImGui::Text("Material %zu:", index);
    ImGui::SameLine();
    draw_material_picker(state, "##material_picker_popup", slot, assets_module, mesh_default);

    if (slot.is_valid()) {
      ImGui::SameLine();

      if (ImGui::Button(ICON_MDI_EXPORT_VARIANT " Duplicate")) {
        slot = extract_material_to_asset(assets_module, slot, renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil());
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Fork this slot's material into an independent copy, so editing it only affects this node.");
      }
    }

    ImGui::PopID();
  }
}

auto draw_directional_light_section(sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    node.remove_component<sbx::scenes::directional_light>();
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::directional_light>();

  draw_color_field("Color", light.color);
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
}

auto draw_point_light_section(sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_LIGHTBULB_OUTLINE " Point Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    node.remove_component<sbx::scenes::point_light>();
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::point_light>();

  draw_color_field("Color", light.color);
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 10000.0f);
}

auto draw_spot_light_section(sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_FLASHLIGHT " Spot Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    node.remove_component<sbx::scenes::spot_light>();
    return;
  }

  if (!is_expanded) {
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
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_EARTH " Skybox", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    node.remove_component<sbx::scenes::skybox>();
    return;
  }

  if (!is_expanded) {
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

auto draw_add_component_menu(sbx::scenes::node& node) -> void {
  if (ImGui::Button(ICON_MDI_PLUS " Add Component")) {
    ImGui::OpenPopup("##add_component_popup");
  }

  if (ImGui::BeginPopup("##add_component_popup")) {
    if (!node.has_component<sbx::scenes::camera>() && ImGui::MenuItem(ICON_MDI_CAMERA_OUTLINE " Camera")) {
      node.add_component<sbx::scenes::camera>();
    }

    if (!node.has_component<sbx::scenes::mesh_renderer>() && ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Mesh Renderer")) {
      node.add_component<sbx::scenes::mesh_renderer>();
    }

    if (!node.has_component<sbx::scenes::directional_light>() && ImGui::MenuItem(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light")) {
      node.add_component<sbx::scenes::directional_light>();
    }

    if (!node.has_component<sbx::scenes::point_light>() && ImGui::MenuItem(ICON_MDI_LIGHTBULB_OUTLINE " Point Light")) {
      node.add_component<sbx::scenes::point_light>();
    }

    if (!node.has_component<sbx::scenes::spot_light>() && ImGui::MenuItem(ICON_MDI_FLASHLIGHT " Spot Light")) {
      node.add_component<sbx::scenes::spot_light>();
    }

    if (!node.has_component<sbx::scenes::skybox>() && ImGui::MenuItem(ICON_MDI_EARTH " Skybox")) {
      node.add_component<sbx::scenes::skybox>();
    }

    ImGui::EndPopup();
  }
}


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

auto properties_panel::_draw_node_properties(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  _draw_name_field(node);
  _draw_transform_section(node);

  if (node.has_component<sbx::scenes::camera>()) {
    draw_camera_section(node);
  }

  if (node.has_component<sbx::scenes::mesh_renderer>()) {
    draw_mesh_renderer_section(state, node, assets_module);
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

  ImGui::Separator();
  draw_add_component_menu(node);
}

auto properties_panel::_draw_material_properties(const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (!_asset_cache.material.is_valid()) {
    ImGui::TextDisabled("Could not load this material.");
    return;
  }

  auto changed = false;

  auto name_buffer = std::array<char, 128u>{};
  std::strncpy(name_buffer.data(), _material_edit.name.c_str(), name_buffer.size() - 1u);
  name_buffer[name_buffer.size() - 1u] = '\0';

  if (ImGui::InputText("Name", name_buffer.data(), name_buffer.size())) {
    _material_edit.name = std::string{name_buffer.data()};
    changed = true;
  }

  changed |= draw_color_field("Base Color", _material_edit.base_color_factor);

  auto emissive = std::array<std::float_t, 3u>{_material_edit.emissive_factor.x(), _material_edit.emissive_factor.y(), _material_edit.emissive_factor.z()};
  if (ImGui::ColorEdit3("Emissive", emissive.data())) {
    _material_edit.emissive_factor = sbx::math::vector3{emissive[0], emissive[1], emissive[2]};
    changed = true;
  }

  changed |= ImGui::DragFloat("Metallic", &_material_edit.metallic_factor, 0.01f, 0.0f, 1.0f);
  changed |= ImGui::DragFloat("Roughness", &_material_edit.roughness_factor, 0.01f, 0.0f, 1.0f);

  static constexpr auto alpha_mode_names = std::array<const char*, 3u>{"Opaque", "Mask", "Blend"};
  auto alpha_index = static_cast<int>(_material_edit.alpha);

  if (ImGui::Combo("Alpha Mode", &alpha_index, alpha_mode_names.data(), static_cast<int>(alpha_mode_names.size()))) {
    _material_edit.alpha = static_cast<sbx::assets::alpha_mode>(alpha_index);
    changed = true;
  }

  if (_material_edit.alpha == sbx::assets::alpha_mode::mask) {
    changed |= ImGui::DragFloat("Alpha Cutoff", &_material_edit.alpha_cutoff, 0.01f, 0.0f, 1.0f);
  }

  changed |= ImGui::Checkbox("Double Sided", &_material_edit.is_double_sided);

  ImGui::SeparatorText("Textures");

  ImGui::Text("Albedo");
  ImGui::SameLine();
  changed |= draw_texture_picker("##albedo_picker", _material_edit.albedo, assets_module, sbx::graphics::format::r8g8b8a8_srgb);

  ImGui::Text("Normal");
  ImGui::SameLine();
  changed |= draw_texture_picker("##normal_picker", _material_edit.normal, assets_module, sbx::graphics::format::r8g8b8a8_unorm);

  ImGui::Text("Metallic/Roughness");
  ImGui::SameLine();
  changed |= draw_texture_picker("##metallic_roughness_picker", _material_edit.metallic_roughness, assets_module, sbx::graphics::format::r8g8b8a8_unorm);

  ImGui::Text("Occlusion");
  ImGui::SameLine();
  changed |= draw_texture_picker("##occlusion_picker", _material_edit.occlusion, assets_module, sbx::graphics::format::r8g8b8a8_unorm);

  ImGui::Text("Emissive");
  ImGui::SameLine();
  changed |= draw_texture_picker("##emissive_picker", _material_edit.emissive, assets_module, sbx::graphics::format::r8g8b8a8_srgb);

  if (changed) {
    // Live preview: mutates the material in place immediately, so every material_handle already
    // pointing at it (e.g. a mesh_renderer slot elsewhere in the scene) reflects the edit on the
    // very next frame — no Save needed to see it. Persisting to disk stays an explicit action
    // (below): writing the file on every drag tick/keystroke would be wasteful.
    assets_module.update_material(_asset_cache.material, _material_edit);
  }

  ImGui::Separator();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_material(_asset_cache.material, asset.path);
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

    if (asset.kind == asset_kind::material && _asset_cache.material.is_valid()) {
      const auto& material = *_asset_cache.material;

      _material_edit.name = material.name();
      _material_edit.base_color_factor = material.base_color_factor();
      _material_edit.emissive_factor = material.emissive_factor();
      _material_edit.metallic_factor = material.metallic_factor();
      _material_edit.roughness_factor = material.roughness_factor();
      _material_edit.alpha = material.alpha();
      _material_edit.alpha_cutoff = material.alpha_cutoff();
      _material_edit.is_double_sided = material.is_double_sided();
      _material_edit.albedo = material.albedo();
      _material_edit.normal = material.normal();
      _material_edit.metallic_roughness = material.metallic_roughness();
      _material_edit.occlusion = material.occlusion();
      _material_edit.emissive = material.emissive();
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
      _draw_material_properties(asset, assets_module);
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
      _draw_node_properties(state, node, assets_module);
    } else {
      // The selected node no longer exists (e.g. deleted); fall back to the empty state.
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
