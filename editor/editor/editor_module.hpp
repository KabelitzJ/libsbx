// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <filesystem>
#include <string>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/ui/ui_module.hpp>

#include <editor/editor_ui_layer.hpp>

namespace editor {

/**
 * @brief The editor's core::module — lifecycle only. Owns editor_ui_layer (see there for
 * everything ImGui-related: dockspace, panels, the viewport, scene save/quit dialogs) and registers
 * it with ui_module; the only things left here are IniFilename (needs the ImGui context, which
 * exists by construction time, but is otherwise not a "what does the UI draw" concern) and
 * grid_enabled, both one-time engine-level settings rather than per-frame UI.
 */
class editor_module final : public sbx::utility::noncopyable {

public:

  using dependencies = sbx::core::dependency_list<sbx::graphics::graphics_module, sbx::assets::assets_module, sbx::scenes::scenes_module, sbx::render::scene_renderer_module, sbx::render::ui_module>;

  editor_module();

  ~editor_module();

  /** @see editor_ui_layer::is_viewport_hovered */
  [[nodiscard]] auto is_viewport_hovered() const noexcept -> bool {
    return _ui_layer.is_viewport_hovered();
  }

  /** @brief Called once by application.cpp right after its own initial scene load. */
  auto set_scene_path(std::filesystem::path path) -> void {
    _ui_layer.set_scene_path(std::move(path));
  }

  /** @see editor_ui_layer::request_quit */
  auto request_quit() -> void {
    _ui_layer.request_quit();
  }

private:

  std::string _ini_file;
  editor_ui_layer _ui_layer{};

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_
