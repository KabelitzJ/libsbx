// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_panel.hpp>

#include <utility>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/scenes/scene_serializer.hpp>

namespace editor {

auto inspector_panel::_draw_prefab_instance_header(sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  const auto& instance = node.get_component<sbx::scenes::prefab_instance>();

  ImGui::TextColored(ImVec4{0.5f, 0.8f, 1.0f, 1.0f}, "%s %s", ICON_MDI_CUBE_SCAN, instance.source.is_valid() ? instance.source->name().c_str() : "Prefab");

  ImGui::SameLine();

  // "##instance" keeps this distinct from _draw_prefab_edit's own Update Prefab button when a prefab nests another.
  if (ImGui::SmallButton(ICON_MDI_CONTENT_SAVE " Update Prefab##instance")) {
    sbx::scenes::scene_serializer::update_prefab_from_node(target, node);
  }
}

// Selecting a .prefab edits it in a scratch scene repopulated whenever the selected id changes; unsaved edits are dropped when switching away, like the other staged editors.
// _draw_node_properties is reused unmodified, since commands take their target scene explicitly.
auto inspector_panel::_draw_prefab_edit(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (!_prefab_edit_session || _prefab_edit_session->prefab->id() != asset.id) {
    auto prefab = assets_module.load_prefab(asset.id);

    if (!prefab.is_valid()) {
      _prefab_edit_session.reset();
      ImGui::TextDisabled("Invalid prefab asset.");
      return;
    }

    auto session = prefab_edit_session{};
    session.prefab = prefab;

    auto root = sbx::scenes::scene_serializer::deserialize_subtree(session.scene, prefab->snapshot());
    session.root_id = root.id();

    _prefab_edit_session = std::move(session);
  }

  auto& session = *_prefab_edit_session;

  ImGui::TextColored(ImVec4{0.5f, 0.8f, 1.0f, 1.0f}, "%s %s", ICON_MDI_CUBE_SCAN, session.prefab->name().c_str());
  ImGui::TextDisabled("Editing the prefab asset directly -- no instance in the scene.");

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Update Prefab##asset")) {
    if (auto root = session.scene.find(session.root_id); root.is_valid()) {
      auto snapshot = sbx::scenes::scene_serializer::serialize_subtree(session.scene, root);
      assets_module.update_prefab(session.prefab, snapshot);

      if (const auto path = assets_module.path_of(session.prefab->id()); !path.empty()) {
        assets_module.save_prefab(session.prefab, path);
      }
    }
  }

  ImGui::Separator();

  if (auto root = session.scene.find(session.root_id); root.is_valid()) {
    _draw_node_properties(state, session.scene, root, assets_module, false);
  }
}

} // namespace editor
