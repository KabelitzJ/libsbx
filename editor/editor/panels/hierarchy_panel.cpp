// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/hierarchy_panel.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/assets/primitive_meshes.hpp>
#include <libsbx/assets/assets_module.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/composite_command.hpp>
#include <editor/commands/scene_commands.hpp>

#include <editor/panels/hierarchy_prefab_menu.hpp>

#include <editor/widgets/inline_rename.hpp>

namespace editor {

// node_drag_drop_payload_type lives in editor_state.hpp now -- shared with the Inspector's
// Node-typed script field slot.

enum class drop_zone {
  before,
  into,
  after
}; // enum class drop_zone

auto icon_for(const sbx::scenes::node& node) -> const char* {
  if (node.has_component<sbx::scenes::camera>()) return ICON_MDI_CAMERA_OUTLINE;
  if (node.has_component<sbx::scenes::directional_light>()) return ICON_MDI_WHITE_BALANCE_SUNNY;
  if (node.has_component<sbx::scenes::point_light>()) return ICON_MDI_LIGHTBULB_OUTLINE;
  if (node.has_component<sbx::scenes::spot_light>()) return ICON_MDI_FLASHLIGHT;
  if (node.has_component<sbx::scenes::skybox>()) return ICON_MDI_EARTH;
  if (node.has_component<sbx::scenes::mesh_renderer>()) return ICON_MDI_CUBE_OUTLINE;
  return ICON_MDI_AXIS_ARROW;
}

// Shared by every "create a node and make it the selection" call site (Add Node, Add Child, each
// 3D Object primitive) -- Command is whichever create_*_command type, constructed from args,
// pushed, and its freshly-created node selected.
template<typename Command, typename... Args>
auto create_and_select_node(editor_state& state, sbx::scenes::scene& scene, Args&&... args) -> void {
  auto command = std::make_unique<Command>(std::forward<Args>(args)...);
  auto* created = command.get();

  state.push_command(scene, std::move(command));
  state.select_node(scene.find(created->id()));
}

auto draw_3d_object_submenu(editor_state& state, sbx::scenes::scene& scene, std::optional<sbx::math::uuid> parent_id) -> void {
  if (!ImGui::BeginMenu(ICON_MDI_AXIS_ARROW " 3D Object")) {
    return;
  }

  for (const auto kind : sbx::reflection::enum_values<sbx::assets::primitive_mesh_kind>()) {
    if (ImGui::MenuItem(std::string{sbx::reflection::to_string(kind)}.c_str())) {
      create_and_select_node<create_primitive_node_command>(state, scene, kind, parent_id);
    }
  }

  ImGui::EndMenu();
}

auto hierarchy_panel::_draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity, std::optional<sbx::math::uuid> parent_id, std::size_t sibling_index) -> void {
  auto node = scene.node_of(entity);

  if (!node.is_valid()) {
    return;
  }

  _visible_row_order.push_back(node.id());

  const auto& relationship = node.get_component<sbx::scenes::relationship>();
  const auto& tag = node.name();
  const auto is_renaming = node.id() == _renaming_id;

  auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

  if (relationship.children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  if (state.is_node_selected(node)) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  ImGui::PushID(static_cast<std::int32_t>(entity));

  const auto is_open = is_renaming ? ImGui::TreeNodeEx("##node_row", flags, "%s", icon_for(node)) : ImGui::TreeNodeEx("##node_row", flags, "%s %s", icon_for(node), tag.c_str());

  const auto row_min = ImGui::GetItemRectMin();
  const auto row_max = ImGui::GetItemRectMax();

  const auto row_deactivated = ImGui::IsItemDeactivated();

  if (!is_renaming) {
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      if (ImGui::GetIO().KeyShift) {
        _pending_range_select = pending_range_select{state.node_selection_anchor(), node.id()};
      } else if (ImGui::GetIO().KeyCtrl) {
        state.toggle_node_selection(node);
      } else {
        // Deferred until release (see row_deactivated below), never selected immediately here --
        // IsItemClicked fires on press, before BeginDragDropSource below ever gets a chance to see
        // the drag. Selecting eagerly on press would flip whatever's driven by the current
        // selection (the Inspector, most prominently) over to this node the instant a drag starts
        // from this row, yanking it out from under a drop target the user is dragging *onto*
        // elsewhere (e.g. a Node-typed script field slot in the Inspector).
        _deferred_click_id = node.id();
        _deferred_click_became_drag = false;
      }
    }

    if (ImGui::BeginDragDropSource()) {
      if (node.id() == _deferred_click_id) {
        _deferred_click_became_drag = true;
      }

      const auto raw_id = node.id().value();
      ImGui::SetDragDropPayload(node_drag_drop_payload_type, &raw_id, sizeof(raw_id));
      ImGui::Text("%s %s", icon_for(node), tag.c_str());
      ImGui::EndDragDropSource();
    }

    if (node.id() == _deferred_click_id && row_deactivated) {
      if (!_deferred_click_became_drag) {
        state.select_node(node);
      }

      _deferred_click_id = sbx::math::uuid::nil();
    }

    if (ImGui::BeginDragDropTarget()) {
      const auto row_height = row_max.y - row_min.y;
      const auto relative_y = row_height > 0.0f ? (ImGui::GetMousePos().y - row_min.y) / row_height : 0.5f;

      const auto zone = relative_y < 0.4f ? drop_zone::before : relative_y > 0.6f ? drop_zone::after : drop_zone::into;

      if (const auto* payload = ImGui::AcceptDragDropPayload(node_drag_drop_payload_type, ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
        const auto dragged_id = sbx::math::uuid::from_value(*static_cast<const std::uint64_t*>(payload->Data));

        switch (zone) {
          case drop_zone::before: _try_reparent(state, scene, dragged_id, parent_id, sibling_index); break;
          case drop_zone::after: _try_reparent(state, scene, dragged_id, parent_id, sibling_index + 1u); break;
          case drop_zone::into: _try_reparent(state, scene, dragged_id, node.id(), relationship.children.size()); break;
        }
      }

      // "before"/"after" land as a sibling of node (same parent); "into" as node's own child --
      // unlike reparenting an existing node, a fresh instantiation doesn't need exact sibling-index
      // placement.
      try_instantiate_prefab_drop(state, scene, (zone == drop_zone::into) ? std::optional<sbx::math::uuid>{node.id()} : parent_id);

      if (const auto* preview = ImGui::GetDragDropPayload(); preview != nullptr && preview->IsDataType(node_drag_drop_payload_type)) {
        auto* draw_list = ImGui::GetWindowDrawList();
        const auto color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);

        switch (zone) {
          case drop_zone::before: draw_list->AddLine(ImVec2(row_min.x, row_min.y), ImVec2(row_max.x, row_min.y), color, 2.0f); break;
          case drop_zone::after: draw_list->AddLine(ImVec2(row_min.x, row_max.y), ImVec2(row_max.x, row_max.y), color, 2.0f); break;
          case drop_zone::into: draw_list->AddRect(row_min, row_max, color, 0.0f, 0, 2.0f); break;
        }
      }

      ImGui::EndDragDropTarget();
    }
  }

  if (is_renaming) {
    ImGui::SameLine();

    const auto rename_result = draw_rename_field(_rename_buffer, _rename_focus_pending, std::numeric_limits<std::float_t>::lowest());

    if (rename_result.committed) {
      _commit_rename(state, scene, node);
    }

    if (rename_result.ended) {
      _renaming_id = sbx::math::uuid::nil();
    }
  }

  if (ImGui::BeginPopupContextItem("##node_context")) {
    if (ImGui::MenuItem(ICON_MDI_PLUS " Add Child")) {
      _pending_add_child_parent_id = node.id();
    }

    draw_3d_object_submenu(state, scene, node.id());

    if (state.selected_node_count() <= 1u && ImGui::MenuItem(ICON_MDI_PENCIL " Rename")) {
      _begin_rename(node);
    }

    if (state.selected_node_count() <= 1u && ImGui::MenuItem(ICON_MDI_CUBE_SCAN " Create Prefab...")) {
      auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

      const auto relative_path = unique_prefab_relative_path(std::string{tag.c_str()});

      auto prefab = sbx::scenes::scene_serializer::create_prefab_from_node(scene, node, std::string{tag.c_str()});
      assets_module.save_prefab(prefab, relative_path);
      sbx::scenes::scene_serializer::attach_prefab_instance(scene, node, prefab);

      // The new .prefab file exists on disk now, but the Asset Browser caches its own folder
      // listing and only rescans on this signal -- without it, nothing renders to drag/select.
      state.request_reveal_in_browser(relative_path);
    }

    _draw_prefab_override_menu(scene, node);

    if (ImGui::MenuItem(ICON_MDI_DELETE " Delete Node")) {
      _pending_delete_id = node.id();
    }

    ImGui::EndPopup();
  }

  if (is_open && !relationship.children.empty()) {
    _draw_child_rows(state, scene, node.id(), relationship.children);

    ImGui::TreePop();
  }

  ImGui::PopID();
}

auto hierarchy_panel::_draw_child_rows(editor_state& state, sbx::scenes::scene& scene, sbx::math::uuid parent_id, const std::vector<sbx::ecs::entity>& children) -> void {
  for (auto i = std::size_t{0u}; i < children.size(); ++i) {
    _draw_node_row(state, scene, children[i], parent_id, i);
  }
}

auto hierarchy_panel::_begin_rename(const sbx::scenes::node& node) -> void {
  _renaming_id = node.id();
  begin_rename(_rename_buffer, _rename_focus_pending, node.name().c_str());
}

auto hierarchy_panel::_commit_rename(editor_state& state, sbx::scenes::scene& scene, sbx::scenes::node& node) -> void {
  const auto before = node.name();
  const auto after = sbx::scenes::tag{std::string{_rename_buffer.data()}};

  if (before == after) {
    return;
  }

  node.name() = after;

  state.push_command(scene, std::make_unique<modify_component_command<sbx::scenes::tag>>(node.id(), before, after, "Rename Node"));
}

auto hierarchy_panel::_draw_empty_space_context_menu(editor_state& state, sbx::scenes::scene& scene) -> void {
  if (ImGui::MenuItem(ICON_MDI_PLUS " Add Node")) {
    create_and_select_node<create_node_command>(state, scene);
  }

  draw_3d_object_submenu(state, scene, std::nullopt);
}

auto hierarchy_panel::draw(editor_state& state) -> void {
  ImGui::Begin(window_name);

  _visible_row_order.clear();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  const auto& top_level = scene.root().get_component<sbx::scenes::relationship>().children;

  for (auto i = std::size_t{0u}; i < top_level.size(); ++i) {
    _draw_node_row(state, scene, top_level[i], std::nullopt, i);
  }

  if (top_level.empty()) {
    ImGui::TextDisabled("No nodes in the active scene.");
  }

  {
    const auto available = ImGui::GetContentRegionAvail();

    if (available.y > 0.0f) {
      ImGui::InvisibleButton("##root_drop_target", available);

      if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
        state.clear_selection();
      }

      if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload = ImGui::AcceptDragDropPayload(node_drag_drop_payload_type, ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
          const auto dragged_id = sbx::math::uuid::from_value(*static_cast<const std::uint64_t*>(payload->Data));
          _try_reparent(state, scene, dragged_id, std::nullopt, top_level.size());
        }

        try_instantiate_prefab_drop(state, scene, std::nullopt);

        ImGui::EndDragDropTarget();
      }

      if (ImGui::BeginPopupContextItem("##hierarchy_context_empty")) {
        _draw_empty_space_context_menu(state, scene);
        ImGui::EndPopup();
      }
    }
  }

  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
    state.clear_selection();
  }

  if (ImGui::BeginPopupContextWindow("##hierarchy_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
    _draw_empty_space_context_menu(state, scene);
    ImGui::EndPopup();
  }

  if (ImGui::IsWindowHovered() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
    auto roots = _filter_to_selection_roots(scene, state.selected_node_ids());

    if (roots.size() == 1u) {
      _pending_delete_id = roots.front();
    } else if (roots.size() > 1u) {
      _pending_delete_ids = std::move(roots);
    }
  }

  if (ImGui::IsWindowHovered() && ImGui::IsKeyPressed(ImGuiKey_F2, false) && state.selected_node_count() == 1u) {
    if (auto selected = state.selected_node(scene); selected.is_valid()) {
      _begin_rename(selected);
    }
  }

  _apply_pending_reparent(state, scene);

  if (_pending_range_select) {
    const auto begin_entry = std::find(_visible_row_order.begin(), _visible_row_order.end(), _pending_range_select->anchor_id);
    const auto end_entry = std::find(_visible_row_order.begin(), _visible_row_order.end(), _pending_range_select->clicked_id);

    if (begin_entry == _visible_row_order.end() || end_entry == _visible_row_order.end()) {
      if (auto clicked = scene.find(_pending_range_select->clicked_id); clicked.is_valid()) {
        state.select_node(clicked);
      }
    } else {
      const auto [low, high] = std::minmax(begin_entry, end_entry);
      state.set_node_selection(std::vector<sbx::math::uuid>(low, high + 1));
    }

    _pending_range_select.reset();
  }

  if (_pending_delete_id != sbx::math::uuid::nil()) {
    if (auto target = scene.find(_pending_delete_id); target.is_valid()) {
      state.push_command(scene, std::make_unique<delete_node_command>(scene, target));
    }

    _pending_delete_id = sbx::math::uuid::nil();
  }

  if (!_pending_delete_ids.empty()) {
    auto sub_commands = std::vector<std::unique_ptr<command>>{};

    for (const auto id : _pending_delete_ids) {
      if (auto target = scene.find(id); target.is_valid()) {
        sub_commands.push_back(std::make_unique<delete_node_command>(scene, target));
      }
    }

    if (!sub_commands.empty()) {
      state.push_command(scene, std::make_unique<composite_command>(std::move(sub_commands), fmt::format("Delete {} Nodes", sub_commands.size())));
    }

    _pending_delete_ids.clear();
  }

  if (_pending_add_child_parent_id != sbx::math::uuid::nil()) {
    if (auto parent = scene.find(_pending_add_child_parent_id); parent.is_valid()) {
      create_and_select_node<create_node_command>(state, scene, parent.id());
    }

    _pending_add_child_parent_id = sbx::math::uuid::nil();
  }

  ImGui::End();
}

} // namespace editor
