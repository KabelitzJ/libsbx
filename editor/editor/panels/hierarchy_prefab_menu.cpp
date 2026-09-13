// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/hierarchy_prefab_menu.hpp>

#include <editor/panels/hierarchy_panel.hpp>

#include <filesystem>
#include <memory>
#include <string>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/asset_tile.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <editor/commands/scene_commands.hpp>

namespace editor {

auto unique_prefab_relative_path(const std::string& tag) -> std::filesystem::path {
  auto& project = sbx::core::engine::project();
  const auto directory = project.assets_directory() / "prefabs";

  std::filesystem::create_directories(directory);

  const auto stem = tag.empty() ? std::string{"Prefab"} : tag;
  auto candidate = stem;

  for (auto suffix = 1; std::filesystem::exists(directory / (candidate + ".prefab")); ++suffix) {
    candidate = fmt::format("{} {}", stem, suffix);
  }

  return std::filesystem::path{"prefabs"} / (candidate + ".prefab");
}

auto try_instantiate_prefab_drop(editor_state& state, sbx::scenes::scene& scene, std::optional<sbx::math::uuid> parent_id) -> void {
  const auto* payload = ImGui::AcceptDragDropPayload(sbx::render::drag_drop_payload_prefab, ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

  if (!payload) {
    return;
  }

  const auto& drag = *static_cast<const sbx::render::asset_drag_payload*>(payload->Data);
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto prefab = assets_module.load_prefab(drag.id);

  if (!prefab.is_valid()) {
    return;
  }

  auto command = std::make_unique<instantiate_prefab_command>(prefab, parent_id);
  auto* created = command.get();

  state.push_command(scene, std::move(command));
  state.select_node(scene.find(created->id()));
}

auto hierarchy_panel::_draw_prefab_override_menu(sbx::scenes::scene& scene, const sbx::scenes::node& node) -> void {
  const auto overrides = sbx::scenes::scene_serializer::prefab_overrides_of(scene, node);

  if (overrides.empty()) {
    return;
  }

  if (ImGui::BeginMenu(ICON_MDI_SOURCE_MERGE " Apply to Prefab")) {
    for (const auto& override_entry : overrides) {
      if (ImGui::MenuItem(override_entry.component_key.c_str())) {
        sbx::scenes::scene_serializer::apply_prefab_override(scene, node, override_entry.component_key);
      }
    }

    if (overrides.size() > 1u && ImGui::MenuItem("Apply All")) {
      for (const auto& override_entry : overrides) {
        sbx::scenes::scene_serializer::apply_prefab_override(scene, node, override_entry.component_key);
      }
    }

    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu(ICON_MDI_BACKUP_RESTORE " Revert to Prefab")) {
    for (const auto& override_entry : overrides) {
      if (ImGui::MenuItem(override_entry.component_key.c_str())) {
        sbx::scenes::scene_serializer::revert_prefab_override(scene, node, override_entry.component_key);
      }
    }

    if (overrides.size() > 1u && ImGui::MenuItem("Revert All")) {
      for (const auto& override_entry : overrides) {
        sbx::scenes::scene_serializer::revert_prefab_override(scene, node, override_entry.component_key);
      }
    }

    ImGui::EndMenu();
  }
}

} // namespace editor
