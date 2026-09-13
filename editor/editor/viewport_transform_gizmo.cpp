// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_transform_gizmo.hpp>

#include <array>
#include <memory>
#include <unordered_map>
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

#include <editor/widgets/drag_session.hpp>

namespace editor {

auto to_imguizmo_operation(gizmo_operation operation) -> ImGuizmo::OPERATION {
  switch (operation) {
    case gizmo_operation::translate: return ImGuizmo::TRANSLATE;
    case gizmo_operation::rotate: return ImGuizmo::ROTATE;
    case gizmo_operation::scale: return ImGuizmo::SCALE;
  }

  return ImGuizmo::TRANSLATE;
}

auto to_imguizmo_mode(gizmo_operation operation, gizmo_mode mode) -> ImGuizmo::MODE {
  if (operation == gizmo_operation::scale) {
    return ImGuizmo::LOCAL;
  }

  return mode == gizmo_mode::local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
}

auto handle_operation_shortcuts(editor_state& state) -> void {
  if (!ImGui::IsWindowHovered()) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
    state.current_gizmo_operation = gizmo_operation::translate;
  } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
    state.current_gizmo_operation = gizmo_operation::rotate;
  } else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
    state.current_gizmo_operation = gizmo_operation::scale;
  }
}

// Shared by both the single-node and group-pivot gizmo paths: converts a just-manipulated world
// matrix back to node's parent-relative local transform and writes it (the "live preview" applied
// every frame while dragging, before any undo command exists for the gesture).
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

// 2+ selected nodes: manipulates a virtual pivot (average position; identity rotation in World
// mode, the primary node's rotation in Local mode — including Scale, which to_imguizmo_mode always
// forces to LOCAL regardless of the toolbar's gizmo_mode; keying off the resolved `mode` rather
// than state.current_gizmo_mode directly is what keeps that Scale quirk intact for groups too) and
// applies the resulting rigid delta transform to every selected node, one undo entry per drag.
//
// Its own hand-rolled cross-frame statics (rather than drag_session, unlike the
// single-node path below) -- it reads its "before" snapshot every frame while still dragging (to
// compute the running delta from the pivot's pre-drag pose), not just once at the end, and its
// IsUsing() checks are deliberately split across two different points in the frame (once before
// this frame's Manipulate() call, once after) — a shape drag_session's single tick() can't
// reproduce without changing that timing.
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

  // Cross-frame group-drag state, separate from the single-node path's statics above — a 2+
  // selection drag snapshots every selected node's own pre-drag world/local transform, keyed by
  // uuid, rather than one shared before-value.
  static auto group_drag_active = false;
  static auto group_drag_pivot_before = sbx::math::matrix4x4::identity;
  static auto group_drag_world_before = std::unordered_map<sbx::math::uuid, sbx::math::matrix4x4>{};
  static auto group_drag_local_before = std::unordered_map<sbx::math::uuid, sbx::scenes::local_transform>{};

  if (ImGuizmo::IsUsing() && !group_drag_active) {
    group_drag_active = true;
    group_drag_pivot_before = pivot_matrix;
    group_drag_world_before.clear();
    group_drag_local_before.clear();

    for (const auto id : selected_ids) {
      if (auto node = scene.find(id); node.is_valid()) {
        group_drag_world_before[id] = node.world_matrix();
        group_drag_local_before[id] = node.transform();
      }
    }
  }

  const auto changed = ImGuizmo::Manipulate(matrices.view.data(), gizmo_projection.data(), operation, mode, pivot_matrix.data(), nullptr, snap);

  if (changed) {
    const auto delta = pivot_matrix * sbx::math::matrix4x4::inverted(group_drag_pivot_before);

    for (const auto& [id, world_before] : group_drag_world_before) {
      if (auto node = scene.find(id); node.is_valid()) {
        apply_manipulated_world_matrix(scene, node, delta * world_before);
      }
    }
  }

  if (!ImGuizmo::IsUsing() && group_drag_active) {
    group_drag_active = false;

    auto sub_commands = std::vector<std::unique_ptr<command>>{};

    for (const auto& [id, local_before] : group_drag_local_before) {
      if (auto node = scene.find(id); node.is_valid()) {
        sub_commands.push_back(std::make_unique<modify_component_command<sbx::scenes::local_transform>>(id, local_before, node.transform(), "Edit Transform"));
      }
    }

    if (!sub_commands.empty()) {
      state.push_command(scene, std::make_unique<composite_command>(std::move(sub_commands), fmt::format("Edit Transform ({} objects)", sub_commands.size())));
    }

    group_drag_world_before.clear();
    group_drag_local_before.clear();
  }

  return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool {
  handle_operation_shortcuts(state);

  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return false;
  }

  if (state.selected_node_count() == 0u) {
    return false;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  const auto pose = editor_module.viewport_camera(scene);

  if (!pose) {
    return false;
  }

  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(viewport_origin.x, viewport_origin.y, viewport_size.x, viewport_size.y);

  const auto aspect = viewport_size.x / viewport_size.y;
  const auto matrices = compute_viewport_camera_matrices(pose->world_matrix, pose->params, aspect);

  auto gizmo_projection = matrices.projection;
  gizmo_projection[1][1] *= -1.0f;

  const auto operation = to_imguizmo_operation(state.current_gizmo_operation);
  const auto mode = to_imguizmo_mode(state.current_gizmo_operation, state.current_gizmo_mode);

  // Hold Ctrl to snap (Blender/Unity convention) instead of moving freely. ImGuizmo reads snap
  // as 3 per-axis values for translate/scale, or just snap[0] (degrees) for rotate.
  static constexpr auto translate_snap = std::array<std::float_t, 3u>{1.0f, 1.0f, 1.0f};
  static constexpr auto rotate_snap = std::array<std::float_t, 3u>{15.0f, 15.0f, 15.0f};
  static constexpr auto scale_snap = std::array<std::float_t, 3u>{0.1f, 0.1f, 0.1f};

  const std::float_t* snap = nullptr;

  if (ImGui::GetIO().KeyCtrl) {
    switch (state.current_gizmo_operation) {
      case gizmo_operation::translate: snap = translate_snap.data(); break;
      case gizmo_operation::rotate: snap = rotate_snap.data(); break;
      case gizmo_operation::scale: snap = scale_snap.data(); break;
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

  // Captured before Manipulate() runs this frame, so it's the true pre-drag value even on the
  // exact frame IsUsing() first flips true.
  const auto pre_manipulate = node.transform();

  const auto changed = ImGuizmo::Manipulate(matrices.view.data(), gizmo_projection.data(), operation, mode, world_matrix.data(), nullptr, snap);

  if (changed) {
    apply_manipulated_world_matrix(scene, node, world_matrix);
  }

  // Cross-frame gizmo-drag state — one shared instance is enough since only one node can have an
  // active single-node gizmo drag at a time (a 2+ selection instead uses draw_group_pivot_gizmo's
  // own statics). Brackets the per-frame write above into one undo entry per drag.
  struct node_drag_before {
    sbx::math::uuid node_id;
    sbx::scenes::local_transform transform;
  };

  static auto drag = drag_session<node_drag_before>{};

  if (const auto ended = drag.tick(ImGuizmo::IsUsing(), [&] { return node_drag_before{node.id(), pre_manipulate}; })) {
    if (auto dragged_node = scene.find(ended->node_id); dragged_node.is_valid()) {
      state.push_command(scene, std::make_unique<modify_component_command<sbx::scenes::local_transform>>(ended->node_id, ended->transform, dragged_node.transform(), "Edit Transform"));
    }
  }

  return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

auto draw_gizmo_toolbar(editor_state& state, const ImVec2& viewport_origin) -> bool {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  if (state.selected_node_count() == 0u) {
    return false;
  }

  constexpr auto padding = 8.0f;
  const auto button_size = ImVec2{28.0f, 28.0f};

  ImGui::SetCursorScreenPos(ImVec2{viewport_origin.x + padding, viewport_origin.y + padding});

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4.0f, 4.0f});
  ImGui::BeginGroup();

  const auto tool_button = [&](const char* icon, gizmo_operation operation, const char* tooltip) {
    const auto is_active = state.current_gizmo_operation == operation;

    if (is_active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }

    if (ImGui::Button(icon, button_size)) {
      state.current_gizmo_operation = operation;
    }

    if (is_active) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltip);
    }
  };

  tool_button(ICON_MDI_ARROW_ALL, gizmo_operation::translate, "Translate (1)");
  tool_button(ICON_MDI_ROTATE_3D_VARIANT, gizmo_operation::rotate, "Rotate (2)");
  tool_button(ICON_MDI_ARROW_EXPAND_ALL, gizmo_operation::scale, "Scale (3)");

  ImGui::EndGroup();
  ImGui::PopStyleVar();

  return ImGui::IsItemHovered();
}

} // namespace editor
