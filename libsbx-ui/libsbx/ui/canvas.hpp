// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_CANVAS_HPP_
#define LIBSBX_UI_CANVAS_HPP_

#include <memory>
#include <type_traits>

#include <libsbx/math/vector2.hpp>

#include <libsbx/sprites/sprites_module.hpp>

#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class canvas {

public:

  bool is_enabled{true};

  canvas()
  : _root{std::make_unique<element>()} {
    _root->anchor_min = {0.0f, 0.0f};
    _root->anchor_max = {1.0f, 1.0f};
  }

  ~canvas() = default;

  canvas(canvas&&) = default;

  auto operator=(canvas&&) -> canvas& = default;

  [[nodiscard]] auto root() -> element& {
    return *_root;
  }

  [[nodiscard]] auto root() const -> const element& {
    return *_root;
  }

  template<typename Type>
  requires (std::is_base_of_v<element, Type>)
  auto create(element& parent) -> Type& {
    auto child = std::make_unique<Type>();
    return static_cast<Type&>(parent.add_child(std::move(child)));
  }

  template<typename Type>
  requires (std::is_base_of_v<element, Type>)
  auto create() -> Type& {
    return create<Type>(*_root);
  }

  auto update(const math::vector2& screen_size) -> void {
    _screen_size = screen_size;

    const auto root_rectangle = rectangle{0.0f, 0.0f, screen_size.x(), screen_size.y()};

    _root->resolve_layout(root_rectangle);
  }

  auto process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> void {
    if (!is_enabled) {
      return;
    }

    _root->process_input_tree(mouse_position, is_down, was_down);
  }

  auto submit() -> void {
    if (!is_enabled) {
      return;
    }

    _root->submit_tree(_screen_size);
  }

private:

  std::unique_ptr<element> _root;
  math::vector2 _screen_size{};

}; // class canvas

} // namespace sbx::ui

#endif // LIBSBX_UI_CANVAS_HPP_
