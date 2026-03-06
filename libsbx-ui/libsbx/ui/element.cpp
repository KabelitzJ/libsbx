// SPDX-License-Identifier: MIT
#include <libsbx/ui/element.hpp>

namespace sbx::ui {

auto element::add_child(std::unique_ptr<element> child) -> element& {
  child->_parent = this;
  auto& ref = *child;
  _children.push_back(std::move(child));
  return ref;
}

auto element::remove_child(element& child) -> void {
  auto it = std::find_if(_children.begin(), _children.end(), [&child](const auto& ptr) {
    return ptr.get() == &child;
  });

  if (it != _children.end()) {
    (*it)->_parent = nullptr;
    _children.erase(it);
  }
}

[[nodiscard]] auto element::children() const -> const std::vector<std::unique_ptr<element>>& {
  return _children;
}

[[nodiscard]] auto element::children() -> std::vector<std::unique_ptr<element>>& {
  return _children;
}

[[nodiscard]] auto element::computed_rectangle() const -> const rectangle& {
  return _computed_rectangle;
}

auto element::clear_layout() -> void {
  _layout.reset();
}

[[nodiscard]] auto element::has_layout() const -> bool {
  return _layout != nullptr;
}

auto element::resolve_layout(const rectangle& parent_rectangle) -> void {
  _resolve_self(parent_rectangle);

  if (_layout) {
    _layout->arrange(_computed_rectangle, _children);
  } else {
    for (auto& child : _children) {
      child->resolve_layout(_computed_rectangle);
    }
  }
}

auto element::resolve_as_arranged(const rectangle& arranged_rect) -> void {
  _computed_rectangle = arranged_rect;

  if (_layout) {
    _layout->arrange(_computed_rectangle, _children);
  } else {
    for (auto& child : _children) {
      child->resolve_layout(_computed_rectangle);
    }
  }
}

auto element::submit_tree(const math::vector2& screen_size) -> void {
  if (!is_enabled) {
    return;
  }

  submit(screen_size);

  for (auto& child : _children) {
    child->submit_tree(screen_size);
  }
}

auto element::process_input_tree(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool {
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

auto element::submit_quad(const math::vector2& screen_size, const math::vector2& quad_position, const math::vector2& quad_size, const math::vector2& quad_pivot, const math::color& quad_color, std::uint32_t albedo_index, std::int32_t quad_sort_order, const math::vector2& uv_min, const math::vector2& uv_max, std::uint32_t flags, std::float_t sdf_px_range) -> void {
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

auto element::_resolve_self(const rectangle& parent_rectangle) -> void {
  const auto anchor_left = parent_rectangle.x + anchor_min.x() * parent_rectangle.width;
  const auto anchor_right = parent_rectangle.x + anchor_max.x() * parent_rectangle.width;
  const auto anchor_bottom = parent_rectangle.y + anchor_min.y() * parent_rectangle.height;
  const auto anchor_top = parent_rectangle.y + anchor_max.y() * parent_rectangle.height;

  const auto left = anchor_left + offset_min.x();
  const auto right = anchor_right + offset_max.x();
  const auto bottom = anchor_bottom + offset_min.y();
  const auto top = anchor_top + offset_max.y();

  _computed_rectangle = rectangle{left, bottom, right - left, top - bottom};
}

} // namespace sbx::ui
