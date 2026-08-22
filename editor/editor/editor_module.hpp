// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_module.hpp>

#include <editor/editor_state.hpp>
#include <editor/panels/editor_panel.hpp>

namespace editor {

class editor_module final : public sbx::utility::noncopyable {

  inline static constexpr auto ini_file = std::string_view{"editor/assets/data/editor.ini"};
  inline static constexpr auto font_path = std::string_view{"editor/assets/fonts/Roboto-Regular.ttf"};
  inline static constexpr auto icon_path = std::string_view{"editor/assets/fonts/materialdesignicons-webfont.ttf"};

public:

  using dependencies = sbx::core::dependency_list<sbx::graphics::graphics_module, sbx::assets::assets_module, sbx::scenes::scenes_module, sbx::render::render_module>;

  editor_module();

  ~editor_module();

  /**
   * @brief Whether the mouse was over the Viewport panel as of the last frame's UI pass. A plain
   * UI-layer fact (ImGui::IsWindowHovered) — the editor app uses it to gate viewport-only input
   * like the dev fly-camera, keeping that gameplay-adjacent logic out of editor_module itself.
   */
  [[nodiscard]] auto is_viewport_hovered() const noexcept -> bool {
    return _viewport_is_hovered;
  }

  /** @brief Called once by application.cpp right after its own initial scene load. */
  auto set_scene_path(std::filesystem::path path) -> void {
    _scene_path = std::move(path);
  }

  /**
   * @brief Quits immediately if the scene has no unsaved changes; otherwise shows a confirmation
   * dialog (Save / Don't Save / Cancel) instead of quitting right away. Use this instead of
   * sbx::core::engine::quit() directly for anything that can originate outside an explicit
   * in-editor "I'm done, discard everything" action (the window's close button, File > Quit).
   */
  auto request_quit() -> void;

private:

  auto _build_ui_frame() -> void;

  auto _update_texture(sbx::graphics::image_handle image) -> void;

  auto _retire_texture(VkDescriptorSet descriptor_set) -> void;

  auto _collect_pending_textures() -> void;

  auto _create_descriptor_pool() -> void;

  auto _initialize_backends() -> void;

  auto _upload_fonts() -> void;

  auto _apply_style() -> void;

  auto _draw_dockspace() -> void;

  auto _create_panels() -> void;

  auto _save_scene(const std::filesystem::path& path) -> void;

  /** @brief Compares the scene's current serialize() output against what's on disk at _scene_path. */
  [[nodiscard]] auto _is_scene_dirty() -> bool;

  auto _draw_save_as_dialog() -> void;

  auto _draw_unsaved_changes_dialog() -> void;

  VkDescriptorPool _descriptor_pool{nullptr};
  std::string _ini_file;

  sbx::graphics::sampler _sampler;
  VkDescriptorSet _texture_id{VK_NULL_HANDLE};
  VkImageView _cached_view{VK_NULL_HANDLE};

  std::vector<std::pair<VkDescriptorSet, std::uint64_t>> _pending_texture_frees{};

  bool _viewport_is_hovered{false};

  editor_state _state{};
  std::vector<std::unique_ptr<editor_panel>> _panels{};

  // Scene save/load path (relative to the assets directory) — empty until the first save, or
  // until application.cpp calls set_scene_path() after its own initial load.
  std::filesystem::path _scene_path{};

  bool _show_save_as_dialog{false};
  std::array<char, 256u> _save_as_buffer{};

  bool _show_unsaved_changes_dialog{false};

  // Set when the unsaved-changes dialog's "Save" has to detour through Save As (no _scene_path
  // yet) — consulted by _draw_save_as_dialog() so that detour still quits once it completes,
  // instead of silently dropping the original quit request.
  bool _quit_after_save_as{false};

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_
