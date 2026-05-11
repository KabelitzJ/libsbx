// SPDX-License-Identifier: MIT
#include <editor/editor_subrenderer.hpp>

#include <editor/bindings/imgui.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/devices/input.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

editor_subrenderer::editor_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachment_descriptions) {
  static_cast<void>(attachment_descriptions);
}

editor_subrenderer::~editor_subrenderer() {

}

auto editor_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  _context.new_frame();

  _draw_dockspace();

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene_image = static_cast<const sbx::graphics::image2d&>(graphics_module.attachment("selection"));

  _viewport_panel.draw(scene_image, _hierarchy_panel.selected_node());

  const auto& panel_size = _viewport_panel.panel_size();

  if (panel_size.x() > 0u && panel_size.y() > 0u) {
    graphics_module.viewports().resize("scene", panel_size);
  }

  if ((_viewport_panel.is_hovered() || _viewport_panel.is_focused()) && !(ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
    const auto& content_min = _viewport_panel.content_min();

    sbx::devices::input::set_active_viewport(content_min, sbx::math::vector2{static_cast<std::float_t>(panel_size.x()), static_cast<std::float_t>(panel_size.y())});
  }

  auto scene_active = _viewport_panel.is_hovered() && !_viewport_panel.is_gizmo_active();
  sbx::devices::input::set_scene_input_active(scene_active);

  if (auto picked = _viewport_panel.consume_picked_node()) {
    _hierarchy_panel.set_selected_node(*picked);
  }

  _log_panel.draw();
  _hierarchy_panel.draw();
  _inspector_panel.draw(_hierarchy_panel.selected_node());
  _asset_browser_panel.draw();

  _context.render();
  _context.render_draw_data(command_buffer);
}

auto editor_subrenderer::_draw_dockspace() -> void {
  auto window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

  auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});

  ImGui::Begin("##dockspace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  ImGui::DockSpace(ImGui::GetID("editor_dockspace"), ImVec2{0.0f, 0.0f}, ImGuiDockNodeFlags_NoWindowMenuButton);

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Quit")) {
        sbx::core::engine::quit();
      }

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  ImGui::End();
}

} // namespace editor
