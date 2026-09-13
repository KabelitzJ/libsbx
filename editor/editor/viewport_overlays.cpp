// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_overlays.hpp>

#include <array>
#include <cfloat>
#include <cmath>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/ecs/view.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

#include <editor/editor_module.hpp>
#include <editor/viewport_camera.hpp>
#include <editor/viewport_picking.hpp>

namespace editor {

auto draw_node_icons(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size, bool gizmo_capturing_input) -> bool {
  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return false;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  const auto pose = editor_module.viewport_camera(scene);

  if (!pose) {
    return false;
  }

  const auto aspect = viewport_size.x / viewport_size.y;
  const auto matrices = compute_viewport_camera_matrices(pose->world_matrix, pose->params, aspect);
  const auto view_projection = matrices.projection * matrices.view;

  constexpr auto icon_size = 24.0f;

  auto* font = ImGui::GetIO().FontDefault;
  auto* draw_list = ImGui::GetWindowDrawList();

  auto any_active = false;

  const auto viewport_size_u = sbx::math::vector2u{static_cast<std::uint32_t>(viewport_size.x), static_cast<std::uint32_t>(viewport_size.y)};

  const auto draw_icon = [&](sbx::ecs::entity entity, const sbx::math::vector3& world_position, const char* icon) {
    const auto projected = project_to_viewport_position(view_projection, world_position, viewport_size_u);

    if (!projected.visible) {
      return;
    }

    const auto screen_position = ImVec2{viewport_origin.x + projected.position.x(), viewport_origin.y + projected.position.y()};
    const auto text_size = font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
    const auto icon_min = ImVec2{screen_position.x - text_size.x * 0.5f, screen_position.y - text_size.y * 0.5f};

    ImGui::SetCursorScreenPos(icon_min);
    ImGui::PushID(static_cast<std::int32_t>(entity));

    const auto node = scene.node_of(entity);
    auto hovered = false;

    // Skip hit-testing (glyph still draws) when the gizmo already has the cursor — see
    // gizmo_capturing_input's doc comment in viewport_overlays.hpp.
    if (!gizmo_capturing_input) {
      ImGui::InvisibleButton("##node_icon", text_size);

      hovered = ImGui::IsItemHovered();

      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (ImGui::GetIO().KeyCtrl) {
          state.toggle_node_selection(node);
        } else if (ImGui::GetIO().KeyShift) {
          state.add_node_to_selection(node);
        } else {
          state.select_node(node);
        }
      }

      any_active |= hovered || ImGui::IsItemActive();
    }

    ImGui::PopID();

    const auto color = state.is_node_selected(node) ? IM_COL32(255, 200, 60, 255) : hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 220, 220, 200);

    draw_list->AddText(font, icon_size, icon_min, color, icon);
  };

  for (auto&& [entity, transform, light] : scene.query<sbx::scenes::world_transform, sbx::scenes::point_light>(sbx::ecs::exclude<sbx::scenes::mesh_renderer, sbx::scenes::inactive>).each()) {
    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_LIGHTBULB);
  }

  for (auto&& [entity, transform, light] : scene.query<sbx::scenes::world_transform, sbx::scenes::spot_light>(sbx::ecs::exclude<sbx::scenes::mesh_renderer, sbx::scenes::inactive>).each()) {
    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_FLASHLIGHT);
  }

  for (auto&& [entity, transform, light] : scene.query<sbx::scenes::world_transform, sbx::scenes::directional_light>(sbx::ecs::exclude<sbx::scenes::mesh_renderer, sbx::scenes::inactive>).each()) {
    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_WHITE_BALANCE_SUNNY);
  }

  // Excluded from its own icon only while actually being viewed through (Play/Paused) — there it
  // sits at the eye position, a degenerate projection (clip.w near zero) that flickered on and
  // off every other frame. In edit mode the viewport looks through the editor camera instead.
  const auto is_viewing_through_active_camera = editor_module.play_state() != editor::play_state::edit;
  const auto active_camera_id = (is_viewing_through_active_camera && scene.has_active_camera()) ? static_cast<sbx::math::uuid>(scene.active_camera().id()) : sbx::math::uuid::nil();

  for (auto&& [entity, transform, node_camera] : scene.query<sbx::scenes::world_transform, sbx::scenes::camera>(sbx::ecs::exclude<sbx::scenes::mesh_renderer, sbx::scenes::inactive>).each()) {
    if (scene.node_of(entity).id() == active_camera_id) {
      continue;
    }

    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_CAMERA);
  }

  return any_active;
}

auto draw_camera_frustum_gizmo(editor_state& state, const ImVec2& viewport_size) -> void {
  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = state.selected_node(scene);

  if (!node.is_valid() || !node.has_component<sbx::scenes::camera>()) {
    return;
  }

  const auto& camera = node.get_component<sbx::scenes::camera>();
  const auto world = node.world_matrix();
  const auto aspect = viewport_size.x / viewport_size.y;

  const auto half_fov = sbx::math::to_radians(sbx::math::degree{camera.fov_degrees}).value() * 0.5f;
  const auto half_height_near = camera.near_plane * std::tan(half_fov);
  const auto half_width_near = half_height_near * aspect;
  const auto half_height_far = camera.far_plane * std::tan(half_fov);
  const auto half_width_far = half_height_far * aspect;

  // Camera-local space, forward along -Z (matches the engine's convention elsewhere, e.g. lighting/view matrices).
  const auto near_corners = std::array<sbx::math::vector3, 4u>{
    sbx::math::vector3{-half_width_near,  half_height_near, -camera.near_plane},
    sbx::math::vector3{ half_width_near,  half_height_near, -camera.near_plane},
    sbx::math::vector3{ half_width_near, -half_height_near, -camera.near_plane},
    sbx::math::vector3{-half_width_near, -half_height_near, -camera.near_plane}
  };

  const auto far_corners = std::array<sbx::math::vector3, 4u>{
    sbx::math::vector3{-half_width_far,  half_height_far, -camera.far_plane},
    sbx::math::vector3{ half_width_far,  half_height_far, -camera.far_plane},
    sbx::math::vector3{ half_width_far, -half_height_far, -camera.far_plane},
    sbx::math::vector3{-half_width_far, -half_height_far, -camera.far_plane}
  };

  const auto to_world = [&world](const sbx::math::vector3& local) -> sbx::math::vector3 {
    const auto point = world * sbx::math::vector4{local, 1.0f};
    return sbx::math::vector3{point.x(), point.y(), point.z()};
  };

  auto& debug_draw = sbx::core::engine::get_module<sbx::render::scene_renderer_module>().debug_draw();
  const auto color = sbx::math::color::yellow();

  for (auto i = std::size_t{0u}; i < 4u; ++i) {
    const auto next = (i + 1u) % 4u;

    debug_draw.add_line(to_world(near_corners[i]), to_world(near_corners[next]), color);
    debug_draw.add_line(to_world(far_corners[i]), to_world(far_corners[next]), color);
    debug_draw.add_line(to_world(near_corners[i]), to_world(far_corners[i]), color);
  }
}

} // namespace editor
