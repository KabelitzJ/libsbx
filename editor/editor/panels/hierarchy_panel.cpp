// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/hierarchy_panel.hpp>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

#include <editor/node_actions.hpp>

#include <editor/panels/hierarchy_prefab_menu.hpp>

#include <editor/widgets/inline_rename.hpp>

namespace editor {

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
  if (node.has_component<sbx::scenes::audio_source>()) return ICON_MDI_VOLUME_HIGH;
  if (node.has_component<sbx::scenes::mesh_renderer>()) return ICON_MDI_CUBE_OUTLINE;
  return ICON_MDI_AXIS_ARROW;
}

// Pushes a create command built from args and selects the new node.
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
      create_and_select_node<create_mesh_node_command>(state, scene, sbx::assets::primitive_mesh_uuid(kind), std::string{sbx::assets::primitive_mesh_name(kind)}, parent_id);
    }
  }

  ImGui::EndMenu();
}

auto draw_audio_submenu(editor_state& state, sbx::scenes::scene& scene, std::optional<sbx::math::uuid> parent_id) -> void {
  if (!ImGui::BeginMenu(ICON_MDI_VOLUME_HIGH " Audio")) {
    return;
  }

  if (ImGui::MenuItem("Audio Source")) {
    const auto id = sbx::math::uuid::create();

    auto commands = std::vector<std::unique_ptr<command>>{};
    commands.push_back(std::make_unique<create_node_command>(parent_id, "Audio Source", id));
    commands.push_back(std::make_unique<add_component_command<sbx::scenes::audio_source>>(id, "Add Audio Source"));

    state.push_command(scene, std::make_unique<composite_command>(std::move(commands), "Create Audio Source"));
    state.select_node(scene.find(id));
  }

  ImGui::EndMenu();
}

auto contains_case_insensitive(std::string_view text, std::string_view query) -> bool {
  const auto lower = [](unsigned char character) { return std::tolower(character); };

  return !std::ranges::search(text, query, {}, lower, lower).empty();
}

auto hierarchy_panel::_update_search_matches(sbx::scenes::scene& scene) -> void {
  _search_visible_ids.clear();

  const auto query = std::string_view{_search_buffer.data()};

  if (query.empty()) {
    return;
  }

  // A node stays visible if it matches or any descendant does, so every match keeps its path to the root.
  const auto visit = [&](this const auto& self, sbx::ecs::entity entity) -> bool {
    auto node = scene.node_of(entity);
    auto is_visible = contains_case_insensitive(node.name().c_str(), query);

    for (const auto child : node.get_component<sbx::scenes::relationship>().children) {
      is_visible |= self(child);
    }

    if (is_visible) {
      _search_visible_ids.insert(node.id());
    }

    return is_visible;
  };

  for (const auto entity : scene.root().get_component<sbx::scenes::relationship>().children) {
    visit(entity);
  }
}

auto hierarchy_panel::_draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity, std::optional<sbx::math::uuid> parent_id, std::size_t sibling_index) -> void {
  auto node = scene.node_of(entity);

  if (!node.is_valid()) {
    return;
  }

  const auto is_searching = _search_buffer[0] != '\0';

  if (is_searching && !_search_visible_ids.contains(node.id())) {
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

  if (is_searching && !relationship.children.empty()) {
    ImGui::SetNextItemOpen(true);
  }

  const auto is_open = is_renaming ? ImGui::TreeNodeEx("##node_row", flags, "%s", icon_for(node)) : ImGui::TreeNodeEx("##node_row", flags, "%s %s", icon_for(node), tag.c_str());

  const auto row_min = ImGui::GetItemRectMin();
  const auto row_max = ImGui::GetItemRectMax();

  const auto row_deactivated = ImGui::IsItemDeactivated();

  if (!is_renaming && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    _pending_node_action = focus_selection;
  }

  if (!is_renaming) {
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      if (ImGui::GetIO().KeyShift) {
        _pending_range_select = pending_range_select{state.node_selection_anchor(), node.id()};
      } else if (ImGui::GetIO().KeyCtrl) {
        state.toggle_node_selection(node);
      } else {
        // Selected on release, not here: IsItemClicked fires on press, and selecting then would switch the Inspector away from a drop target the user is dragging onto.
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

      // before/after become a sibling, into a child; a new instance doesn't need an exact sibling index.
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

    const auto rename_result = draw_rename_field(_rename_buffer, _rename_focus_pending, -FLT_MIN); // -FLT_MIN fills the rest of the row

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
    draw_audio_submenu(state, scene, node.id());

    if (state.selected_node_count() <= 1u && ImGui::MenuItem(ICON_MDI_PENCIL " Rename")) {
      _begin_rename(node);
    }

    if (state.selected_node_count() <= 1u && ImGui::MenuItem(ICON_MDI_CUBE_SCAN " Create Prefab...")) {
      auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

      const auto relative_path = unique_prefab_relative_path(std::string{tag.c_str()});

      auto prefab = sbx::scenes::scene_serializer::create_prefab_from_node(scene, node, std::string{tag.c_str()});
      assets_module.save_prefab(prefab, relative_path);
      sbx::scenes::scene_serializer::attach_prefab_instance(scene, node, prefab);

      // The Asset Browser only rescans its cached listing on this request, so the new .prefab shows up.
      state.request_reveal_in_browser(relative_path);
    }

    _draw_prefab_override_menu(scene, node);

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Copy", "Ctrl+C")) {
      if (!state.is_node_selected(node)) {
        state.select_node(node);
      }

      copy_selection(state, scene);
    }

    if (ImGui::MenuItem(ICON_MDI_CONTENT_PASTE " Paste", "Ctrl+V", false, !state.node_clipboard.empty())) {
      state.select_node(node);
      _pending_node_action = paste_clipboard;
    }

    if (ImGui::MenuItem(ICON_MDI_CONTENT_DUPLICATE " Duplicate", "Ctrl+D")) {
      if (!state.is_node_selected(node)) {
        state.select_node(node);
      }

      _pending_node_action = duplicate_selection;
    }

    if (ImGui::MenuItem(ICON_MDI_DELETE " Delete Node", "Delete")) {
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

auto hierarchy_panel::_draw_scene_header(editor_state& state, const sbx::scenes::loaded_scene& loaded, bool can_unload) -> bool {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  const auto is_persistent = loaded.handle == sbx::scenes::persistent_scene_handle;
  const auto is_active = loaded.handle == scenes_module.active_scene_handle();
  const auto is_editing = !scenes_module.is_simulating();

  const auto label = fmt::format("{} {}{}{}", is_persistent ? ICON_MDI_INFINITY : ICON_MDI_FILE_TREE, loaded.name, state.is_scene_dirty(loaded.handle) ? " *" : "", is_active && can_unload ? "  (active)" : "");

  ImGui::PushID(static_cast<int>(loaded.handle));

  const auto is_open = ImGui::TreeNodeEx("##scene_header", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", label.c_str());

  if (ImGui::IsItemHovered() && !loaded.path.empty()) {
    ImGui::SetTooltip("%s", loaded.path.generic_string().c_str());
  }

  // Dropping nodes on a scene moves them into it, as top-level nodes.
  if (ImGui::BeginDragDropTarget()) {
    if (const auto* payload = ImGui::AcceptDragDropPayload(node_drag_drop_payload_type)) {
      const auto dragged_id = sbx::math::uuid::from_value(*static_cast<const std::uint64_t*>(payload->Data));
      const auto& selected = state.selected_node_ids();
      const auto drags_selection = std::ranges::find(selected, dragged_id) != selected.end();

      _pending_scene_move = pending_scene_move{drags_selection ? selected : std::vector<sbx::math::uuid>{dragged_id}, loaded.handle};
    }

    ImGui::EndDragDropTarget();
  }

  if (ImGui::BeginPopupContextItem("##scene_header_context")) {
    if (ImGui::MenuItem(ICON_MDI_CHECK_CIRCLE_OUTLINE " Set Active Scene", nullptr, false, !is_active && !is_persistent)) {
      scenes_module.set_active_scene(loaded.handle);
    }

    ImGui::Separator();

    ImGui::BeginDisabled(!is_editing || is_persistent);

    if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE " Save Scene")) {
      state.request_scene_file_action(editor_state::scene_file_action::save, loaded.handle);
    }

    if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE_EDIT " Save Scene As...")) {
      state.request_scene_file_action(editor_state::scene_file_action::save_as, loaded.handle);
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_MDI_CLOSE " Unload Scene", nullptr, false, can_unload)) {
      state.request_scene_file_action(editor_state::scene_file_action::unload, loaded.handle);
    }

    ImGui::EndDisabled();

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_MDI_PLUS " Create Empty", nullptr, false, !is_persistent)) {
      _pending_create_in_scene = loaded.handle;
    }

    ImGui::EndPopup();
  }

  ImGui::PopID();

  return is_open;
}

auto hierarchy_panel::_apply_pending_scene_move(editor_state& state, sbx::scenes::scene& scene) -> void {
  if (!_pending_scene_move) {
    return;
  }

  const auto move = std::exchange(_pending_scene_move, std::nullopt);

  auto commands = std::vector<std::unique_ptr<command>>{};
  auto end_index = scene.root().get_component<sbx::scenes::relationship>().children.size();

  for (const auto id : selection_roots(scene, move->dragged_ids)) {
    auto node = scene.find(id);

    if (!node.is_valid()) {
      continue;
    }

    // Scenes own top-level nodes, so a nested node first moves to the top level, appended in drag order.
    if (scene.node_of(node.get_component<sbx::scenes::relationship>().parent) != scene.root()) {
      commands.push_back(std::make_unique<reparent_node_command>(scene, node, std::nullopt, end_index++));
    }

    commands.push_back(std::make_unique<set_scene_membership_command>(scene, id, move->handle));
  }

  if (!commands.empty()) {
    state.push_command(scene, std::make_unique<composite_command>(std::move(commands), "Move to Scene"));
  }
}

auto hierarchy_panel::_draw_empty_space_context_menu(editor_state& state, sbx::scenes::scene& scene) -> void {
  if (ImGui::MenuItem(ICON_MDI_PLUS " Add Node")) {
    create_and_select_node<create_node_command>(state, scene);
  }

  draw_3d_object_submenu(state, scene, std::nullopt);
  draw_audio_submenu(state, scene, std::nullopt);
}

auto hierarchy_panel::draw(editor_state& state) -> void {
  ImGui::Begin(window_name);

  _visible_row_order.clear();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##hierarchy_search", ICON_MDI_MAGNIFY " Search...", _search_buffer.data(), _search_buffer.size());

  _update_search_matches(scene);

  const auto& top_level = scene.root().get_component<sbx::scenes::relationship>().children;

  // Copied: a header's "Set Active Scene" runs while the list is walked.
  const auto loaded_scenes = scenes_module.loaded_scenes();
  const auto editable_scene_count = std::ranges::count_if(loaded_scenes, [](const sbx::scenes::loaded_scene& loaded) { return loaded.handle != sbx::scenes::persistent_scene_handle; });

  for (const auto& loaded : loaded_scenes) {
    if (!_draw_scene_header(state, loaded, editable_scene_count > 1)) {
      continue;
    }

    // sibling_index stays the position among all top-level nodes, which is what reordering works in.
    for (auto i = std::size_t{0u}; i < top_level.size(); ++i) {
      if (scene.scene_of(scene.node_of(top_level[i])) == loaded.handle) {
        _draw_node_row(state, scene, top_level[i], std::nullopt, i);
      }
    }
  }

  if (top_level.empty()) {
    ImGui::TextDisabled("No nodes in the active scene.");
  } else if (_search_buffer[0] != '\0' && _search_visible_ids.empty()) {
    ImGui::TextDisabled("No matching nodes.");
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

  if (ImGui::IsWindowHovered() && ImGui::IsKeyPressed(ImGuiKey_F2, false) && state.selected_node_count() == 1u) {
    if (auto selected = state.selected_node(scene); selected.is_valid()) {
      _begin_rename(selected);
    }
  }

  _apply_pending_reparent(state, scene);
  _apply_pending_scene_move(state, scene);

  if (_pending_create_in_scene) {
    const auto id = sbx::math::uuid::create();

    auto commands = std::vector<std::unique_ptr<command>>{};
    commands.push_back(std::make_unique<create_node_command>(std::nullopt, "Node", id));
    commands.push_back(std::make_unique<set_scene_membership_command>(scene, id, *_pending_create_in_scene));

    state.push_command(scene, std::make_unique<composite_command>(std::move(commands), "Create Node"));
    state.select_node(scene.find(id));

    _pending_create_in_scene.reset();
  }

  if (_pending_node_action != nullptr) {
    _pending_node_action(state, scene);
    _pending_node_action = nullptr;
  }

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



  if (_pending_add_child_parent_id != sbx::math::uuid::nil()) {
    if (auto parent = scene.find(_pending_add_child_parent_id); parent.is_valid()) {
      create_and_select_node<create_node_command>(state, scene, parent.id());
    }

    _pending_add_child_parent_id = sbx::math::uuid::nil();
  }

  ImGui::End();
}

} // namespace editor
