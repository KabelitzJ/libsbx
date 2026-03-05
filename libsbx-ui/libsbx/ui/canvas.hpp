// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_CANVAS_HPP_
#define LIBSBX_UI_CANVAS_HPP_

#include <vector>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/sprites/sprites_module.hpp>

#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class canvas {

public:

  bool is_enabled{true};

  canvas() {
    const auto root_id = _allocate();

    auto& root = _elements[root_id.value];
    root.anchor_min = {0.0f, 0.0f};
    root.anchor_max = {1.0f, 1.0f};
    root.is_visible = false;
    root.is_enabled = true;
  }

  ~canvas() = default;

  canvas(canvas&&) = default;

  auto operator=(canvas&&) -> canvas& = default;

  [[nodiscard]] auto root() const -> element_id {
    return element_id{0u};
  }

  [[nodiscard]] auto get(element_id id) -> element& {
    return _elements[id.value];
  }

  [[nodiscard]] auto get(element_id id) const -> const element& {
    return _elements[id.value];
  }

  auto create_element(element_id parent = element_id{0u}) -> element_id {
    const auto id = _allocate();

    auto& elem = _elements[id.value];
    elem.parent = parent;

    auto& parent_elem = _elements[parent.value];

    if (!parent_elem.first_child.is_valid()) {
      parent_elem.first_child = id;
    } else {
      auto sibling = parent_elem.first_child;

      while (_elements[sibling.value].next_sibling.is_valid()) {
        sibling = _elements[sibling.value].next_sibling;
      }

      _elements[sibling.value].next_sibling = id;
    }

    return id;
  }

  auto remove_element(element_id id) -> void {
    if (!id.is_valid() || id.value == 0u) {
      return;
    }

    _remove_children(id);

    auto& elem = _elements[id.value];

    if (elem.parent.is_valid()) {
      _unlink_from_parent(id);
    }

    elem.is_alive = false;
    _free_list.push_back(id.value);
  }

  auto update(const math::vector2& screen_size) -> void {
    _screen_size = screen_size;

    const auto root_rect = rect{0.0f, 0.0f, screen_size.x(), screen_size.y()};

    _resolve_layout(root(), root_rect);
  }

  auto submit(sprites::sprites_module& sprites) -> void {
    if (!is_enabled) {
      return;
    }

    _submit_element(root(), sprites);
  }

private:

  auto _allocate() -> element_id {
    if (!_free_list.empty()) {
      const auto index = _free_list.back();
      _free_list.pop_back();

      _elements[index] = element{};
      _elements[index].is_alive = true;

      return element_id{index};
    }

    _elements.emplace_back();
    _elements.back().is_alive = true;

    return element_id{static_cast<std::uint32_t>(_elements.size() - 1u)};
  }

  auto _unlink_from_parent(element_id id) -> void {
    auto& elem = _elements[id.value];
    auto& parent = _elements[elem.parent.value];

    if (parent.first_child == id) {
      parent.first_child = elem.next_sibling;
    } else {
      auto sibling = parent.first_child;

      while (sibling.is_valid() && _elements[sibling.value].next_sibling != id) {
        sibling = _elements[sibling.value].next_sibling;
      }

      if (sibling.is_valid()) {
        _elements[sibling.value].next_sibling = elem.next_sibling;
      }
    }

    elem.parent = null_element;
    elem.next_sibling = null_element;
  }

  auto _remove_children(element_id id) -> void {
    auto child = _elements[id.value].first_child;

    while (child.is_valid()) {
      auto next = _elements[child.value].next_sibling;

      _remove_children(child);

      _elements[child.value].is_alive = false;
      _elements[child.value].parent = null_element;
      _elements[child.value].first_child = null_element;
      _elements[child.value].next_sibling = null_element;
      _free_list.push_back(child.value);

      child = next;
    }

    _elements[id.value].first_child = null_element;
  }

  auto _resolve_layout(element_id id, const rect& parent_rect) -> void {
    auto& elem = _elements[id.value];

    const auto anchor_left = parent_rect.x + elem.anchor_min.x() * parent_rect.width;
    const auto anchor_right = parent_rect.x + elem.anchor_max.x() * parent_rect.width;
    const auto anchor_bottom = parent_rect.y + elem.anchor_min.y() * parent_rect.height;
    const auto anchor_top = parent_rect.y + elem.anchor_max.y() * parent_rect.height;

    const auto left = anchor_left + elem.offset_min.x();
    const auto right = anchor_right + elem.offset_max.x();
    const auto bottom = anchor_bottom + elem.offset_min.y();
    const auto top = anchor_top + elem.offset_max.y();

    elem.computed_rect = rect{left, bottom, right - left, top - bottom};

    auto child = elem.first_child;

    while (child.is_valid()) {
      _resolve_layout(child, elem.computed_rect);
      child = _elements[child.value].next_sibling;
    }
  }

  auto _submit_element(element_id id, sprites::sprites_module& sprites) -> void {
    auto& elem = _elements[id.value];

    if (!elem.is_enabled) {
      return;
    }

    if (elem.is_visible) {
      const auto albedo_index = sprites.register_image(elem.image);

      const auto& r = elem.computed_rect;

      const auto pivot_x = r.x + r.width * elem.pivot.x();
      const auto pivot_y = r.y + r.height * elem.pivot.y();

      const auto position = math::vector2{
        pivot_x - _screen_size.x() * 0.5f,
        pivot_y - _screen_size.y() * 0.5f
      };

      sprites.submit(sprites::sprite_space::screen_overlay, sprites::sprite_batch::sprite_instance{
        .model = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{position.x(), position.y(), 0.0f}),
        .base_color = elem.color,
        .emissive_factor = {0.0f, 0.0f, 0.0f, 1.0f},
        .size = {r.width, r.height},
        .pivot = elem.pivot,
        .uv_min = {0.0f, 0.0f},
        .uv_max = {1.0f, 1.0f},
        .emissive_strength = 0.0f,
        .albedo_image_index = albedo_index,
        .emissive_image_index = 0u,
        .sort_order = elem.sort_order,
        .flags = 0u,
        .padding0 = 0u,
        .padding1 = 0u,
        .padding2 = 0u
      });
    }

    if (auto* txt = std::get_if<text_data>(&elem.data); txt) {
      _submit_text(elem, *txt, sprites);
    }

    auto child = elem.first_child;

    while (child.is_valid()) {
      _submit_element(child, sprites);
      child = _elements[child.value].next_sibling;
    }
  }

  auto _submit_text(element& elem, text_data& txt, sprites::sprites_module& sprites) -> void {
    if (!txt.font_ref) {
      return;
    }

    txt.ensure_layout();

    if (txt.cached_quads.empty()) {
      return;
    }

    const auto atlas_index = sprites.register_image(txt.font_ref->atlas);

    const auto& r = elem.computed_rect;

    const auto origin_x = r.x;
    const auto origin_y = r.y + r.height * 0.5f - txt.font_size * 0.5f;

    for (const auto& quad : txt.cached_quads) {
      const auto glyph_x = origin_x + quad.position.x();
      const auto glyph_y = origin_y + quad.position.y();

      const auto position = math::vector2{
        glyph_x - _screen_size.x() * 0.5f,
        glyph_y - _screen_size.y() * 0.5f
      };

      sprites.submit(sprites::sprite_space::screen_overlay, sprites::sprite_batch::sprite_instance{
        .model = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{position.x(), position.y(), 0.0f}),
        .base_color = elem.color,
        .emissive_factor = {0.0f, 0.0f, 0.0f, 1.0f},
        .size = quad.size,
        .pivot = {0.0f, 0.0f},
        .uv_min = quad.uv_min,
        .uv_max = quad.uv_max,
        .emissive_strength = 0.0f,
        .albedo_image_index = atlas_index,
        .emissive_image_index = 0u,
        .sort_order = elem.sort_order + 1,
        .flags = sprites::sprite_batch::flag_msdf_text,
        .padding0 = 0u,
        .padding1 = 0u,
        .padding2 = 0u
      });
    }
  }

  std::vector<element> _elements;
  std::vector<std::uint32_t> _free_list;
  math::vector2 _screen_size{};

}; // class canvas

} // namespace sbx::ui

#endif // LIBSBX_UI_CANVAS_HPP_
