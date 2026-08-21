// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_gizmo.hpp>

#include <array>

#include <ImGuizmo.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <editor/viewport_camera.hpp>

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

auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool {
  handle_operation_shortcuts(state);

  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return false;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = state.selected_node(scene);

  if (!node.is_valid() || !scene.has_active_camera()) {
    return false;
  }

  auto camera_node = scene.active_camera();

  if (!camera_node.is_valid() || !camera_node.has_component<sbx::scenes::camera>()) {
    return false;
  }

  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(viewport_origin.x, viewport_origin.y, viewport_size.x, viewport_size.y);

  const auto& camera = camera_node.get_component<sbx::scenes::camera>();
  const auto aspect = viewport_size.x / viewport_size.y;
  const auto matrices = compute_viewport_camera_matrices(camera_node, camera, aspect);

  auto gizmo_projection = matrices.projection;
  gizmo_projection[1][1] *= -1.0f;

  const auto operation = to_imguizmo_operation(state.current_gizmo_operation);
  const auto mode = to_imguizmo_mode(state.current_gizmo_operation, state.current_gizmo_mode);

  auto world_matrix = node.world_matrix();

  const auto changed = ImGuizmo::Manipulate(matrices.view.data(), gizmo_projection.data(), operation, mode, world_matrix.data());

  if (changed) {
    const auto& relationship = node.get_component<sbx::scenes::relationship>();

    auto local_matrix = world_matrix;

    if (relationship.parent != sbx::ecs::null_entity) {
      if (auto parent_node = scene.node_of(relationship.parent); parent_node.is_valid()) {
        local_matrix = sbx::math::matrix4x4::inverted(parent_node.world_matrix()) * world_matrix;
      }
    }

    auto translation = std::array<std::float_t, 3u>{};
    auto rotation = std::array<std::float_t, 3u>{}; // degrees
    auto scale = std::array<std::float_t, 3u>{};

    ImGuizmo::DecomposeMatrixToComponents(local_matrix.data(), translation.data(), rotation.data(), scale.data());

    auto& transform = node.transform();
    transform.position = sbx::math::vector3f{translation[0], translation[1], translation[2]};
    transform.rotation = sbx::math::quaternion{sbx::math::vector3f{rotation[0], rotation[1], rotation[2]}};
    transform.scale = sbx::math::vector3f{scale[0], scale[1], scale[2]};
  }

  return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

} // namespace editor
