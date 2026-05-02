// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_VIEWPORT_PANEL_HPP_
#define EDITOR_PANELS_VIEWPORT_PANEL_HPP_

#include <span>

#include <vulkan/vulkan.h>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/graphics/images/image2d.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

class viewport_panel {

public:

  viewport_panel() = default;

  ~viewport_panel();

  auto draw(const sbx::graphics::image2d& scene_image, sbx::scenes::node selected_node) -> void;

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

  auto is_gizmo_active() const -> bool {
    return ImGuizmo::IsUsing() || ImGuizmo::IsOver();
  }

private:

  auto _update_texture(const sbx::graphics::image2d& image) -> void;

  auto _draw_toolbar() -> void;

  auto _draw_gizmo(sbx::scenes::scene& scene, sbx::scenes::node selected_node) -> void;

  static auto _to_imguizmo(const sbx::math::matrix4x4& source, std::span<std::float_t, 16> destination) -> void;

  static auto _from_imguizmo(std::span<const std::float_t, 16> source) -> sbx::math::matrix4x4;

  VkDescriptorSet _texture_id{VK_NULL_HANDLE};
  VkImageView _cached_view{VK_NULL_HANDLE};

  sbx::math::vector2u _panel_size{0u, 0u};
  sbx::math::vector2 _content_min{0.0f, 0.0f};

  bool _is_focused{false};
  bool _is_hovered{false};

  ImGuizmo::OPERATION _gizmo_operation{ImGuizmo::TRANSLATE};
  ImGuizmo::MODE _gizmo_mode{ImGuizmo::LOCAL};

}; // class viewport_panel

} // namespace editor

#endif // EDITOR_PANELS_VIEWPORT_PANEL_HPP_
