// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/project_settings_panel.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/widgets/vector_fields.hpp>

namespace editor {

auto project_settings_panel::draw(editor_state& state) -> void {
  auto open_layers_tab = false;

  if (state.open_layer_settings_request) {
    state.open_layer_settings_request = false;
    is_open = true;
    open_layers_tab = true;
    ImGui::SetNextWindowFocus();
  }

  if (!is_open) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2{760.0f, 520.0f}, ImGuiCond_FirstUseEver);

  if (ImGui::Begin(window_name, &is_open) && ImGui::BeginTabBar("##project_settings_tabs")) {
    if (ImGui::BeginTabItem("General")) {
      _draw_general_tab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Physics")) {
      _draw_physics_tab();
      ImGui::EndTabItem();
    }

    const auto layers_visible = ImGui::BeginTabItem("Layers", nullptr, open_layers_tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);

    if (layers_visible) {
      _draw_layers_tab();
      ImGui::EndTabItem();
    }

    _layers_tab_was_visible = layers_visible;

    if (ImGui::BeginTabItem("Audio")) {
      _draw_audio_tab();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  } else {
    _layers_tab_was_visible = false;
  }

  ImGui::End();
}

auto project_settings_panel::_draw_general_tab() -> void {
  auto& project = sbx::core::engine::project();

  auto name = std::array<char, 128u>{};
  std::strncpy(name.data(), project.name().c_str(), name.size() - 1u);

  if (ImGui::InputText("Project Name", name.data(), name.size())) {
    project.set_name(std::string{name.data()});
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    project.save();
  }

  const auto& startup_scene = project.startup_scene();
  const auto preview = startup_scene ? startup_scene->generic_string() : std::string{"(None)"};

  if (ImGui::BeginCombo("Startup Scene", preview.c_str())) {
    if (ImGui::Selectable("(None)", !startup_scene)) {
      project.set_startup_scene(std::nullopt);
      project.save();
    }

    const auto assets_directory = project.assets_directory();

    for (const auto& entry : std::filesystem::recursive_directory_iterator{assets_directory}) {
      if (!entry.is_regular_file() || entry.path().extension() != ".scene") {
        continue;
      }

      const auto relative = std::filesystem::relative(entry.path(), assets_directory);

      if (ImGui::Selectable(relative.generic_string().c_str(), startup_scene && *startup_scene == relative)) {
        project.set_startup_scene(relative);
        project.save();
      }
    }

    ImGui::EndCombo();
  }

  ImGui::TextDisabled("Loaded when the runtime (and the editor) starts.");
}

auto project_settings_panel::_draw_physics_tab() -> void {
  auto& project = sbx::core::engine::project();

  const auto& gravity = project.gravity();
  auto gravity_values = std::array<std::float_t, 3u>{gravity.x(), gravity.y(), gravity.z()};

  const auto gravity_result = draw_vector3_control("Gravity", gravity_values, {0.0f, -9.81f, 0.0f}, 0.05f);

  if (gravity_result.changed) {
    project.set_gravity(sbx::math::vector3{gravity_values[0], gravity_values[1], gravity_values[2]});
  }

  if (gravity_result.committed) {
    project.save();
  }

  auto iterations = static_cast<std::int32_t>(project.velocity_iterations());

  if (ImGui::DragInt("Solver Iterations", &iterations, 0.1f, 1, 64)) {
    project.set_velocity_iterations(static_cast<std::uint32_t>(iterations));
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    project.save();
  }

  auto fixed_timestep = project.fixed_timestep();

  if (ImGui::DragFloat("Fixed Timestep", &fixed_timestep, 0.0005f, 0.001f, 0.1f, "%.4f s")) {
    project.set_fixed_timestep(fixed_timestep);
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    project.save();
  }

  ImGui::TextDisabled("%.1f physics/FixedUpdate steps per second.", 1.0f / project.fixed_timestep());
}

// Whether @p ancestor is @p candidate or one of its parents, so a drop can't create a cycle.
auto is_mixer_group_within(const std::vector<sbx::core::mixer_group>& groups, sbx::math::uuid candidate, const sbx::math::uuid& ancestor) -> bool {
  for (auto steps = std::size_t{0u}; steps <= groups.size() && candidate != sbx::math::uuid::nil(); ++steps) {
    if (candidate == ancestor) {
      return true;
    }

    const auto entry = std::ranges::find(groups, candidate, &sbx::core::mixer_group::id);
    candidate = entry != groups.end() ? entry->parent : sbx::math::uuid::nil();
  }

  return false;
}

auto project_settings_panel::_draw_audio_tab() -> void {
  static constexpr auto drag_drop_payload_mixer_group = "SBX_MIXER_GROUP";

  auto& project = sbx::core::engine::project();

  auto max_voices = static_cast<std::int32_t>(project.max_voices());

  if (ImGui::DragInt("Max Voices", &max_voices, 0.25f, 1, 255)) {
    project.set_max_voices(static_cast<std::uint32_t>(std::clamp(max_voices, 1, 255)));
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    project.save();
  }

  ImGui::TextDisabled("How many sounds can be audible at once. Takes effect on the next start.");

  ImGui::SeparatorText("Mixer Groups");
  ImGui::TextDisabled("A sound's volume is multiplied by every group above it. Drag a group onto another to move it.");

  // Edited as a copy; structural changes are deferred until the table is drawn.
  auto groups = project.mixer_groups();

  auto changed = false;
  auto commit = false;
  auto add_child_of = std::optional<sbx::math::uuid>{};
  auto remove_group = std::optional<sbx::math::uuid>{};
  auto reparent = std::optional<std::pair<sbx::math::uuid, sbx::math::uuid>>{}; // (group, new parent)

  const auto draw_row = [&](this auto&& self, std::size_t index) -> void {
    auto& group = groups[index];
    const auto id = group.id;
    const auto is_master = index == 0u;
    const auto has_children = std::ranges::any_of(groups, [&](const sbx::core::mixer_group& other) { return other.parent == id; });

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID(static_cast<int>(id.value() ^ (id.value() >> 32u)));

    const auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | (has_children ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf);
    const auto is_open = ImGui::TreeNodeEx("##group", flags);

    if (!is_master && ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload(drag_drop_payload_mixer_group, &id, sizeof(id));
      ImGui::TextUnformatted(group.name.c_str());
      ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
      if (const auto* payload = ImGui::AcceptDragDropPayload(drag_drop_payload_mixer_group)) {
        const auto dragged = *static_cast<const sbx::math::uuid*>(payload->Data);

        if (!is_mixer_group_within(groups, id, dragged)) {
          reparent = std::pair{dragged, id};
        }
      }

      ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();

    auto name = std::array<char, 64u>{};
    std::strncpy(name.data(), group.name.c_str(), name.size() - 1u);

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::InputText("##name", name.data(), name.size())) {
      group.name = name.data();
      changed = true;
    }

    commit |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);

    changed |= ImGui::SliderFloat("##volume", &group.volume, 0.0f, 1.0f);
    commit |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::TableNextColumn();

    if (ImGui::Checkbox("##mute", &group.muted)) {
      changed = true;
      commit = true;
    }

    ImGui::TableNextColumn();

    if (ImGui::SmallButton(ICON_MDI_PLUS)) {
      add_child_of = id;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Add a child group");
    }

    if (!is_master) {
      ImGui::SameLine();

      if (ImGui::SmallButton(ICON_MDI_DELETE)) {
        remove_group = id;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Delete; its children move up a level");
      }
    }

    if (is_open) {
      for (auto child = std::size_t{0u}; child < groups.size(); ++child) {
        if (groups[child].parent == id) {
          self(child);
        }
      }

      ImGui::TreePop();
    }

    ImGui::PopID();
  };

  if (ImGui::BeginTable("##mixer_groups", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Mute", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();

    draw_row(0u);

    ImGui::EndTable();
  }

  if (add_child_of) {
    groups.push_back(sbx::core::mixer_group{sbx::math::uuid::create(), "New Group", *add_child_of});
    changed = commit = true;
  }

  if (remove_group) {
    const auto removed = std::ranges::find(groups, *remove_group, &sbx::core::mixer_group::id);
    const auto new_parent = removed->parent;

    for (auto& group : groups) {
      if (group.parent == *remove_group) {
        group.parent = new_parent;
      }
    }

    groups.erase(removed);
    changed = commit = true;
  }

  if (reparent) {
    std::ranges::find(groups, reparent->first, &sbx::core::mixer_group::id)->parent = reparent->second;
    changed = commit = true;
  }

  if (changed) {
    project.set_mixer_groups(std::move(groups));
  }

  if (commit) {
    project.save();
  }
}

auto project_settings_panel::_draw_layers_tab() -> void {
  auto& project = sbx::core::engine::project();

  if (!_layers_tab_was_visible) {
    _layer_rows.clear();

    for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
      if (!project.layers()[index].empty()) {
        _layer_rows.push_back(static_cast<std::uint8_t>(index));
      }
    }
  }

  const auto content_region = ImGui::GetContentRegionAvail();
  const auto list_width = content_region.x * 0.25f;

  if (ImGui::BeginChild("##layer_list", ImVec2{list_width, 0.0f}, true)) {
    ImGui::TextUnformatted("Layers");
    ImGui::Separator();

    auto pending_remove = std::optional<std::uint8_t>{};

    for (const auto index : _layer_rows) {
      ImGui::PushID(static_cast<int>(index));

      auto buffer = std::array<char, 64u>{};
      std::strncpy(buffer.data(), project.layers()[index].c_str(), buffer.size() - 1u);

      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);

      // Deliberately not gated on non-empty: an in-progress rename (select-all, retype) passes through an empty string, and
      // that must not delete the row out from under the user -- only the "x" button does that.
      if (ImGui::InputText("##name", buffer.data(), buffer.size())) {
        project.set_layer_name(index, std::string{buffer.data()});
        project.save();
      }

      ImGui::SameLine();

      if (ImGui::Button(ICON_MDI_CLOSE)) {
        pending_remove = index;
      }

      ImGui::PopID();
    }

    if (pending_remove) {
      project.set_layer_name(*pending_remove, std::string{});
      project.save();
      std::erase(_layer_rows, *pending_remove);
    }

    ImGui::Spacing();

    ImGui::BeginDisabled(_layer_rows.size() >= sbx::core::layer_count);

    if (ImGui::Button(ICON_MDI_PLUS " Add Layer", ImVec2{-1.0f, 0.0f})) {
      for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
        if (project.layers()[index].empty()) {
          project.set_layer_name(static_cast<std::uint8_t>(index), "New Layer");
          project.save();
          _layer_rows.push_back(static_cast<std::uint8_t>(index)); // always the bottom row, whichever numeric slot it reused
          break;
        }
      }
    }

    ImGui::EndDisabled();
  }

  ImGui::EndChild();

  ImGui::SameLine();

  if (ImGui::BeginChild("##layer_matrix_pane", ImVec2{0.0f, 0.0f}, true)) {
    ImGui::TextUnformatted("Layer Collision Matrix");
    ImGui::TextDisabled("Unchecking a cell stops those two layers from physically colliding.");
    ImGui::Separator();

    auto named_indices = std::vector<std::uint8_t>{};

    for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
      if (!project.layers()[index].empty()) {
        named_indices.push_back(static_cast<std::uint8_t>(index));
      }
    }

    if (named_indices.empty()) {
      ImGui::TextDisabled("No named layers yet -- add one on the left.");
    } else {
      const auto column_count = static_cast<int>(named_indices.size()) + 1;
      const auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

      if (ImGui::BeginTable("##layer_matrix", column_count, table_flags, ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 110.0f);

        for (const auto column_layer : named_indices) {
          ImGui::TableSetupColumn(project.layer_name(column_layer).c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
        }

        ImGui::TableHeadersRow();

        for (const auto row_layer : named_indices) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(project.layer_name(row_layer).c_str());

          auto column_slot = 1;

          for (const auto column_layer : named_indices) {
            ImGui::TableSetColumnIndex(column_slot);
            ++column_slot;

            ImGui::PushID(static_cast<int>(row_layer) * static_cast<int>(sbx::core::layer_count) + static_cast<int>(column_layer));

            auto collide = project.layers_collide(row_layer, column_layer);

            if (ImGui::Checkbox("##cell", &collide)) {
              project.set_layers_collide(row_layer, column_layer, collide);
              project.save();
            }

            ImGui::PopID();
          }
        }

        ImGui::EndTable();
      }
    }
  }

  ImGui::EndChild();
}

} // namespace editor
