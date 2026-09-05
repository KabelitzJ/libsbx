// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_panel.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/rigidbody.hpp>

#include <libsbx/scripting/scripting_module.hpp>
#include <libsbx/scripting/managed/type.hpp>
#include <libsbx/scripting/managed/field_info.hpp>
#include <libsbx/scripting/managed/attribute.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/scene_commands.hpp>
#include <editor/commands/script_commands.hpp>

namespace editor {

// Brackets a continuous-drag-style edit (DragFloat, ColorEdit4, SliderAngle, InputText, ...) into
// one undo entry per completed edit rather than one per frame: call every frame right after the
// widget. `pending` must outlive one full activate/deactivate cycle (a caller's function-local
// static) — one shared instance per section is enough since ImGui only has one active item at a time.
template<typename Component>
auto bracket_edit(editor_state& state, const sbx::scenes::node& node, const Component& component, std::optional<Component>& pending, const char* label) -> void {
  if (ImGui::IsItemActivated() && !pending) {
    pending = component;
  }

  if (ImGui::IsItemDeactivatedAfterEdit() && pending) {
    state.push_command(std::make_unique<modify_component_command<Component>>(node.id(), *pending, component, label));
    pending.reset();
  }
}


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

// path_of() is fully resolved, not relative to assets_directory() like asset_selection::path and
// load_*/save_material need. Empty on unknown/outside-assets_directory() ids.
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

// Used by the asset pickers below — rescanned fresh each time a popup opens (lists are small).
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

// Button shows the slot's material file name; opens a popup with a filter, an optional
// "Reset to Mesh Default" (reseeds from the mesh's own submesh material), and every .material
// file under assets. Second button jumps Properties to that material's editable view.
auto draw_material_picker(editor_state& state, const char* popup_id, sbx::assets::material_handle& slot, sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& mesh_default = {}) -> bool {
  auto changed = false;

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
      changed = true;
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
        changed = true;
      }
    }

    ImGui::EndPopup();
  }

  return changed;
}

// Forks a material into a new, independent .material asset next to the mesh, so editing the copy
// doesn't affect other nodes sharing the original. mesh_id may be nil (falls back to assets root).
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
  info.casts_shadow = source->casts_shadow();
  info.receives_shadow = source->receives_shadow();
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

// Same idea as draw_material_picker, for texture slots. format follows load_material's per-slot
// convention (srgb for albedo/emissive, unorm for normal/metallic_roughness/occlusion).
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
// itself — draw_mesh_renderer_section detects the change and clears it so sync_materials_with_mesh
// reseeds cleanly from the new mesh's submeshes.
auto draw_mesh_picker(editor_state& state, const char* popup_id, sbx::assets::mesh_handle& slot, sbx::assets::assets_module& assets_module) -> bool {
  auto changed = false;

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
        changed = true;
      }
    }

    ImGui::EndPopup();
  }

  return changed;
}

// Same idea as draw_mesh_picker, for particle_effect.effect — same jump-to-edit button, since
// particle_effect assets are edited in place (see _draw_particle_effect_properties) like materials.
auto draw_particle_effect_picker(editor_state& state, const char* popup_id, sbx::assets::particle_effect_handle& slot, sbx::assets::assets_module& assets_module) -> bool {
  auto changed = false;

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
      state.select_asset(slot->id(), relative_asset_path(assets_module, slot->id()), asset_kind::particle_effect);
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Edit this particle effect");
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

    if (ImGui::MenuItem("(None)")) {
      slot = sbx::assets::particle_effect_handle{};
      changed = true;
    }

    auto& project = sbx::core::engine::project();
    auto files = std::vector<std::filesystem::path>{};
    collect_files_with_extension(project.assets_directory(), ".particle_effect", files);

    for (const auto& file : files) {
      const auto relative = std::filesystem::relative(file, project.assets_directory());
      const auto relative_string = relative.string();

      if (filter_buffer[0] != '\0' && !contains_ignore_case(relative_string, filter_buffer.data())) {
        continue;
      }

      if (ImGui::MenuItem(relative_string.c_str())) {
        slot = assets_module.load_particle_effect(relative);
        changed = true;
      }
    }

    ImGui::EndPopup();
  }

  return changed;
}

// changed: any axis changed this frame. started/committed: whether a drag (or a same-frame reset
// button click, which is its own complete started+committed gesture) began/finished this frame —
// callers use these to bracket the whole X/Y/Z row into one undo entry instead of one per frame.
struct vector3_edit_result {
  bool changed{false};
  bool started{false};
  bool committed{false};
}; // struct vector3_edit_result

// Color-coded X/Y/Z row: click an axis button to reset it to reset_value, followed by its drag field.
auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, std::float_t reset_value, std::float_t speed) -> vector3_edit_result {
  static constexpr auto axis_labels = std::array<const char*, 3u>{"X", "Y", "Z"};
  static constexpr auto axis_ids = std::array<const char*, 3u>{"##X", "##Y", "##Z"};
  static constexpr auto axis_colors = std::array<ImVec4, 3u>{
    ImVec4{0.75f, 0.20f, 0.25f, 1.0f}, // X - red
    ImVec4{0.30f, 0.65f, 0.30f, 1.0f}, // Y - green
    ImVec4{0.20f, 0.45f, 0.80f, 1.0f}, // Z - blue
  };

  auto result = vector3_edit_result{};

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
      result.changed = true;
      result.started = true;
      result.committed = true;
    }

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(item_width);
    result.changed |= ImGui::DragFloat(axis_ids[axis], &values[axis], speed);
    result.started |= ImGui::IsItemActivated();
    result.committed |= ImGui::IsItemDeactivatedAfterEdit();
  }

  ImGui::PopStyleVar();
  ImGui::PopID();

  return result;
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

// A plain key-list editor rather than an interactive curve graph -- keys don't need to be authored
// in time order (assets::curve::evaluate() finds the bracketing pair regardless), so this is enough
// to author any curve the graph widget would produce, just less visual.
auto draw_curve_editor(const char* label, sbx::assets::curve& curve, std::float_t value_min, std::float_t value_max) -> bool {
  auto changed = false;

  if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Framed)) {
    return false;
  }

  auto& keys = curve.keys;
  auto removed_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < keys.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& key = keys[index];

    changed |= ImGui::SliderFloat("Time", &key.time, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("Value", &key.value, 0.01f, value_min, value_max);

    if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Key")) {
      removed_index = index;
      changed = true;
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  if (removed_index) {
    // static_vector has no erase() -- shift the tail down by hand, same as the collision plane list.
    for (auto i = *removed_index; i + 1u < keys.size(); ++i) {
      keys[i] = keys[i + 1u];
    }

    keys.pop_back();
  }

  ImGui::BeginDisabled(keys.size() >= sbx::assets::curve_max_keys);

  if (ImGui::Button(ICON_MDI_PLUS " Add Key")) {
    keys.push_back(sbx::assets::curve_key{});
    changed = true;
  }

  ImGui::EndDisabled();

  ImGui::TreePop();

  return changed;
}

auto draw_gradient_editor(const char* label, sbx::assets::gradient& gradient) -> bool {
  auto changed = false;

  if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Framed)) {
    return false;
  }

  ImGui::Text("Color Keys");
  ImGui::PushID("color_keys");

  auto& color_keys = gradient.color_keys;
  auto removed_color_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < color_keys.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& key = color_keys[index];

    changed |= ImGui::SliderFloat("Time", &key.time, 0.0f, 1.0f);
    changed |= draw_color_field("Color", key.color);

    if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Color Key")) {
      removed_color_index = index;
      changed = true;
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  if (removed_color_index) {
    for (auto i = *removed_color_index; i + 1u < color_keys.size(); ++i) {
      color_keys[i] = color_keys[i + 1u];
    }

    color_keys.pop_back();
  }

  ImGui::BeginDisabled(color_keys.size() >= sbx::assets::gradient_max_keys);

  if (ImGui::Button(ICON_MDI_PLUS " Add Color Key")) {
    color_keys.push_back(sbx::assets::gradient_color_key{});
    changed = true;
  }

  ImGui::EndDisabled();
  ImGui::PopID();

  ImGui::Text("Alpha Keys");
  ImGui::PushID("alpha_keys");

  auto& alpha_keys = gradient.alpha_keys;
  auto removed_alpha_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < alpha_keys.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& key = alpha_keys[index];

    changed |= ImGui::SliderFloat("Time", &key.time, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Alpha", &key.alpha, 0.0f, 1.0f);

    if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Alpha Key")) {
      removed_alpha_index = index;
      changed = true;
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  if (removed_alpha_index) {
    for (auto i = *removed_alpha_index; i + 1u < alpha_keys.size(); ++i) {
      alpha_keys[i] = alpha_keys[i + 1u];
    }

    alpha_keys.pop_back();
  }

  ImGui::BeginDisabled(alpha_keys.size() >= sbx::assets::gradient_max_keys);

  if (ImGui::Button(ICON_MDI_PLUS " Add Alpha Key")) {
    alpha_keys.push_back(sbx::assets::gradient_alpha_key{});
    changed = true;
  }

  ImGui::EndDisabled();
  ImGui::PopID();

  ImGui::TreePop();

  return changed;
}

auto draw_camera_section(editor_state& state, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_CAMERA_OUTLINE " Camera", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::scenes::camera>>(node.id(), node.get_component<sbx::scenes::camera>(), "Remove Camera"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& camera = node.get_component<sbx::scenes::camera>();
  static auto pending = std::optional<sbx::scenes::camera>{};

  ImGui::DragFloat("FOV (degrees)", &camera.fov_degrees, 0.5f, 1.0f, 179.0f);
  bracket_edit(state, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Near Plane", &camera.near_plane, 0.01f, 0.001f, camera.far_plane);
  bracket_edit(state, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Far Plane", &camera.far_plane, 1.0f, camera.near_plane, 100000.0f);
  bracket_edit(state, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Exposure", &camera.exposure, 0.05f, -8.0f, 8.0f);
  bracket_edit(state, node, camera, pending, "Edit Camera");

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  const auto is_active_camera = scene.has_active_camera() && scene.active_camera().id() == node.id();

  if (is_active_camera) {
    ImGui::BeginDisabled();
    ImGui::Button("Active Camera");
    ImGui::EndDisabled();
  } else if (ImGui::Button("Set as Active Camera")) {
    state.push_command(std::make_unique<set_active_camera_command>(node));
  }
}

auto draw_mesh_renderer_section(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_CUBE_OUTLINE " Mesh Renderer", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::scenes::mesh_renderer>>(node.id(), node.get_component<sbx::scenes::mesh_renderer>(), "Remove Mesh Renderer"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& renderer = node.get_component<sbx::scenes::mesh_renderer>();

  // Resolved within this single frame (popup-based pickers, not a multi-frame drag) — snapshot
  // once up front and push at most one command at the end if anything below actually changed.
  const auto before = renderer;
  auto changed = false;

  const auto previous_mesh_id = renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil();

  ImGui::Text("Mesh:");
  ImGui::SameLine();
  changed |= draw_mesh_picker(state, "##mesh_picker_popup", renderer.mesh, assets_module);

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
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& slot = renderer.materials[index];
    const auto mesh_default = (renderer.mesh.is_valid() && index < renderer.mesh->submeshes().size())
      ? renderer.mesh->submeshes()[index].material
      : sbx::assets::material_handle{};

    ImGui::Text("Material %zu:", index);
    ImGui::SameLine();
    changed |= draw_material_picker(state, "##material_picker_popup", slot, assets_module, mesh_default);

    if (slot.is_valid()) {
      ImGui::SameLine();

      if (ImGui::Button(ICON_MDI_EXPORT_VARIANT " Duplicate")) {
        slot = extract_material_to_asset(assets_module, slot, renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil());
        changed = true;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Fork this slot's material into an independent copy, so editing it only affects this node.");
      }
    }

    ImGui::PopID();
  }

  if (changed) {
    state.push_command(std::make_unique<modify_component_command<sbx::scenes::mesh_renderer>>(node.id(), before, renderer, "Edit Mesh Renderer"));
  }
}

auto draw_directional_light_section(editor_state& state, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::scenes::directional_light>>(node.id(), node.get_component<sbx::scenes::directional_light>(), "Remove Directional Light"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::directional_light>();
  static auto pending = std::optional<sbx::scenes::directional_light>{};

  draw_color_field("Color", light.color);
  bracket_edit(state, node, light, pending, "Edit Directional Light");
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  bracket_edit(state, node, light, pending, "Edit Directional Light");

  {
    const auto before = light;
    if (ImGui::Checkbox("Casts Shadows", &light.casts_shadows)) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::directional_light>>(node.id(), before, light, "Edit Directional Light"));
    }
  }

  ImGui::DragFloat("Shadow Distance", &light.shadow_distance, 0.5f, 1.0f, 1000.0f);
  bracket_edit(state, node, light, pending, "Edit Directional Light");
}

auto draw_point_light_section(editor_state& state, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_LIGHTBULB_OUTLINE " Point Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::scenes::point_light>>(node.id(), node.get_component<sbx::scenes::point_light>(), "Remove Point Light"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::point_light>();
  static auto pending = std::optional<sbx::scenes::point_light>{};

  draw_color_field("Color", light.color);
  bracket_edit(state, node, light, pending, "Edit Point Light");
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  bracket_edit(state, node, light, pending, "Edit Point Light");
  ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 10000.0f);
  bracket_edit(state, node, light, pending, "Edit Point Light");
}

auto draw_spot_light_section(editor_state& state, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_FLASHLIGHT " Spot Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::scenes::spot_light>>(node.id(), node.get_component<sbx::scenes::spot_light>(), "Remove Spot Light"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::spot_light>();
  static auto pending = std::optional<sbx::scenes::spot_light>{};

  draw_color_field("Color", light.color);
  bracket_edit(state, node, light, pending, "Edit Spot Light");
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  bracket_edit(state, node, light, pending, "Edit Spot Light");
  ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 10000.0f);
  bracket_edit(state, node, light, pending, "Edit Spot Light");

  // inner_angle/outer_angle are stored in radians; SliderAngle operates on a radians pointer
  // while displaying/editing in degrees, so these bind directly — no manual conversion.
  ImGui::SliderAngle("Inner Angle", &light.inner_angle, 0.0f, 90.0f);
  bracket_edit(state, node, light, pending, "Edit Spot Light");
  ImGui::SliderAngle("Outer Angle", &light.outer_angle, 0.0f, 90.0f);
  bracket_edit(state, node, light, pending, "Edit Spot Light");
}

auto draw_skybox_section(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_EARTH " Skybox", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::scenes::skybox>>(node.id(), node.get_component<sbx::scenes::skybox>(), "Remove Skybox"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& sky = node.get_component<sbx::scenes::skybox>();
  static auto pending = std::optional<sbx::scenes::skybox>{};

  if (sky.environment.is_valid()) {
    ImGui::Text("Environment: %s", path_text(assets_module, sky.environment->id()).c_str());

    const auto* environment = sky.environment.get();
    const auto is_baked = environment->radiance_index() != sbx::assets::environment_map::invalid_index && environment->irradiance_index() != sbx::assets::environment_map::invalid_index && environment->prefiltered_index() != sbx::assets::environment_map::invalid_index;
    ImGui::Text("Baked: %s", is_baked ? "yes" : "no");
  } else {
    ImGui::TextDisabled("Environment: (none)");
  }

  ImGui::DragFloat("Background Intensity", &sky.intensity, 0.05f, 0.0f, 100.0f);
  bracket_edit(state, node, sky, pending, "Edit Skybox");
  ImGui::DragFloat("Ambient Intensity", &sky.ambient_intensity, 0.05f, 0.0f, 100.0f);
  bracket_edit(state, node, sky, pending, "Edit Skybox");
}

auto draw_particle_effect_instance_section(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_FIREWORK " Particle Effect", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::scenes::particle_effect>>(node.id(), node.get_component<sbx::scenes::particle_effect>(), "Remove Particle Effect"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& instance = node.get_component<sbx::scenes::particle_effect>();

  ImGui::Text("Effect:");
  ImGui::SameLine();

  {
    const auto before = instance;

    if (draw_particle_effect_picker(state, "##particle_effect_picker_popup", instance.effect, assets_module)) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::particle_effect>>(node.id(), before, instance, "Edit Particle Effect"));
    }
  }

  if (instance.effect.is_valid()) {
    ImGui::Text("Emitters: %zu", instance.effect->emitters().size());
  } else {
    ImGui::TextDisabled("No effect assigned.");
  }

  {
    const auto before = instance;

    if (ImGui::Checkbox("Loop", &instance.loop)) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::particle_effect>>(node.id(), before, instance, "Edit Particle Effect"));
    }

    ImGui::BeginDisabled(instance.loop);

    if (ImGui::DragFloat("Duration", &instance.duration, 0.05f, 0.01f, 3600.0f)) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::particle_effect>>(node.id(), before, instance, "Edit Particle Effect"));
    }

    ImGui::EndDisabled();
  }

  ImGui::SeparatorText("Playback");

  // Play/Pause/Stop below are transport controls, not authored data — deliberately excluded from
  // undo/redo (only Loop above is tracked).

  const auto is_playing = instance.playback == sbx::scenes::particle_playback_state::playing;
  const auto is_stopped = instance.playback == sbx::scenes::particle_playback_state::stopped;

  ImGui::BeginDisabled(!instance.effect.is_valid());

  if (is_playing) {
    if (ImGui::Button(ICON_MDI_PAUSE " Pause")) {
      instance.playback = sbx::scenes::particle_playback_state::paused;
    }
  } else if (ImGui::Button(ICON_MDI_PLAY " Play")) {
    instance.playback = sbx::scenes::particle_playback_state::playing;
  }

  ImGui::EndDisabled();

  ImGui::SameLine();

  ImGui::BeginDisabled(is_stopped);

  if (ImGui::Button(ICON_MDI_STOP " Stop")) {
    // Only flips playback -- particles_module::fixed_update() notices next frame and clears every
    // emitter's particle array on its own.
    instance.playback = sbx::scenes::particle_playback_state::stopped;
    instance.elapsed = 0.0f;
  }

  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", is_playing ? "Playing" : is_stopped ? "Stopped" : "Paused");
}

auto draw_rigidbody_section(editor_state& state, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_SOCCER " Rigidbody", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::physics::rigidbody>>(node.id(), node.get_component<sbx::physics::rigidbody>(), "Remove Rigidbody"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& body = node.get_component<sbx::physics::rigidbody>();
  static auto pending = std::optional<sbx::physics::rigidbody>{};

  static constexpr auto body_type_names = std::array<const char*, 3u>{"Dynamic", "Kinematic", "Static"};
  const auto current_index = static_cast<std::size_t>(body.type);

  if (ImGui::BeginCombo("Body Type", body_type_names[current_index])) {
    for (auto index = std::size_t{0u}; index < body_type_names.size(); ++index) {
      const auto is_selected = (index == current_index);

      if (ImGui::Selectable(body_type_names[index], is_selected) && index != current_index) {
        const auto before = body;
        body.type = static_cast<sbx::physics::body_type>(index);
        state.push_command(std::make_unique<modify_component_command<sbx::physics::rigidbody>>(node.id(), before, body, "Edit Rigidbody"));
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  // inverse_mass is the stored source of truth (see rigidbody's doc comment) — the field here
  // just presents/edits its reciprocal.
  auto mass = (body.inverse_mass > 0.0f) ? (1.0f / body.inverse_mass) : 0.0f;

  if (ImGui::DragFloat("Mass (kg)", &mass, 0.05f, 0.0f, 100000.0f)) {
    body.inverse_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
  }

  bracket_edit(state, node, body, pending, "Edit Rigidbody");

  ImGui::DragFloat("Linear Damping", &body.linear_damping, 0.005f, 0.0f, 10.0f);
  bracket_edit(state, node, body, pending, "Edit Rigidbody");
  ImGui::DragFloat("Angular Damping", &body.angular_damping, 0.005f, 0.0f, 10.0f);
  bracket_edit(state, node, body, pending, "Edit Rigidbody");
  ImGui::DragFloat("Gravity Scale", &body.gravity_scale, 0.05f, -10.0f, 10.0f);
  bracket_edit(state, node, body, pending, "Edit Rigidbody");

  auto linear_velocity = std::array<std::float_t, 3u>{body.linear_velocity.x(), body.linear_velocity.y(), body.linear_velocity.z()};

  if (ImGui::DragFloat3("Linear Velocity", linear_velocity.data(), 0.05f)) {
    body.linear_velocity = sbx::math::vector3{linear_velocity[0], linear_velocity[1], linear_velocity[2]};
  }

  bracket_edit(state, node, body, pending, "Edit Rigidbody");

  auto angular_velocity = std::array<std::float_t, 3u>{body.angular_velocity.x(), body.angular_velocity.y(), body.angular_velocity.z()};

  if (ImGui::DragFloat3("Angular Velocity", angular_velocity.data(), 0.05f)) {
    body.angular_velocity = sbx::math::vector3{angular_velocity[0], angular_velocity[1], angular_velocity[2]};
  }

  bracket_edit(state, node, body, pending, "Edit Rigidbody");
}

// offset/rotation are shared by shape_collider and mesh_collider — same fields, same widgets.
template<typename Collider>
auto draw_collider_offset_rotation_friction(editor_state& state, sbx::scenes::node& node, Collider& collider, std::optional<Collider>& pending, const char* label) -> void {
  auto offset = std::array<std::float_t, 3u>{collider.offset.x(), collider.offset.y(), collider.offset.z()};

  if (ImGui::DragFloat3("Offset", offset.data(), 0.05f)) {
    collider.offset = sbx::math::vector3{offset[0], offset[1], offset[2]};
  }

  bracket_edit(state, node, collider, pending, label);

  const auto euler = sbx::math::quaternion::euler_angles(collider.rotation);
  auto rotation_degrees = std::array<std::float_t, 3u>{euler.x(), euler.y(), euler.z()};

  if (ImGui::DragFloat3("Rotation", rotation_degrees.data(), 0.5f)) {
    collider.rotation = sbx::math::quaternion{sbx::math::vector3{rotation_degrees[0], rotation_degrees[1], rotation_degrees[2]}};
  }

  bracket_edit(state, node, collider, pending, label);

  ImGui::DragFloat("Friction", &collider.friction, 0.01f, 0.0f, 10.0f);
  bracket_edit(state, node, collider, pending, label);
  ImGui::DragFloat("Restitution", &collider.restitution, 0.01f, 0.0f, 1.0f);
  bracket_edit(state, node, collider, pending, label);
}

auto draw_shape_collider_section(editor_state& state, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_SHAPE_OUTLINE " Shape Collider", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::physics::shape_collider>>(node.id(), node.get_component<sbx::physics::shape_collider>(), "Remove Shape Collider"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& collider = node.get_component<sbx::physics::shape_collider>();
  static auto pending = std::optional<sbx::physics::shape_collider>{};

  if (node.has_component<sbx::physics::mesh_collider>()) {
    // Narrowphase only resolves one collider per node (shape_collider wins; see
    // narrowphase.cpp's resolve_convex) — Add Component already blocks creating both.
    ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, ICON_MDI_ALERT_OUTLINE " Also has a Mesh Collider -- it will be ignored.");
  }

  static constexpr auto shape_names = std::array<const char*, 4u>{"Sphere", "Cylinder", "Capsule", "Box"};
  const auto current_index = std::min(collider.shape.index(), std::size_t{3u}); // clamp: index 4 (triangle) never legitimately appears here

  if (ImGui::BeginCombo("Shape", shape_names[current_index])) {
    for (auto index = std::size_t{0u}; index < shape_names.size(); ++index) {
      const auto is_selected = (index == current_index);

      if (ImGui::Selectable(shape_names[index], is_selected) && index != current_index) {
        const auto before = collider;

        switch (index) {
          case 0u: collider.shape = sbx::physics::sphere{}; break;
          case 1u: collider.shape = sbx::physics::cylinder{}; break;
          case 2u: collider.shape = sbx::physics::capsule{}; break;
          case 3u: collider.shape = sbx::physics::box{}; break;
          default: break;
        }

        state.push_command(std::make_unique<modify_component_command<sbx::physics::shape_collider>>(node.id(), before, collider, "Change Collider Shape"));
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  if (auto* sphere = std::get_if<sbx::physics::sphere>(&collider.shape)) {
    ImGui::DragFloat("Radius", &sphere->radius, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, node, collider, pending, "Edit Shape Collider");
  } else if (auto* cylinder = std::get_if<sbx::physics::cylinder>(&collider.shape)) {
    ImGui::DragFloat("Radius", &cylinder->radius, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, node, collider, pending, "Edit Shape Collider");
    ImGui::DragFloat("Half Height", &cylinder->half_height, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, node, collider, pending, "Edit Shape Collider");
  } else if (auto* capsule = std::get_if<sbx::physics::capsule>(&collider.shape)) {
    ImGui::DragFloat("Radius", &capsule->radius, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, node, collider, pending, "Edit Shape Collider");
    ImGui::DragFloat("Half Height", &capsule->half_height, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, node, collider, pending, "Edit Shape Collider");
  } else if (auto* box = std::get_if<sbx::physics::box>(&collider.shape)) {
    auto half_extents = std::array<std::float_t, 3u>{box->half_extents.x(), box->half_extents.y(), box->half_extents.z()};

    if (ImGui::DragFloat3("Half Extents", half_extents.data(), 0.05f, 0.001f, 1000.0f)) {
      box->half_extents = sbx::math::vector3{half_extents[0], half_extents[1], half_extents[2]};
    }

    bracket_edit(state, node, collider, pending, "Edit Shape Collider");
  }

  draw_collider_offset_rotation_friction(state, node, collider, pending, "Edit Shape Collider");
}

auto draw_mesh_collider_section(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_TERRAIN " Mesh Collider", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(std::make_unique<remove_component_command<sbx::physics::mesh_collider>>(node.id(), node.get_component<sbx::physics::mesh_collider>(), "Remove Mesh Collider"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& collider = node.get_component<sbx::physics::mesh_collider>();
  static auto pending = std::optional<sbx::physics::mesh_collider>{};

  if (node.has_component<sbx::physics::shape_collider>()) {
    // Narrowphase only resolves one collider per node (shape_collider wins; see
    // narrowphase.cpp's resolve_convex) — Add Component already blocks creating both.
    ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, ICON_MDI_ALERT_OUTLINE " Also has a Shape Collider -- this one will be ignored.");
  }

  {
    const auto before = collider;

    ImGui::Text("Mesh:");
    ImGui::SameLine();

    if (draw_mesh_picker(state, "##mesh_collider_picker_popup", collider.mesh, assets_module)) {
      state.push_command(std::make_unique<modify_component_command<sbx::physics::mesh_collider>>(node.id(), before, collider, "Edit Mesh Collider"));
    }
  }

  ImGui::Checkbox("Convex", &collider.is_convex);
  bracket_edit(state, node, collider, pending, "Edit Mesh Collider");

  if (!collider.is_convex) {
    ImGui::TextDisabled("Non-convex: only valid on a static or kinematic rigidbody.");
  }

  draw_collider_offset_rotation_friction(state, node, collider, pending, "Edit Mesh Collider");
}

// Every script class name matching `filter` (case-insensitive substring, see contains_ignore_case) --
// used only to decide whether the Script submenu below has anything to show for the active filter,
// so it can hide itself along with every other entry instead of opening onto an empty list.
auto any_script_matches(sbx::scripting::scripting_module& scripting_module, std::string_view filter) -> bool {
  auto& behavior_type = scripting_module.game_assembly().get_type("Sbx.Core.Behavior");

  for (auto* candidate : scripting_module.game_assembly().get_types()) {
    if (*candidate == behavior_type || !candidate->is_subclass_of(behavior_type)) {
      continue; // skip the base class itself and anything that isn't a Behavior
    }

    if (contains_ignore_case(std::string{candidate->get_full_name()}, filter)) {
      return true;
    }
  }

  return false;
}

auto draw_add_component_menu(editor_state& state, sbx::scenes::node& node, sbx::scripting::scripting_module& scripting_module) -> void {
  static constexpr auto label = ICON_MDI_PLUS " Add Component";

  // Centered horizontally in the panel, rather than left-aligned like a regular control.
  const auto button_width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
  const auto available_width = ImGui::GetContentRegionAvail().x;

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available_width - button_width) * 0.5f));

  if (ImGui::Button(label)) {
    ImGui::OpenPopup("##add_component_popup");
  }

  // Anchored under the button (centered, not wherever the mouse clicked) with a fixed width so it
  // doesn't jump or resize as the filter text changes. Must be set every frame the popup could
  // open -- ImGui only applies a pending SetNextWindowPos/Size to the very next Begin call.
  constexpr auto popup_width = 260.0f;
  const auto button_min = ImGui::GetItemRectMin();
  const auto button_max = ImGui::GetItemRectMax();
  const auto button_center_x = (button_min.x + button_max.x) * 0.5f;

  // Opens upward instead whenever there isn't roughly enough room below in the viewport's work
  // area, so a tall Inspector doesn't push the popup off the bottom of the window. The estimate
  // only has to pick the right side -- the upward case's pivot anchors to the popup's real height.
  constexpr auto estimated_popup_height = 320.0f;

  const auto work_min = ImGui::GetMainViewport()->WorkPos;
  const auto work_max = ImVec2{work_min.x + ImGui::GetMainViewport()->WorkSize.x, work_min.y + ImGui::GetMainViewport()->WorkSize.y};

  const auto opens_upward = (button_max.y + estimated_popup_height) > work_max.y;
  const auto popup_x = std::clamp(button_center_x - popup_width * 0.5f, work_min.x, std::max(work_min.x, work_max.x - popup_width));

  if (opens_upward) {
    // pivot {0, 1}: the given pos is the window's bottom-left corner instead of its top-left, so it
    // grows upward from the button using its own true content height, not the estimate above.
    ImGui::SetNextWindowPos(ImVec2{popup_x, button_min.y}, ImGuiCond_Always, ImVec2{0.0f, 1.0f});
  } else {
    ImGui::SetNextWindowPos(ImVec2{popup_x, button_max.y}, ImGuiCond_Always);
  }

  ImGui::SetNextWindowSize(ImVec2{popup_width, 0.0f}, ImGuiCond_Always);

  if (ImGui::BeginPopup("##add_component_popup")) {
    // A single always-the-same popup id, so IsWindowAppearing() alone tells us this is a fresh
    // open -- reset the filter and refocus it each time.
    static auto filter_buffer = std::array<char, 128u>{};

    if (ImGui::IsWindowAppearing()) {
      filter_buffer[0] = '\0';
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##add_component_filter", ICON_MDI_MAGNIFY " Filter components...", filter_buffer.data(), filter_buffer.size());

    const auto passes = [&](std::string_view name) {
      return filter_buffer[0] == '\0' || contains_ignore_case(name, filter_buffer.data());
    };

    ImGui::Separator();

    if (!node.has_component<sbx::scenes::camera>() && passes("Camera") && ImGui::MenuItem(ICON_MDI_CAMERA_OUTLINE " Camera")) {
      state.push_command(std::make_unique<add_component_command<sbx::scenes::camera>>(node.id(), "Add Camera"));
    }

    if (!node.has_component<sbx::scenes::mesh_renderer>() && passes("Mesh Renderer") && ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Mesh Renderer")) {
      state.push_command(std::make_unique<add_component_command<sbx::scenes::mesh_renderer>>(node.id(), "Add Mesh Renderer"));
    }

    if (!node.has_component<sbx::scenes::directional_light>() && passes("Directional Light") && ImGui::MenuItem(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light")) {
      state.push_command(std::make_unique<add_component_command<sbx::scenes::directional_light>>(node.id(), "Add Directional Light"));
    }

    if (!node.has_component<sbx::scenes::point_light>() && passes("Point Light") && ImGui::MenuItem(ICON_MDI_LIGHTBULB_OUTLINE " Point Light")) {
      state.push_command(std::make_unique<add_component_command<sbx::scenes::point_light>>(node.id(), "Add Point Light"));
    }

    if (!node.has_component<sbx::scenes::spot_light>() && passes("Spot Light") && ImGui::MenuItem(ICON_MDI_FLASHLIGHT " Spot Light")) {
      state.push_command(std::make_unique<add_component_command<sbx::scenes::spot_light>>(node.id(), "Add Spot Light"));
    }

    if (!node.has_component<sbx::scenes::skybox>() && passes("Skybox") && ImGui::MenuItem(ICON_MDI_EARTH " Skybox")) {
      state.push_command(std::make_unique<add_component_command<sbx::scenes::skybox>>(node.id(), "Add Skybox"));
    }

    if (!node.has_component<sbx::scenes::particle_effect>() && passes("Particle Effect") && ImGui::MenuItem(ICON_MDI_FIREWORK " Particle Effect")) {
      state.push_command(std::make_unique<add_component_command<sbx::scenes::particle_effect>>(node.id(), "Add Particle Effect"));
    }

    if (!node.has_component<sbx::physics::rigidbody>() && passes("Rigidbody") && ImGui::MenuItem(ICON_MDI_SOCCER " Rigidbody")) {
      state.push_command(std::make_unique<add_component_command<sbx::physics::rigidbody>>(node.id(), "Add Rigidbody"));
    }

    // A node only ever gets one collider kind -- narrowphase only ever resolves one per node anyway
    // (see narrowphase.cpp's resolve_convex), so having both is never useful, just confusing.
    if (!node.has_component<sbx::physics::shape_collider>() && !node.has_component<sbx::physics::mesh_collider>() && passes("Shape Collider") && ImGui::MenuItem(ICON_MDI_SHAPE_OUTLINE " Shape Collider")) {
      state.push_command(std::make_unique<add_component_command<sbx::physics::shape_collider>>(node.id(), "Add Shape Collider"));
    }

    if (!node.has_component<sbx::physics::mesh_collider>() && !node.has_component<sbx::physics::shape_collider>() && passes("Mesh Collider") && ImGui::MenuItem(ICON_MDI_TERRAIN " Mesh Collider")) {
      state.push_command(std::make_unique<add_component_command<sbx::physics::mesh_collider>>(node.id(), "Add Mesh Collider"));
    }

    // Open-ended category, not a fixed name -- passes when "Script" matches or any script inside
    // it would, so a search for a script's own name doesn't hide the one submenu that satisfies it.
    if (passes("Script") || (filter_buffer[0] != '\0' && any_script_matches(scripting_module, filter_buffer.data()))) {
      if (ImGui::BeginMenu(ICON_MDI_FILE_CODE_OUTLINE " Script")) {
        auto& behavior_type = scripting_module.game_assembly().get_type("Sbx.Core.Behavior");
        auto any_available = false;

        for (auto* candidate : scripting_module.game_assembly().get_types()) {
          if (*candidate == behavior_type || !candidate->is_subclass_of(behavior_type)) {
            continue; // skip the base class itself and anything that isn't a Behavior
          }

          const auto full_name = std::string{candidate->get_full_name()};

          if (!passes(full_name)) {
            continue;
          }

          any_available = true;

          if (ImGui::MenuItem(full_name.c_str())) {
            state.push_command(std::make_unique<attach_script_command>(node.id(), full_name));
          }
        }

        if (!any_available) {
          ImGui::TextDisabled(filter_buffer[0] != '\0' ? "No scripts match." : "No compiled scripts found.");
        }

        ImGui::EndMenu();
      }
    }

    ImGui::EndPopup();
  }
}

// v1 supports only float/int/bool/string (the only types get/set_field_value<T> explicitly
// support); anything else renders as a disabled, unsupported label. Edits go straight to the live
// managed::object while a live instance exists (playing/paused), since play_mode_controller
// reloads the pre-play snapshot on Stop and mid-Play edits would never persist; otherwise they
// write to the persisted override.
auto draw_script_field_inspector(editor_state& state, sbx::scenes::node& node, sbx::scenes::script_entry& entry) -> void {
  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  auto& type = scripting_module.game_assembly().get_type(entry.class_name);

  if (!type) {
    ImGui::TextDisabled("(script class not found in the compiled assembly — recompile?)");
    return;
  }

  // A live instance exists exactly while playing or paused: instantiate() creates it on Play,
  // run_on_destroy() tears it down on Stop. Must key off instance existence, not
  // scenes_module.is_simulating() (false while paused), or paused edits would target the wrong side.
  sbx::scripting::managed::object* live_instance = nullptr;

  if (node.has_component<sbx::scripting::scripts>()) {
    for (auto& instance : node.get_component<sbx::scripting::scripts>().instances) {
      if (instance.get_type().get_full_name() == entry.class_name) {
        live_instance = &instance;
        break;
      }
    }
  }

  // Shared across every field below — snapshots the whole script_component so undo/redo restores
  // it via modify_component_command<script_component>. Untouched while live_instance is set, since
  // those edits target the live object directly and are never tracked (see the comment above).
  static auto pending = std::optional<sbx::scenes::script_component>{};

  const auto capture_before = [&] {
    if (!live_instance && !pending) {
      pending = node.get_component<sbx::scenes::script_component>();
    }
  };

  const auto commit_after = [&] {
    if (!live_instance && pending) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::script_component>>(node.id(), *pending, node.get_component<sbx::scenes::script_component>(), "Edit Script Field"));
      pending.reset();
    }
  };

  for (auto& field : type.get_fields()) {
    if (field.get_accessibility() != sbx::scripting::managed::type_accessibility::public_access) {
      continue;
    }

    const auto field_name = std::string{field.get_name()};

    auto hidden = false;
    auto display_name = field_name;
    auto is_read_only = false;
    auto has_clamp = false;
    auto clamp_min = 0.0f;
    auto clamp_max = 0.0f;

    for (auto& field_attribute : field.get_attributes()) {
      const auto attribute_type_name = std::string{field_attribute.get_type().get_full_name()};

      if (attribute_type_name == "Sbx.Core.Attributes.HideFromEditorAttribute") {
        hidden = true;
      } else if (attribute_type_name == "Sbx.Core.Attributes.ShowInEditorAttribute") {
        // ShowInEditorAttribute exposes DisplayName/IsReadOnly as auto-properties, not plain
        // fields — read via get_property_value, not get_field_value.
        display_name = field_attribute.get_property_value<std::string>("DisplayName");
        is_read_only = field_attribute.get_property_value<bool>("IsReadOnly");
      } else if (attribute_type_name == "Sbx.Core.Attributes.ClampValueAttribute") {
        has_clamp = true;
        clamp_min = static_cast<std::float_t>(field_attribute.get_property_value<std::double_t>("Min"));
        clamp_max = static_cast<std::float_t>(field_attribute.get_property_value<std::double_t>("Max"));
      }
    }

    if (hidden) {
      continue;
    }

    sbx::scenes::script_field_override* override_slot = nullptr;

    for (auto& existing : entry.field_overrides) {
      if (existing.name == field_name) {
        override_slot = &existing;
        break;
      }
    }

    const auto ensure_override_slot = [&]() -> sbx::scenes::script_field_override& {
      if (!override_slot) {
        entry.field_overrides.push_back(sbx::scenes::script_field_override{.name = field_name});
        override_slot = &entry.field_overrides.back();
      }

      return *override_slot;
    };

    ImGui::PushID(field_name.c_str());
    ImGui::BeginDisabled(is_read_only);

    const auto field_type_name = std::string{field.get_type().get_full_name()};

    if (field_type_name == "System.Single") {
      auto value = live_instance ? live_instance->get_field_value<std::float_t>(field_name)
                                  : override_slot ? override_slot->float_value : 0.0f;

      const auto changed = has_clamp
        ? ImGui::SliderFloat(display_name.c_str(), &value, clamp_min, clamp_max)
        : ImGui::DragFloat(display_name.c_str(), &value, 0.05f);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::float32;
          slot.float_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "System.Int32") {
      auto value = live_instance ? live_instance->get_field_value<std::int32_t>(field_name)
                                  : override_slot ? override_slot->int_value : 0;

      const auto changed = has_clamp
        ? ImGui::SliderInt(display_name.c_str(), &value, static_cast<std::int32_t>(clamp_min), static_cast<std::int32_t>(clamp_max))
        : ImGui::DragInt(display_name.c_str(), &value);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::int32;
          slot.int_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "System.Boolean") {
      auto value = live_instance ? live_instance->get_field_value<bool>(field_name)
                                  : override_slot ? override_slot->bool_value : false;

      const auto changed = ImGui::Checkbox(display_name.c_str(), &value);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::boolean;
          slot.bool_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "System.String") {
      const auto current = live_instance ? live_instance->get_field_value<std::string>(field_name)
                                          : override_slot ? override_slot->string_value : std::string{};

      auto buffer = std::array<char, 256u>{};
      std::strncpy(buffer.data(), current.c_str(), buffer.size() - 1u);
      buffer[buffer.size() - 1u] = '\0';

      const auto changed = ImGui::InputText(display_name.c_str(), buffer.data(), buffer.size());

      capture_before();

      if (changed) {
        auto value = std::string{buffer.data()};

        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::string;
          slot.string_value = value;
        }
      }

      commit_after();
    } else {
      ImGui::TextDisabled("%s: (unsupported type %s)", display_name.c_str(), field_type_name.c_str());
    }

    ImGui::EndDisabled();
    ImGui::PopID();
  }
}

// One standalone collapsible per attached script, same shape as draw_camera_section and friends,
// but called once per script_component entry rather than guarded by has_component<T>(). Title
// assumes the "<ClassName>.cs" file-name convention "New Script" generates.
auto draw_script_section(editor_state& state, sbx::scenes::node& node, sbx::scenes::script_entry& entry, std::optional<std::string>& pending_removal) -> void {
  auto is_open = true;

  const auto title = fmt::format(ICON_MDI_FILE_CODE_OUTLINE " {}.cs", entry.class_name);

  const auto is_expanded = ImGui::CollapsingHeader(title.c_str(), &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    pending_removal = entry.class_name; // defer to after the caller's loop — script_component.scripts must not shrink mid-iteration
    return;
  }

  if (is_expanded) {
    draw_script_field_inspector(state, node, entry);
  }
}

auto inspector_panel::_draw_name_field(editor_state& state, sbx::scenes::node& node) -> void {
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

  if (ImGui::IsItemActivated() && !_pending_name_before) {
    _pending_name_before = node.name();
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    // scene::find(name) can go stale after this (scene::_entities_by_name is populated at creation
    // only) — fine, selection/hierarchy key on entity/id, never name.
    node.name() = sbx::scenes::tag{std::string{_name_buffer.data()}};

    if (_pending_name_before) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::tag>>(id, *_pending_name_before, node.name(), "Rename Node"));
      _pending_name_before.reset();
    }
  }

  ImGui::Text("UUID: %llu", static_cast<unsigned long long>(id.value()));
}

auto inspector_panel::_draw_transform_section(editor_state& state, sbx::scenes::node& node) -> void {
  ImGui::SeparatorText("Transform");

  auto& transform = node.transform();

  // started captures the pre-mutation snapshot (must run before this frame's change, if any, is
  // applied below — the same frame can both start and finish a drag, via the reset button).
  // committed pushes it once the drag (or reset click) is done.
  const auto capture_before = [&](const vector3_edit_result& result) {
    if (result.started && !_pending_transform_before) {
      _pending_transform_before = transform;
    }
  };

  const auto commit_after = [&](const vector3_edit_result& result) {
    if (result.committed && _pending_transform_before) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::local_transform>>(node.id(), *_pending_transform_before, transform, "Edit Transform"));
      _pending_transform_before.reset();
    }
  };

  auto position = std::array<std::float_t, 3u>{transform.position.x(), transform.position.y(), transform.position.z()};
  const auto position_result = draw_vector3_control("Position", position, 0.0f, 0.05f);

  capture_before(position_result);

  if (position_result.changed) {
    transform.position = sbx::math::vector3{position[0], position[1], position[2]};
  }

  commit_after(position_result);

  // See _rotation_node_id/_rotation_cache/_rotation's declarations for why this is cached rather
  // than re-derived from the quaternion every frame.
  if (node.id().value() != _rotation_node_id.value() || !(transform.rotation == _rotation_cache)) {
    const auto euler = sbx::math::quaternion::euler_angles(transform.rotation);
    _rotation = {euler.x(), euler.y(), euler.z()};
    _rotation_node_id = node.id();
    _rotation_cache = transform.rotation;
  }

  const auto rotation_result = draw_vector3_control("Rotation", _rotation, 0.0f, 0.5f);

  capture_before(rotation_result);

  if (rotation_result.changed) {
    transform.rotation = sbx::math::quaternion{sbx::math::vector3{_rotation[0], _rotation[1], _rotation[2]}};
    _rotation_cache = transform.rotation;
  }

  commit_after(rotation_result);

  auto scale = std::array<std::float_t, 3u>{transform.scale.x(), transform.scale.y(), transform.scale.z()};
  const auto scale_result = draw_vector3_control("Scale", scale, 1.0f, 0.05f);

  capture_before(scale_result);

  if (scale_result.changed) {
    transform.scale = sbx::math::vector3{scale[0], scale[1], scale[2]};
  }

  commit_after(scale_result);
}

auto inspector_panel::_draw_node_properties(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  // A little vertical breathing room between each section, on top of the frame/item padding
  // pushed in draw() — keeps a node with several components/scripts from reading as one dense
  // unbroken block of controls.
  const auto section_gap = [] { ImGui::Dummy(ImVec2{0.0f, 6.0f}); };

  _draw_name_field(state, node);
  section_gap();
  _draw_transform_section(state, node);

  if (node.has_component<sbx::scenes::camera>()) {
    section_gap();
    draw_camera_section(state, node);
  }

  if (node.has_component<sbx::scenes::mesh_renderer>()) {
    section_gap();
    draw_mesh_renderer_section(state, node, assets_module);
  }

  if (node.has_component<sbx::scenes::directional_light>()) {
    section_gap();
    draw_directional_light_section(state, node);
  }

  if (node.has_component<sbx::scenes::point_light>()) {
    section_gap();
    draw_point_light_section(state, node);
  }

  if (node.has_component<sbx::scenes::spot_light>()) {
    section_gap();
    draw_spot_light_section(state, node);
  }

  if (node.has_component<sbx::scenes::skybox>()) {
    section_gap();
    draw_skybox_section(state, node, assets_module);
  }

  if (node.has_component<sbx::scenes::particle_effect>()) {
    section_gap();
    draw_particle_effect_instance_section(state, node, assets_module);
  }

  if (node.has_component<sbx::physics::rigidbody>()) {
    section_gap();
    draw_rigidbody_section(state, node);
  }

  if (node.has_component<sbx::physics::shape_collider>()) {
    section_gap();
    draw_shape_collider_section(state, node);
  }

  if (node.has_component<sbx::physics::mesh_collider>()) {
    section_gap();
    draw_mesh_collider_section(state, node, assets_module);
  }

  if (node.has_component<sbx::scenes::script_component>()) {
    auto& scripts = node.get_component<sbx::scenes::script_component>();
    auto pending_removal = std::optional<std::string>{};

    for (auto index = std::size_t{0u}; index < scripts.scripts.size(); ++index) {
      section_gap();

      ImGui::PushID(static_cast<std::int32_t>(index));
      draw_script_section(state, node, scripts.scripts[index], pending_removal);
      ImGui::PopID();
    }

    if (pending_removal) {
      for (const auto& entry : scripts.scripts) {
        if (entry.class_name == *pending_removal) {
          state.push_command(std::make_unique<detach_script_command>(node.id(), entry));
          break;
        }
      }
    }
  }

  section_gap();
  ImGui::Separator();
  ImGui::Spacing();
  draw_add_component_menu(state, node, scripting_module);
}

auto inspector_panel::_draw_material_properties(const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
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
  auto alpha_index = static_cast<std::int32_t>(_material_edit.alpha);

  if (ImGui::Combo("Alpha Mode", &alpha_index, alpha_mode_names.data(), static_cast<std::int32_t>(alpha_mode_names.size()))) {
    _material_edit.alpha = static_cast<sbx::assets::alpha_mode>(alpha_index);
    changed = true;
  }

  if (_material_edit.alpha == sbx::assets::alpha_mode::mask) {
    changed |= ImGui::DragFloat("Alpha Cutoff", &_material_edit.alpha_cutoff, 0.01f, 0.0f, 1.0f);
  }

  changed |= ImGui::Checkbox("Double Sided", &_material_edit.is_double_sided);
  changed |= ImGui::Checkbox("Casts Shadow", &_material_edit.casts_shadow);
  changed |= ImGui::Checkbox("Receives Shadow", &_material_edit.receives_shadow);

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
    // Live preview: mutates in place so every material_handle pointing at it reflects the edit
    // next frame. Disk persistence stays an explicit Save (below).
    assets_module.update_material(_asset_cache.material, _material_edit);
  }

  ImGui::Separator();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_material(_asset_cache.material, asset.path);
  }
}

auto inspector_panel::_draw_particle_effect_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (!_asset_cache.particle_effect.is_valid()) {
    ImGui::TextDisabled("Could not load this particle effect.");
    return;
  }

  auto changed = false;

  auto name_buffer = std::array<char, 128u>{};
  std::strncpy(name_buffer.data(), _particle_effect_edit.name.c_str(), name_buffer.size() - 1u);
  name_buffer[name_buffer.size() - 1u] = '\0';

  if (ImGui::InputText("Name", name_buffer.data(), name_buffer.size())) {
    _particle_effect_edit.name = std::string{name_buffer.data()};
    changed = true;
  }

  ImGui::SeparatorText("Emitters");

  static constexpr auto blend_mode_names = std::array<const char*, 2u>{"Additive", "Alpha Blend"};
  static constexpr auto shape_names = std::array<const char*, 4u>{"Point", "Sphere", "Box", "Cone"};
  static constexpr auto simulation_mode_names = std::array<const char*, 2u>{"CPU", "GPU"};

  auto& emitters = _particle_effect_edit.emitters;
  auto removed_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < emitters.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& emitter = emitters[index];

    const auto header_label = emitter.name.empty() ? fmt::format("Emitter {}", index) : emitter.name;
    const auto is_expanded = ImGui::TreeNodeEx("##emitter_node", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "%s", header_label.c_str());

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_MDI_DELETE).x - ImGui::GetStyle().FramePadding.x);

    if (ImGui::SmallButton(ICON_MDI_DELETE)) {
      removed_index = index;
      changed = true;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Remove this emitter");
    }

    if (is_expanded) {
      auto emitter_name_buffer = std::array<char, 128u>{};
      std::strncpy(emitter_name_buffer.data(), emitter.name.c_str(), emitter_name_buffer.size() - 1u);
      emitter_name_buffer[emitter_name_buffer.size() - 1u] = '\0';

      if (ImGui::InputText("Name", emitter_name_buffer.data(), emitter_name_buffer.size())) {
        emitter.name = std::string{emitter_name_buffer.data()};
        changed = true;
      }

      auto blend_mode_index = static_cast<std::int32_t>(emitter.blend_mode);

      if (ImGui::Combo("Blend Mode", &blend_mode_index, blend_mode_names.data(), static_cast<std::int32_t>(blend_mode_names.size()))) {
        emitter.blend_mode = static_cast<sbx::assets::emitter_blend_mode>(blend_mode_index);
        changed = true;
      }

      auto simulation_mode_index = static_cast<std::int32_t>(emitter.simulation_mode);

      if (ImGui::Combo("Simulation Mode", &simulation_mode_index, simulation_mode_names.data(), static_cast<std::int32_t>(simulation_mode_names.size()))) {
        emitter.simulation_mode = static_cast<sbx::assets::particle_simulation_mode>(simulation_mode_index);
        changed = true;
      }

      // GPU silently ignores unsupported emitter configs (billboard-only, no
      // collision/sub-emitters/trails/cone) rather than rejecting them; warn inline.
      if (emitter.simulation_mode == sbx::assets::particle_simulation_mode::gpu && !emitter.supports_gpu_simulation()) {
        ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, ICON_MDI_ALERT " GPU doesn't support this emitter's current config");

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("GPU is billboard-only, has no collision, sub-emitters, trails, or cone shape support. Unsupported settings are silently ignored while this mode is selected.");
        }
      }

      changed |= ImGui::DragFloat("Emission Rate", &emitter.emission_rate, 0.5f, 0.0f, 10000.0f);

      auto burst_count = static_cast<std::int32_t>(emitter.burst_count);
      if (ImGui::DragInt("Burst Count", &burst_count, 1.0f, 0, 100000)) {
        emitter.burst_count = static_cast<std::uint32_t>(std::max(burst_count, 0));
        changed = true;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Spawned once, on top of Emission Rate, the moment this emitter becomes active.");
      }

      auto shape_index = static_cast<std::int32_t>(emitter.shape);

      if (ImGui::Combo("Shape", &shape_index, shape_names.data(), static_cast<std::int32_t>(shape_names.size()))) {
        emitter.shape = static_cast<sbx::assets::emitter_shape>(shape_index);
        changed = true;
      }

      if (emitter.shape == sbx::assets::emitter_shape::sphere) {
        changed |= ImGui::DragFloat("Radius", &emitter.shape_extents.x(), 0.01f, 0.0f, 1000.0f);
      } else if (emitter.shape == sbx::assets::emitter_shape::box) {
        auto shape_extents = std::array<std::float_t, 3u>{emitter.shape_extents.x(), emitter.shape_extents.y(), emitter.shape_extents.z()};
        if (draw_vector3_control("Half Extents", shape_extents, 0.0f, 0.01f).changed) {
          emitter.shape_extents = sbx::math::vector3{shape_extents[0], shape_extents[1], shape_extents[2]};
          changed = true;
        }
      } else if (emitter.shape == sbx::assets::emitter_shape::cone) {
        auto cone_angle_degrees = emitter.cone.angle.to_degrees().value();
        if (ImGui::DragFloat("Cone Angle", &cone_angle_degrees, 0.1f, 0.1f, 89.0f)) {
          emitter.cone.angle = sbx::math::degree{cone_angle_degrees};
          changed = true;
        }

        changed |= ImGui::DragFloat("Cone Radius", &emitter.cone.radius, 0.01f, 0.001f, 1000.0f);
        changed |= ImGui::SliderFloat("Cone Emit From Volume", &emitter.cone.emit_from_volume, 0.0f, 1.0f);

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("0 = spawn on the base disc (Unity's \"Emit from: Base\"), 1 = fill the whole cone volume.");
        }
      }

      auto velocity_min = std::array<std::float_t, 3u>{emitter.velocity_min.x(), emitter.velocity_min.y(), emitter.velocity_min.z()};
      if (draw_vector3_control("Velocity Min", velocity_min, 0.0f, 0.05f).changed) {
        emitter.velocity_min = sbx::math::vector3{velocity_min[0], velocity_min[1], velocity_min[2]};
        changed = true;
      }

      auto velocity_max = std::array<std::float_t, 3u>{emitter.velocity_max.x(), emitter.velocity_max.y(), emitter.velocity_max.z()};
      if (draw_vector3_control("Velocity Max", velocity_max, 0.0f, 0.05f).changed) {
        emitter.velocity_max = sbx::math::vector3{velocity_max[0], velocity_max[1], velocity_max[2]};
        changed = true;
      }

      changed |= ImGui::DragFloat("Lifetime Min", &emitter.lifetime_min, 0.01f, 0.0f, 3600.0f);
      changed |= ImGui::DragFloat("Lifetime Max", &emitter.lifetime_max, 0.01f, emitter.lifetime_min, 3600.0f);

      changed |= draw_color_field("Start Color", emitter.start_color);
      changed |= draw_color_field("End Color", emitter.end_color);
      changed |= draw_gradient_editor("Color Over Lifetime (overrides Start/End Color if it has keys)", emitter.color_over_lifetime);

      changed |= ImGui::DragFloat("Size Min", &emitter.size_min, 0.005f, 0.0f, 1000.0f);
      changed |= ImGui::DragFloat("Size Max", &emitter.size_max, 0.005f, emitter.size_min, 1000.0f);
      changed |= draw_curve_editor("Size Over Lifetime (multiplier)", emitter.size_over_lifetime, 0.0f, 10.0f);

      changed |= ImGui::DragFloat("Rotation Min", &emitter.rotation_min, 0.01f, -6.2832f, 6.2832f);
      changed |= ImGui::DragFloat("Rotation Max", &emitter.rotation_max, 0.01f, emitter.rotation_min, 6.2832f);
      changed |= draw_curve_editor("Rotation Over Lifetime (rad/s)", emitter.rotation_over_lifetime, -20.0f, 20.0f);

      changed |= ImGui::DragFloat("Gravity", &emitter.gravity, 0.05f, -1000.0f, 1000.0f);
      changed |= ImGui::DragFloat("Drag", &emitter.drag, 0.01f, 0.0f, 100.0f);

      if (ImGui::TreeNodeEx("Velocity Over Lifetime (m/s, added on top of Velocity Min/Max)", ImGuiTreeNodeFlags_Framed)) {
        changed |= draw_curve_editor("X", emitter.velocity_over_lifetime.x, -100.0f, 100.0f);
        changed |= draw_curve_editor("Y", emitter.velocity_over_lifetime.y, -100.0f, 100.0f);
        changed |= draw_curve_editor("Z", emitter.velocity_over_lifetime.z, -100.0f, 100.0f);
        ImGui::TreePop();
      }

      auto force_min = std::array<std::float_t, 3u>{emitter.force_over_lifetime_min.x(), emitter.force_over_lifetime_min.y(), emitter.force_over_lifetime_min.z()};
      if (draw_vector3_control("Force Min", force_min, 0.0f, 0.05f).changed) {
        emitter.force_over_lifetime_min = sbx::math::vector3{force_min[0], force_min[1], force_min[2]};
        changed = true;
      }

      auto force_max = std::array<std::float_t, 3u>{emitter.force_over_lifetime_max.x(), emitter.force_over_lifetime_max.y(), emitter.force_over_lifetime_max.z()};
      if (draw_vector3_control("Force Max", force_max, 0.0f, 0.05f).changed) {
        emitter.force_over_lifetime_max = sbx::math::vector3{force_max[0], force_max[1], force_max[2]};
        changed = true;
      }

      ImGui::SeparatorText("Rendering");

      static constexpr auto render_mode_names = std::array<const char*, 2u>{"Billboard", "Mesh"};
      auto render_mode_index = static_cast<std::int32_t>(emitter.render_mode);

      if (ImGui::Combo("Render Mode", &render_mode_index, render_mode_names.data(), static_cast<std::int32_t>(render_mode_names.size()))) {
        emitter.render_mode = static_cast<sbx::assets::particle_render_mode>(render_mode_index);
        changed = true;
      }

      if (emitter.render_mode == sbx::assets::particle_render_mode::billboard) {
        ImGui::Text("Texture");
        ImGui::SameLine();

        changed |= draw_texture_picker("##particle_texture_picker", emitter.texture, assets_module, sbx::graphics::format::r8g8b8a8_srgb);
      } else {
        ImGui::Text("Mesh");
        ImGui::SameLine();

        changed |= draw_mesh_picker(state, "##particle_render_mesh_picker", emitter.render_mesh, assets_module);

        ImGui::Text("Material Override");
        ImGui::SameLine();

        changed |= draw_material_picker(state, "##particle_render_material_picker", emitter.render_material, assets_module);

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Leave unset to use each submesh's own authored material.");
        }
      }

      ImGui::SeparatorText("Collision");

      static constexpr auto collision_mode_names = std::array<const char*, 3u>{"None", "Planes", "World"};
      auto collision_mode_index = static_cast<std::int32_t>(emitter.collision.mode);

      if (ImGui::Combo("Collision Mode", &collision_mode_index, collision_mode_names.data(), static_cast<std::int32_t>(collision_mode_names.size()))) {
        emitter.collision.mode = static_cast<sbx::assets::particle_collision_mode>(collision_mode_index);
        changed = true;
      }

      if (emitter.collision.mode != sbx::assets::particle_collision_mode::none) {
        changed |= ImGui::DragFloat("Bounce", &emitter.collision.bounce, 0.01f, 0.0f, 2.0f);
        changed |= ImGui::SliderFloat("Lifetime Loss", &emitter.collision.lifetime_loss, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Dampen", &emitter.collision.dampen, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Radius Scale", &emitter.collision.radius_scale, 0.01f, 0.01f, 10.0f);

        auto max_collisions = static_cast<std::int32_t>(emitter.collision.max_collisions_per_particle);
        if (ImGui::DragInt("Max Collisions", &max_collisions, 1.0f, 0, 100)) {
          emitter.collision.max_collisions_per_particle = static_cast<std::uint32_t>(std::max(max_collisions, 0));
          changed = true;
        }

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("0 = unlimited");
        }
      }

      if (emitter.collision.mode == sbx::assets::particle_collision_mode::planes) {
        auto& planes = emitter.collision.planes;

        ImGui::Text("Planes (%zu / %zu)", planes.size(), sbx::assets::collision_max_planes);

        auto removed_plane_index = std::optional<std::size_t>{};

        for (auto plane_index = std::size_t{0u}; plane_index < planes.size(); ++plane_index) {
          ImGui::PushID(static_cast<std::int32_t>(plane_index));

          auto& plane = planes[plane_index];

          auto normal = std::array<std::float_t, 3u>{plane.normal.x(), plane.normal.y(), plane.normal.z()};
          if (draw_vector3_control("Normal", normal, 0.0f, 0.01f).changed) {
            plane.normal = sbx::math::vector3::normalized(sbx::math::vector3{normal[0], normal[1], normal[2]});
            changed = true;
          }

          changed |= ImGui::DragFloat("Distance", &plane.distance, 0.05f, -1000.0f, 1000.0f);

          if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Plane")) {
            removed_plane_index = plane_index;
            changed = true;
          }

          ImGui::PopID();
        }

        if (removed_plane_index) {
          for (auto i = *removed_plane_index; i + 1u < planes.size(); ++i) {
            planes[i] = planes[i + 1u];
          }

          planes.pop_back();
        }

        ImGui::BeginDisabled(planes.size() >= sbx::assets::collision_max_planes);

        if (ImGui::Button(ICON_MDI_PLUS " Add Plane")) {
          planes.push_back(sbx::assets::collision_plane{});
          changed = true;
        }

        ImGui::EndDisabled();
      }

      ImGui::SeparatorText("Sub-Emitters");

      static constexpr auto sub_emitter_event_names = std::array<const char*, 3u>{"Birth", "Death", "Collision"};

      auto& sub_emitters = emitter.sub_emitters;
      auto removed_sub_emitter_index = std::optional<std::size_t>{};

      for (auto sub_emitter_index = std::size_t{0u}; sub_emitter_index < sub_emitters.size(); ++sub_emitter_index) {
        ImGui::PushID(static_cast<std::int32_t>(sub_emitter_index));

        auto& binding = sub_emitters[sub_emitter_index];

        auto event_index = static_cast<std::int32_t>(binding.event);

        if (ImGui::Combo("Event", &event_index, sub_emitter_event_names.data(), static_cast<std::int32_t>(sub_emitter_event_names.size()))) {
          binding.event = static_cast<sbx::assets::sub_emitter_event>(event_index);
          changed = true;
        }

        ImGui::Text("Effect");
        ImGui::SameLine();

        changed |= draw_particle_effect_picker(state, "##sub_emitter_effect_picker", binding.effect, assets_module);

        changed |= ImGui::SliderFloat("Probability", &binding.probability, 0.0f, 1.0f);
        changed |= ImGui::Checkbox("Inherit Velocity", &binding.inherit_velocity);

        if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Sub-Emitter")) {
          removed_sub_emitter_index = sub_emitter_index;
          changed = true;
        }

        ImGui::Separator();
        ImGui::PopID();
      }

      if (removed_sub_emitter_index) {
        sub_emitters.erase(sub_emitters.begin() + static_cast<std::ptrdiff_t>(*removed_sub_emitter_index));
      }

      if (ImGui::Button(ICON_MDI_PLUS " Add Sub-Emitter")) {
        sub_emitters.push_back(sbx::assets::sub_emitter_binding{});
        changed = true;
      }

      ImGui::SeparatorText("Trail");

      changed |= ImGui::Checkbox("Enabled", &emitter.trail.enabled);

      if (emitter.trail.enabled) {
        changed |= ImGui::DragFloat("Min Vertex Distance", &emitter.trail.min_vertex_distance, 0.005f, 0.001f, 100.0f);
        changed |= ImGui::DragFloat("Trail Lifetime", &emitter.trail.lifetime, 0.01f, 0.01f, 3600.0f);
        changed |= ImGui::DragFloat("Trail Width", &emitter.trail.width, 0.005f, 0.001f, 1000.0f);
        changed |= draw_gradient_editor("Color Over Trail (0 = head, 1 = tail; overrides the particle's own color if it has keys)", emitter.trail.color_over_trail);
        changed |= ImGui::Checkbox("Die With Particle", &emitter.trail.die_with_particle);

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Off (Unity's default): the trail lingers and fades after its particle dies. On: it vanishes immediately.");
        }
      }

      ImGui::TreePop();
    }

    ImGui::PopID();
  }

  if (removed_index) {
    emitters.erase(emitters.begin() + static_cast<std::ptrdiff_t>(*removed_index));
  }

  if (ImGui::Button(ICON_MDI_PLUS " Add Emitter")) {
    emitters.push_back(sbx::assets::particle_emitter{.name = fmt::format("Emitter {}", emitters.size())});
    changed = true;
  }

  if (changed) {
    // Live preview, same as _draw_material_properties — mutates in place; disk persistence stays
    // an explicit Save (below).
    assets_module.update_particle_effect(_asset_cache.particle_effect, _particle_effect_edit);
  }

  ImGui::Separator();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_particle_effect(_asset_cache.particle_effect, asset.path);
  }
}

auto inspector_panel::_draw_asset_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (_asset_cache.id.value() != asset.id.value()) {
    _asset_cache = asset_property_cache{};
    _asset_cache.id = asset.id;

    switch (asset.kind) {
      case asset_kind::texture: _asset_cache.texture = assets_module.load_texture(asset.id); break;
      case asset_kind::mesh: _asset_cache.mesh = assets_module.load_mesh(asset.id); break;
      case asset_kind::material: _asset_cache.material = assets_module.load_material(asset.id); break;
      case asset_kind::environment_map: _asset_cache.environment_map = assets_module.load_environment_map(asset.id); break;
      case asset_kind::particle_effect: _asset_cache.particle_effect = assets_module.load_particle_effect(asset.id); break;
      case asset_kind::scene:
      case asset_kind::script:
      case asset_kind::unknown:
        break;
    }

    if (asset.kind == asset_kind::particle_effect && _asset_cache.particle_effect.is_valid()) {
      const auto& effect = *_asset_cache.particle_effect;

      _particle_effect_edit.name = effect.name();
      _particle_effect_edit.emitters = effect.emitters();
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
      _material_edit.casts_shadow = material.casts_shadow();
      _material_edit.receives_shadow = material.receives_shadow();
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
    case asset_kind::particle_effect: {
      ImGui::Text("Type: Particle Effect");
      _draw_particle_effect_properties(state, asset, assets_module);
      break;
    }
    case asset_kind::scene: {
      ImGui::Text("Type: Scene (not imported)");
      break;
    }
    case asset_kind::script: {
      ImGui::Text("Type: Script");
      ImGui::Text("Class: %s", asset.path.stem().string().c_str());
      ImGui::TextDisabled("Attach to a node via its \"Add Component > Script\" menu.");
      break;
    }
    case asset_kind::unknown: {
      ImGui::Text("Type: Unknown");
      break;
    }
  }
}

auto inspector_panel::draw(editor_state& state) -> void {
  // A bit more breathing room than ImGui's tight defaults. WindowPadding must be pushed before
  // Begin() -- it's read while laying out the window itself.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10.0f, 10.0f});

  ImGui::Begin(window_name);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{6.0f, 4.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0f, 6.0f});

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
    _draw_asset_properties(state, *asset, assets_module);
  } else {
    ImGui::TextDisabled("Nothing selected.");
  }

  ImGui::PopStyleVar(2); // FramePadding, ItemSpacing

  ImGui::End();

  ImGui::PopStyleVar(); // WindowPadding
}

} // namespace editor
