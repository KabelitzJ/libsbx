// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/navigation_panel.hpp>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/physics_module.hpp>

namespace editor {

auto navigation_panel::draw(editor_state& state) -> void {
  static_cast<void>(state);

  if (!is_open) {
    return;
  }

  if (!ImGui::Begin(window_name, &is_open)) {
    ImGui::End();

    return;
  }

  ImGui::SeparatorText("Agent");
  ImGui::DragFloat("Radius", &_settings.agent_radius, 0.01f, 0.01f, 5.0f);
  ImGui::DragFloat("Height", &_settings.agent_height, 0.01f, 0.01f, 5.0f);
  ImGui::DragFloat("Max Slope (deg)", &_settings.agent_max_slope, 0.5f, 0.0f, 89.0f);
  ImGui::DragFloat("Max Climb", &_settings.agent_max_climb, 0.01f, 0.0f, 5.0f);

  ImGui::SeparatorText("Voxelization");
  ImGui::DragFloat("Cell Size", &_settings.cell_size, 0.01f, 0.01f, 2.0f);
  ImGui::DragFloat("Cell Height", &_settings.cell_height, 0.01f, 0.01f, 2.0f);

  ImGui::SeparatorText("Regions & Contours");
  ImGui::DragFloat("Min Region Size", &_settings.region_min_size, 0.5f, 0.0f, 200.0f);
  ImGui::DragFloat("Merge Region Size", &_settings.region_merge_size, 0.5f, 0.0f, 200.0f);
  ImGui::DragFloat("Max Edge Length", &_settings.edge_max_length, 0.5f, 0.0f, 100.0f);
  ImGui::DragFloat("Max Edge Error", &_settings.edge_max_error, 0.1f, 0.1f, 10.0f);
  ImGui::InputInt("Verts Per Poly", &_settings.verts_per_poly);

  ImGui::Separator();

  auto& physics_module = sbx::core::engine::get_module<sbx::physics::physics_module>();

  if (ImGui::Button(ICON_MDI_MAP_MARKER_PATH " Bake Navmesh")) {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    auto& scene = scenes_module.active_scene();

    physics_module.bake_navmesh(scene, _settings);
  }

  ImGui::SameLine();

  if (physics_module.has_navmesh()) {
    ImGui::TextColored(ImVec4{0.4f, 0.9f, 0.4f, 1.0f}, "%zu polygons", physics_module.navmesh().polys.size());
  } else {
    ImGui::TextColored(ImVec4{0.9f, 0.6f, 0.2f, 1.0f}, "No navmesh baked");
  }

  auto flags = physics_module.debug_draw_flags();

  if (ImGui::Checkbox("Show Navmesh", &flags.navmesh)) {
    physics_module.set_debug_draw_flags(flags);
  }

  if (ImGui::Checkbox("Show Nav Agents", &flags.nav_agents)) {
    physics_module.set_debug_draw_flags(flags);
  }

  ImGui::End();
}

} // namespace editor
