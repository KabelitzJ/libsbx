// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_transform_gizmo.hpp>

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <ImGuizmo.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <editor/editor_module.hpp>
#include <editor/viewport_camera.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/composite_command.hpp>

#include <editor/editor_preferences.hpp>

namespace editor {

auto to_imguizmo_operation(gizmo_operation operation) -> ImGuizmo::OPERATION {
  switch (operation) {
    case gizmo_operation::translate: return ImGuizmo::TRANSLATE;
    case gizmo_operation::rotate: return ImGuizmo::ROTATE;
    case gizmo_operation::scale: return ImGuizmo::SCALE;
    case gizmo_operation::select: break;
  }

  return ImGuizmo::TRANSLATE;
}

auto to_imguizmo_mode(gizmo_operation operation, gizmo_mode mode) -> ImGuizmo::MODE {
  if (operation == gizmo_operation::scale) {
    return ImGuizmo::LOCAL;
  }

  return mode == gizmo_mode::local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
}

// Writes a manipulated world matrix back as the node's parent-relative local transform; the live preview during a drag.
auto apply_manipulated_world_matrix(sbx::scenes::scene& scene, sbx::scenes::node& node, const sbx::math::matrix4x4& new_world_matrix) -> void {
  const auto& relationship = node.get_component<sbx::scenes::relationship>();

  auto local_matrix = new_world_matrix;

  if (relationship.parent != sbx::ecs::null_entity) {
    if (auto parent_node = scene.node_of(relationship.parent); parent_node.is_valid() && parent_node.has_component<sbx::scenes::world_transform>()) {
      local_matrix = sbx::math::matrix4x4::inverted(parent_node.world_matrix()) * new_world_matrix;
    }
  }

  auto translation = std::array<std::float_t, 3u>{};
  auto rotation = std::array<std::float_t, 3u>{}; // degrees
  auto scale = std::array<std::float_t, 3u>{};

  ImGuizmo::DecomposeMatrixToComponents(local_matrix.data(), translation.data(), rotation.data(), scale.data());

  auto& transform = node.transform();
  transform.position = sbx::math::vector3{translation[0], translation[1], translation[2]};
  transform.rotation = sbx::math::quaternion{sbx::math::vector3{rotation[0], rotation[1], rotation[2]}};
  transform.scale = sbx::math::vector3{scale[0], scale[1], scale[2]};
}

// 2+ nodes: manipulates a virtual pivot and applies its rigid delta to every node, one undo entry per drag.
// Uses the resolved `mode`, so Scale (always LOCAL) orients to the primary node for groups too.
// Not a drag_session: the before-snapshot is read every frame for the running delta, and IsUsing() is checked both before and after Manipulate().
auto draw_group_pivot_gizmo(editor_state& state, sbx::scenes::scene& scene, const viewport_camera_matrices& matrices, const sbx::math::matrix4x4& gizmo_projection, ImGuizmo::OPERATION operation, ImGuizmo::MODE mode, const std::float_t* snap) -> bool {
  const auto& selected_ids = state.selected_node_ids();

  auto pivot_position = sbx::math::vector3{0.0f, 0.0f, 0.0f};
  auto valid_count = std::size_t{0u};

  for (const auto id : selected_ids) {
    if (auto node = scene.find(id); node.is_valid()) {
      pivot_position = pivot_position + sbx::math::vector3{node.world_matrix()[3]};
      valid_count += 1u;
    }
  }

  if (valid_count == 0u) {
    return false;
  }

  pivot_position = pivot_position / static_cast<std::float_t>(valid_count);

  auto pivot_rotation_degrees = std::array<std::float_t, 3u>{0.0f, 0.0f, 0.0f};

  if (mode == ImGuizmo::LOCAL) {
    if (auto primary = state.selected_node(scene); primary.is_valid()) {
      auto unused_translation = std::array<std::float_t, 3u>{};
      auto unused_scale = std::array<std::float_t, 3u>{};

      ImGuizmo::DecomposeMatrixToComponents(primary.world_matrix().data(), unused_translation.data(), pivot_rotation_degrees.data(), unused_scale.data());
    }
  }

  const auto pivot_translation = std::array<std::float_t, 3u>{pivot_position.x(), pivot_position.y(), pivot_position.z()};
  static constexpr auto pivot_scale = std::array<std::float_t, 3u>{1.0f, 1.0f, 1.0f};

  auto pivot_matrix = sbx::math::matrix4x4::identity;
  ImGuizmo::RecomposeMatrixFromComponents(pivot_translation.data(), pivot_rotation_degrees.data(), pivot_scale.data(), pivot_matrix.data());

  auto& drag = state.gizmo_group_drag;

  if (ImGuizmo::IsUsing() && !drag.active) {
    drag.active = true;
    drag.pivot_before = pivot_matrix;
    drag.world_before.clear();
    drag.local_before.clear();

    for (const auto id : selected_ids) {
      if (auto node = scene.find(id); node.is_valid()) {
        drag.world_before[id] = node.world_matrix();
        drag.local_before[id] = node.transform();
      }
    }
  }

  const auto changed = ImGuizmo::Manipulate(matrices.view.data(), gizmo_projection.data(), operation, mode, pivot_matrix.data(), nullptr, snap);

  if (changed) {
    const auto delta = pivot_matrix * sbx::math::matrix4x4::inverted(drag.pivot_before);

    for (const auto& [id, world_before] : drag.world_before) {
      if (auto node = scene.find(id); node.is_valid()) {
        apply_manipulated_world_matrix(scene, node, delta * world_before);
      }
    }
  }

  if (!ImGuizmo::IsUsing() && drag.active) {
    drag.active = false;

    auto sub_commands = std::vector<std::unique_ptr<command>>{};

    for (const auto& [id, local_before] : drag.local_before) {
      if (auto node = scene.find(id); node.is_valid()) {
        sub_commands.push_back(std::make_unique<modify_component_command<sbx::scenes::local_transform>>(id, local_before, node.transform(), "Edit Transform"));
      }
    }

    if (!sub_commands.empty()) {
      state.push_command(scene, std::make_unique<composite_command>(std::move(sub_commands), fmt::format("Edit Transform ({} objects)", sub_commands.size())));
    }

    drag.world_before.clear();
    drag.local_before.clear();
  }

  return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool {

  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return false;
  }

  // The select tool shows no gizmo, so clicks always pick.
  if (state.selected_node_count() == 0u || state.current_gizmo_operation == gizmo_operation::select) {
    return false;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  const auto pose = editor_module.viewport_camera(scene);

  if (!pose) {
    return false;
  }

  auto& colors = ImGuizmo::GetStyle().Colors;

  for (auto axis = std::size_t{0u}; axis < 3u; ++axis) {
    const auto color = ImGui::ColorConvertU32ToFloat4(axis_colors()[axis]);
    colors[ImGuizmo::DIRECTION_X + axis] = color;
    colors[ImGuizmo::PLANE_X + axis] = ImVec4{color.x, color.y, color.z, 0.38f};
  }

  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(viewport_origin.x, viewport_origin.y, viewport_size.x, viewport_size.y);

  const auto aspect = viewport_size.x / viewport_size.y;
  const auto matrices = compute_viewport_camera_matrices(pose->world_matrix, pose->params, aspect);

  auto gizmo_projection = matrices.projection;
  gizmo_projection[1][1] *= -1.0f;

  const auto operation = to_imguizmo_operation(state.current_gizmo_operation);
  const auto mode = to_imguizmo_mode(state.current_gizmo_operation, state.current_gizmo_mode);

  // Snapping uses the Preferences steps while on, and Ctrl inverts it for this drag; ImGuizmo reads three per-axis values, or snap[0] in degrees for rotation.
  const auto& preferences = editor_module.preferences();
  const auto translate_snap = std::array<std::float_t, 3u>{preferences.translate_snap, preferences.translate_snap, preferences.translate_snap};
  const auto rotate_snap = std::array<std::float_t, 3u>{preferences.rotate_snap, preferences.rotate_snap, preferences.rotate_snap};
  const auto scale_snap = std::array<std::float_t, 3u>{preferences.scale_snap, preferences.scale_snap, preferences.scale_snap};

  const std::float_t* snap = nullptr;

  if (state.is_snap_enabled != ImGui::GetIO().KeyCtrl) {
    switch (state.current_gizmo_operation) {
      case gizmo_operation::translate: snap = translate_snap.data(); break;
      case gizmo_operation::rotate: snap = rotate_snap.data(); break;
      case gizmo_operation::scale: snap = scale_snap.data(); break;
      case gizmo_operation::select: break;
    }
  }

  if (state.selected_node_count() >= 2u) {
    return draw_group_pivot_gizmo(state, scene, matrices, gizmo_projection, operation, mode, snap);
  }

  auto node = state.selected_node(scene);

  if (!node.is_valid()) {
    return false;
  }

  auto world_matrix = node.world_matrix();

  // Captured before Manipulate(), so it's the pre-drag value even on the frame the drag starts.
  const auto pre_manipulate = node.transform();

  const auto changed = ImGuizmo::Manipulate(matrices.view.data(), gizmo_projection.data(), operation, mode, world_matrix.data(), nullptr, snap);

  if (changed) {
    apply_manipulated_world_matrix(scene, node, world_matrix);
  }

  // One undo entry per drag.
  if (const auto ended = state.gizmo_node_drag.tick(ImGuizmo::IsUsing(), [&] { return gizmo_node_drag_before{node.id(), pre_manipulate}; })) {
    if (auto dragged_node = scene.find(ended->node_id); dragged_node.is_valid()) {
      state.push_command(scene, std::make_unique<modify_component_command<sbx::scenes::local_transform>>(ended->node_id, ended->transform, dragged_node.transform(), "Edit Transform"));
    }
  }

  return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

auto draw_gizmo_toolbar(editor_state& state, const ImVec2& viewport_origin) -> bool {
  auto& actions = state.actions;
  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();
  auto& preferences = editor_module.preferences();

  constexpr auto padding = 8.0f;
  const auto button_size = ImVec2{28.0f, 28.0f};

  ImGui::SetCursorScreenPos(ImVec2{viewport_origin.x + padding, viewport_origin.y + padding});

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.0f, 4.0f});
  ImGui::BeginGroup();

  auto is_over_toolbar = false;

  // One button per action: highlighted while its toggle is on, tooltip with the action's name and shortcut.
  const auto action_button = [&](std::string_view id) {
    const auto action = actions.find(id);

    if (!action) {
      return;
    }

    const auto is_on = action->is_checked && action->is_checked();

    if (is_on) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }

    ImGui::BeginDisabled(!actions.is_enabled(*action));

    if (ImGui::Button(action->icon, button_size)) {
      actions.run(id);
    }

    ImGui::EndDisabled();

    if (is_on) {
      ImGui::PopStyleColor();
    }

    actions.item_tooltip(id);
    is_over_toolbar |= ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);

    ImGui::SameLine();
  };

  const auto separator = [&] {
    ImGui::SameLine(0.0f, 8.0f);
  };

  action_button("gizmo.select");
  action_button("gizmo.move");
  action_button("gizmo.rotate");
  action_button("gizmo.scale");

  separator();

  action_button("gizmo.local_space");
  action_button("gizmo.snap");

  // The snap steps, next to the toggle, like Godot's Configure Snap.
  if (ImGui::Button(ICON_MDI_CHEVRON_DOWN, ImVec2{16.0f, button_size.y})) {
    ImGui::OpenPopup("##snap_steps");
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Snap steps");
  }

  is_over_toolbar |= ImGui::IsItemHovered();

  if (ImGui::BeginPopup("##snap_steps")) {
    auto changed = false;

    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Move Step", &preferences.translate_snap, 0.01f, 0.001f, 100.0f, "%.3f");
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Rotate Step", &preferences.rotate_snap, 0.5f, 0.1f, 180.0f, "%.1f deg");
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Scale Step", &preferences.scale_snap, 0.005f, 0.001f, 10.0f, "%.3f");
    changed |= ImGui::IsItemDeactivatedAfterEdit();

    if (changed) {
      editor_module.save_preferences();
    }

    ImGui::TextDisabled("Hold Ctrl while dragging to invert snapping.");

    ImGui::EndPopup();
  }

  ImGui::SameLine();
  separator();

  action_button("view.align_drops");
  action_button("view.ruler");

  separator();

  // The Viewport's display options.
  if (ImGui::Button(ICON_MDI_EYE_OUTLINE " View", ImVec2{0.0f, button_size.y})) {
    ImGui::OpenPopup("##viewport_view_menu");
  }

  is_over_toolbar |= ImGui::IsItemHovered();

  if (ImGui::BeginPopup("##viewport_view_menu")) {
    actions.menu_item("view.front");
    actions.menu_item("view.rear");
    actions.menu_item("view.right");
    actions.menu_item("view.left");
    actions.menu_item("view.top");
    actions.menu_item("view.bottom");

    ImGui::Separator();

    actions.menu_item("view.focus_origin");
    actions.menu_item("edit.focus_selection");
    actions.menu_item("view.preview_camera");

    ImGui::Separator();

    actions.menu_item("view.grid");
    actions.menu_item("view.wireframe");

    ImGui::EndPopup();
  }

  ImGui::EndGroup();
  ImGui::PopStyleVar();

  // A popup open from the toolbar also keeps clicks from picking through it.
  return is_over_toolbar || ImGui::IsPopupOpen("##snap_steps") || ImGui::IsPopupOpen("##viewport_view_menu");
}

} // namespace editor
