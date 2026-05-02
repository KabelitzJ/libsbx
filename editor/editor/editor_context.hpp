// SPDX-License-Identifier: MIT
#ifndef EDITOR_EDITOR_CONTEXT_HPP_
#define EDITOR_EDITOR_CONTEXT_HPP_

#include <vulkan/vulkan.h>

#include <libsbx/devices/devices_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace editor {

class editor_context {

  inline static constexpr auto ini_file = std::string_view{"res://data/editor.ini"};
  inline static constexpr auto font_path = std::string_view{"res://fonts/Roboto-Regular.ttf"};
  inline static constexpr auto icon_path = std::string_view{"res://fonts/materialdesignicons-webfont.ttf"};

public:

  editor_context();

  ~editor_context();

  auto new_frame() -> void;

  auto render() -> void;

  auto render_draw_data(VkCommandBuffer command_buffer) -> void;

  auto wants_capture_mouse() const -> bool;

  auto wants_capture_keyboard() const -> bool;

private:

  auto _create_descriptor_pool() -> void;

  auto _init_backends() -> void;

  auto _upload_fonts() -> void;

  auto _apply_style() -> void;

  VkDescriptorPool _descriptor_pool{VK_NULL_HANDLE};

  std::string _ini_file;

}; // class editor_context

} // namespace editor

#endif // EDITOR_EDITOR_CONTEXT_HPP_
