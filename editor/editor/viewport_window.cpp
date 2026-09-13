// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_window.hpp>

#include <cstdint>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/ui/ui_module.hpp>

#include <editor/editor_ui_layer.hpp>
#include <editor/editor_module.hpp>

#include <editor/viewport_transform_gizmo.hpp>
#include <editor/viewport_view_gizmo.hpp>
#include <editor/viewport_overlays.hpp>
#include <editor/viewport_picking.hpp>

namespace editor {

auto draw_viewport_window(editor_state& state, const sbx::graphics::sampler& sampler) -> bool {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin(editor_ui_layer::viewport_window_name, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // ImGui::PopStyleVar();

  const auto is_hovered = ImGui::IsWindowHovered();

  auto available = ImGui::GetContentRegionAvail();

  auto width = static_cast<std::uint32_t>(available.x > 0.0f ? available.x : 1.0f);
  auto height = static_cast<std::uint32_t>(available.y > 0.0f ? available.y : 1.0f);

  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  const auto final_image = scene_renderer_module.final_image();

  if (final_image.is_valid() && available.x > 0.0f && available.y > 0.0f) {
    scene_renderer_module.set_viewport_extent(sbx::math::vector2u{width, height});

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& registry = graphics_module.resource_registry();

    const auto texture_id = ui_module.texture_id(registry.get<sbx::graphics::image>(final_image).view(), sampler);

    ImGui::Image(texture_id, available);

    const auto image_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const auto image_origin = ImGui::GetItemRectMin();

    scene_renderer_module.set_viewport_offset(sbx::math::vector2{image_origin.x, image_origin.y});

    auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();
    const auto is_editing = editor_module.play_state() == editor::play_state::edit;

    auto gizmo_active = false;
    auto toolbar_active = false;
    auto view_gizmo_active = false;
    auto icons_active = false;

    if (is_editing) {
      gizmo_active = draw_viewport_gizmo(state, image_origin, available);
      toolbar_active = draw_gizmo_toolbar(state, image_origin);
      view_gizmo_active = draw_view_gizmo(image_origin, available);
      icons_active = draw_node_icons(state, image_origin, available, gizmo_active);
      draw_camera_frustum_gizmo(state, available);
    }

    if (is_editing && image_clicked && !gizmo_active && !toolbar_active && !view_gizmo_active && !icons_active) {
      const auto mouse_position = ImGui::GetMousePos();

      pick_node_at_viewport_position(state, sbx::math::vector2{mouse_position.x - image_origin.x, mouse_position.y - image_origin.y}, sbx::math::vector2u{width, height});
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  return is_hovered;
}

} // namespace editor
