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

  // "##instance" -- distinct from _draw_prefab_edit's own "Update Prefab" button below: if this
  // instance's own prefab asset is itself a nested instance of another prefab, _draw_prefab_edit
  // draws _draw_node_properties (and thus this header) for its root too, and two same-labeled
  // buttons with no ID suffix would otherwise collide in the same ID scope.
  if (ImGui::SmallButton(ICON_MDI_CONTENT_SAVE " Update Prefab##instance")) {
    sbx::scenes::scene_serializer::update_prefab_from_node(target, node);
  }
}

/**
 * @brief Selecting a `.prefab` asset draws straight into it, same pattern _asset_cache already
 * uses for every other kind: re-populate the scratch scene whenever the selected id changes, not
 * an explicitly opened/closed mode. Switching to a different prefab (or away entirely) simply
 * stops drawing the old one; any edits since the last "Update Prefab" click are just not persisted
 * — same expectation as any other unsaved-edit-in-a-staged-buffer view in this panel
 * (_material_edit/_particle_effect_edit work the same way).
 *
 * _draw_node_properties is called completely unmodified here (identity fields off — see its doc
 * comment), against the session's own scratch scene instead of the real active scene — see
 * command.hpp's doc comment on why every command class takes its target scene explicitly rather
 * than resolving one internally, which is what makes this reuse possible with no duplicated UI
 * code and no global state.
 */
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
