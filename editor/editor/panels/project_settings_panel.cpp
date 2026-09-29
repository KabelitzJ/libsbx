// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/project_settings_panel.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/math/vector3.hpp>

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
