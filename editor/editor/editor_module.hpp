// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <cstdint>
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

  VkDescriptorPool _descriptor_pool{nullptr};
  std::string _ini_file;

  sbx::graphics::sampler _sampler;
  VkDescriptorSet _texture_id{VK_NULL_HANDLE};
  VkImageView _cached_view{VK_NULL_HANDLE};

  std::vector<std::pair<VkDescriptorSet, std::uint64_t>> _pending_texture_frees{};

  bool _viewport_is_hovered{false};

  editor_state _state{};

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_
