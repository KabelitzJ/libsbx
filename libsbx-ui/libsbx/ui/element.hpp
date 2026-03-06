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

#include <libsbx/ui/layout.hpp>

namespace sbx::ui {

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

  size_hints sizing{};

  element() = default;

  virtual ~element() = default;

  auto add_child(std::unique_ptr<element> child) -> element&;

  auto remove_child(element& child) -> void;

  [[nodiscard]] auto children() const -> const std::vector<std::unique_ptr<element>>&;

  [[nodiscard]] auto children() -> std::vector<std::unique_ptr<element>>&;

  [[nodiscard]] auto computed_rectangle() const -> const rectangle&;

  template<typename Type, typename... Args>
  requires (std::is_base_of_v<ui::layout, Type>)
  auto set_layout(Args&&... args) -> Type& {
    auto layout = std::make_unique<Type>(std::forward<Args>(args)...);
    auto& reference = *layout;

    _layout = std::move(layout);

    return reference;
  }

  auto clear_layout() -> void;

  [[nodiscard]] auto has_layout() const -> bool;

  auto resolve_layout(const rectangle& parent_rectangle) -> void;

  auto resolve_as_arranged(const rectangle& arranged_rect) -> void;

  auto submit_tree(const math::vector2& screen_size) -> void;

  auto process_input_tree(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool;

protected:

  virtual auto submit(const math::vector2& screen_size) -> void { }

  virtual auto process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool { return false; }

  auto submit_quad(const math::vector2& screen_size, const math::vector2& quad_position, const math::vector2& quad_size, const math::vector2& quad_pivot, const math::color& quad_color, std::uint32_t albedo_index, std::int32_t quad_sort_order, const math::vector2& uv_min, const math::vector2& uv_max, std::uint32_t flags, std::float_t sdf_px_range = 0.0f) -> void;

private:

  auto _resolve_self(const rectangle& parent_rectangle) -> void;

  element* _parent{nullptr};
  std::vector<std::unique_ptr<element>> _children;
  rectangle _computed_rectangle{};
  std::unique_ptr<ui::layout> _layout{};

}; // class element

} // namespace sbx::ui

#endif // LIBSBX_UI_ELEMENT_HPP_
