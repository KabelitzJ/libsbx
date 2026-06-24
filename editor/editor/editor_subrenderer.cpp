// SPDX-License-Identifier: MIT
#include <editor/editor_subrenderer.hpp>

#include <editor/bindings/imgui.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/devices/input.hpp>

#include <editor/bindings/imgui.hpp>

#include <editor/editor_module.hpp>

namespace editor {

editor_subrenderer::editor_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachment_descriptions, const std::string& attachment_name)
: _attachment_name{attachment_name} {
  static_cast<void>(attachment_descriptions);
}

editor_subrenderer::~editor_subrenderer() {

}

auto editor_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  _context.new_frame();

  _draw_dockspace();

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  auto& scene_image = static_cast<const sbx::graphics::image2d&>(graphics_module.attachment(_attachment_name));

  _viewport_panel.draw(scene_image);

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
    editor_module.set_selection(editor::node_selection{*picked});
  }

  _log_panel.draw();
  _hierarchy_panel.draw();
  _inspector_panel.draw();
  _asset_browser_panel.draw();
  _attachment_panel.draw();

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

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("Scene")) {
      if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
        // TODO: create a new empty scene
      }

      if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
        // TODO: open scene from file dialog
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
        scenes_module.save_scene("res://scenes/default.yaml");
      }

      if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
        // TODO: save scene to a path chosen via file dialog
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
        // TODO
      }

      if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
        // TODO
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
        // TODO: duplicate selected node
      }

      if (ImGui::MenuItem("Delete", "Del")) {
        // TODO: delete selected node
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Preferences...")) {
        // TODO
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create")) {
      if (ImGui::MenuItem("Empty Node")) {
        // TODO: create empty node at scene root
      }

      if (ImGui::BeginMenu("3D Object")) {
        if (ImGui::MenuItem("Cube")) {
          // TODO
        }

        if (ImGui::MenuItem("Sphere")) {
          // TODO
        }

        if (ImGui::MenuItem("Plane")) {
          // TODO
        }

        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Light")) {
        if (ImGui::MenuItem("Point Light")) {
          // TODO
        }

        if (ImGui::MenuItem("Directional Light")) {
          // TODO
        }

        ImGui::EndMenu();
      }

      if (ImGui::MenuItem("Camera")) {
        // TODO
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Material")) {
        // TODO: create a new material asset
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Hierarchy", nullptr, false)) {
        // TODO: toggle hierarchy panel
      }

      if (ImGui::MenuItem("Inspector", nullptr, false)) {
        // TODO: toggle inspector panel
      }

      if (ImGui::MenuItem("Console", nullptr, false)) {
        // TODO: toggle console panel
      }

      if (ImGui::MenuItem("Asset Browser", nullptr, false)) {
        // TODO: toggle asset browser panel
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Reset Layout")) {
        // TODO
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
      if (ImGui::MenuItem("Bake IBL")) {
        // TODO
      }

      if (ImGui::MenuItem("Statistics")) {
        // TODO
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("Documentation")) {
        // TODO
      }

      if (ImGui::MenuItem("About")) {
        // TODO
      }

      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Quit")) {
      sbx::core::engine::quit();
    }

    ImGui::EndMenuBar();
  }

  ImGui::End();
}

} // namespace editor
