// SPDX-License-Identifier: MIT
#include <editor/editor_subrenderer.hpp>

#include <imgui.h>

#include <editor/bindings/imgui.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/scenes/scenes_module.hpp>

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

  auto& scene_image = static_cast<const sbx::graphics::image2d&>(graphics_module.attachment("scene_output"));

  _viewport_panel.draw(scene_image);

  if (scenes_module.has_scene()) {
    auto& scene = scenes_module.scene();

    _hierarchy_panel.draw(scene);
    _inspector_panel.draw(scene, _hierarchy_panel.selected_node());
  }

  _log_panel.draw();

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

  ImGui::DockSpace(ImGui::GetID("editor_dockspace"), ImVec2{0.0f, 0.0f}, ImGuiDockNodeFlags_None);

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
