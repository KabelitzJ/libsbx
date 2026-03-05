// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_ELEMENT_HPP_
#define LIBSBX_UI_ELEMENT_HPP_

#include <vector>
#include <memory>
#include <algorithm>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/images/image.hpp>

#include <libsbx/sprites/sprites_module.hpp>

namespace sbx::ui {

struct rectangle {

  std::float_t x{0.0f};
  std::float_t y{0.0f};
  std::float_t width{0.0f};
  std::float_t height{0.0f};

  [[nodiscard]] auto contains(const math::vector2& point) const -> bool {
    return point.x() >= x && point.x() <= x + width && point.y() >= y && point.y() <= y + height;
  }

}; // struct rectangle

class element {

public:

  math::vector2 anchor_min{0.0f, 0.0f};
  math::vector2 anchor_max{0.0f, 0.0f};
  math::vector2 offset_min{0.0f, 0.0f};
  math::vector2 offset_max{0.0f, 0.0f};
  math::vector2 pivot{0.5f, 0.5f};

  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::int32_t sort_order{0};
  bool is_enabled{true};
  bool is_interactive{false};

  element() = default;

  virtual ~element() = default;

  auto add_child(std::unique_ptr<element> child) -> element& {
    child->_parent = this;
    auto& reference = *child;

    _children.push_back(std::move(child));

    return reference;
  }

  auto remove_child(element& child) -> void {
    auto it = std::find_if(_children.begin(), _children.end(), [&child](const auto& ptr) {
      return ptr.get() == &child;
    });

    if (it != _children.end()) {
      (*it)->_parent = nullptr;
      _children.erase(it);
    }
  }

  [[nodiscard]] auto children() const -> const std::vector<std::unique_ptr<element>>& {
    return _children;
  }

  [[nodiscard]] auto computed_rectangle() const -> const rectangle& {
    return _computed_rectangle;
  }

  auto resolve_layout(const rectangle& parent_rectangle) -> void {
    const auto anchor_left = parent_rectangle.x + anchor_min.x() * parent_rectangle.width;
    const auto anchor_right = parent_rectangle.x + anchor_max.x() * parent_rectangle.width;
    const auto anchor_bottom = parent_rectangle.y + anchor_min.y() * parent_rectangle.height;
    const auto anchor_top = parent_rectangle.y + anchor_max.y() * parent_rectangle.height;

    const auto left = anchor_left + offset_min.x();
    const auto right = anchor_right + offset_max.x();
    const auto bottom = anchor_bottom + offset_min.y();
    const auto top = anchor_top + offset_max.y();

    _computed_rectangle = rectangle{left, bottom, right - left, top - bottom};

    for (auto& child : _children) {
      child->resolve_layout(_computed_rectangle);
    }
  }

  auto submit_tree(const math::vector2& screen_size) -> void {
    if (!is_enabled) {
      return;
    }

    submit(screen_size);

    for (auto& child : _children) {
      child->submit_tree(screen_size);
    }
  }

  auto process_input_tree(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool {
    if (!is_enabled) {
      return false;
    }

    for (auto it = _children.rbegin(); it != _children.rend(); ++it) {
      if ((*it)->process_input_tree(mouse_position, is_down, was_down)) {
        return true;
      }
    }

    return process_input(mouse_position, is_down, was_down);
  }

protected:

  virtual auto submit(const math::vector2& screen_size) -> void { }

  virtual auto process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool { return false; }

  auto submit_quad(const math::vector2& screen_size, const math::vector2& quad_position, const math::vector2& quad_size, const math::vector2& quad_pivot, const math::color& quad_color, std::uint32_t albedo_index, std::int32_t quad_sort_order, const math::vector2& uv_min, const math::vector2& uv_max, std::uint32_t flags, std::float_t sdf_px_range = 0.0f) -> void {
    auto& sprites_module = core::engine::get_module<sprites::sprites_module>();

    const auto position = math::vector2{
      quad_position.x() - screen_size.x() * 0.5f,
      quad_position.y() - screen_size.y() * 0.5f
    };

    sprites_module.submit(sprites::sprite_space::screen_overlay, sprites::sprite_batch::sprite_instance{
      .model = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{position.x(), position.y(), 0.0f}),
      .base_color = quad_color,
      .emissive_factor = {0.0f, 0.0f, 0.0f, 1.0f},
      .size = quad_size,
      .pivot = quad_pivot,
      .uv_min = uv_min,
      .uv_max = uv_max,
      .emissive_strength = 0.0f,
      .albedo_image_index = albedo_index,
      .emissive_image_index = 0u,
      .sort_order = quad_sort_order,
      .flags = flags,
      .sdf_px_range = sdf_px_range,
      .padding0 = 0u,
      .padding1 = 0u
    });
  }

private:

  element* _parent{nullptr};
  std::vector<std::unique_ptr<element>> _children;
  rectangle _computed_rectangle{};

}; // class element

} // namespace sbx::ui

#endif // LIBSBX_UI_ELEMENT_HPP_
