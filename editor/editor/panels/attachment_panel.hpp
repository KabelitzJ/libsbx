// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_ATTACHMENT_PANEL_HPP_
#define EDITOR_PANELS_ATTACHMENT_PANEL_HPP_

#include <vulkan/vulkan.h>

#include <deque>
#include <string>

#include <libsbx/graphics/images/image2d.hpp>

namespace editor {

class attachment_panel {

public:

  attachment_panel();

  ~attachment_panel();

  auto draw() -> void;

private:

  auto _update_texture(const sbx::graphics::image2d& image) -> void;

  std::size_t _current_attachment_name;
  std::string _attachment_name;

  VkDescriptorSet _texture_id{VK_NULL_HANDLE};
  VkImageView _cached_view{VK_NULL_HANDLE};

}; // class attachment_panel

} // namespace editor

#endif // EDITOR_PANELS_ATTACHMENT_PANEL_HPP_