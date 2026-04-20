// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_VIEWPORT_PANEL_HPP_
#define EDITOR_PANELS_VIEWPORT_PANEL_HPP_

#include <vulkan/vulkan.h>

#include <imgui.h>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/images/image2d.hpp>

namespace editor {

class viewport_panel {

public:

  viewport_panel() = default;

  ~viewport_panel();

  auto draw(const sbx::graphics::image2d& scene_image) -> void;

  auto panel_size() const -> const sbx::math::vector2u& {
    return _panel_size;
  }

  auto content_min() const -> const sbx::math::vector2& {
    return _content_min;
  }

  auto is_focused() const -> bool {
    return _is_focused;
  }

  auto is_hovered() const -> bool {
    return _is_hovered;
  }

private:

  auto _update_texture(const sbx::graphics::image2d& image) -> void;

  VkDescriptorSet _texture_id{VK_NULL_HANDLE};
  VkImageView _cached_view{VK_NULL_HANDLE};

  sbx::math::vector2u _panel_size{0u, 0u};
  sbx::math::vector2 _content_min{0.0f, 0.0f};

  bool _is_focused{false};
  bool _is_hovered{false};

}; // class viewport_panel

} // namespace editor

#endif // EDITOR_PANELS_VIEWPORT_PANEL_HPP_
