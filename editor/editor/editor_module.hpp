// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_module.hpp>

namespace editor {

class editor_module final : public sbx::utility::noncopyable {

  inline static constexpr auto ini_file = std::string_view{"editor/assets/data/editor.ini"};
  inline static constexpr auto font_path = std::string_view{"editor/assets/fonts/Roboto-Regular.ttf"};
  inline static constexpr auto icon_path = std::string_view{"editor/assets/fonts/materialdesignicons-webfont.ttf"};

public:

  using dependencies = sbx::core::dependency_list<sbx::graphics::graphics_module, sbx::assets::assets_module, sbx::scenes::scenes_module, sbx::render::render_module>;

  editor_module();

  ~editor_module();

  auto post_update() -> void;

private:
  
  auto _update_texture(sbx::graphics::image_handle image) -> void;

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

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_
