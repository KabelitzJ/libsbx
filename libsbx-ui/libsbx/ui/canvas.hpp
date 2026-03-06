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

  canvas();

  ~canvas() = default;

  canvas(canvas&&) = default;

  auto operator=(canvas&&) -> canvas& = default;

  [[nodiscard]] auto root() -> element&;

  [[nodiscard]] auto root() const -> const element&;

  template<typename Type>
  requires (std::is_base_of_v<ui::element, Type>)
  auto create(element& parent) -> Type& {
    auto child = std::make_unique<Type>();
    return static_cast<Type&>(parent.add_child(std::move(child)));
  }

  template<typename Type>
  requires (std::is_base_of_v<ui::element, Type>)
  auto create() -> Type& {
    return create<Type>(*_root);
  }

  template<typename Type, typename... Args>
  requires (std::is_base_of_v<ui::layout, Type>)
  auto set_layout(Args&&... args) -> Type& {
    auto layout = std::make_unique<Type>(std::forward<Args>(args)...);
    auto& reference = *layout;

    _root->_layout = std::move(layout);

    return reference;
  }

  auto update(const math::vector2& screen_size) -> void;

  auto process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> void;

  auto submit() -> void;

private:

  std::unique_ptr<element> _root;
  math::vector2 _screen_size{};

}; // class canvas

} // namespace sbx::ui

#endif // LIBSBX_UI_CANVAS_HPP_
