// SPDX-License-Identifier: MIT
#include <libsbx/ui/canvas.hpp>

namespace sbx::ui {

canvas::canvas()
: _root{std::make_unique<element>()} {
  _root->anchor_min = {0.0f, 0.0f};
  _root->anchor_max = {1.0f, 1.0f};
}

[[nodiscard]] auto canvas::root() -> element& {
  return *_root;
}

[[nodiscard]] auto canvas::root() const -> const element& {
  return *_root;
}

auto canvas::update(const math::vector2& screen_size) -> void {
  _screen_size = screen_size;

  const auto root_rectangle = rectangle{0.0f, 0.0f, screen_size.x(), screen_size.y()};

  _root->resolve_layout(root_rectangle);
}

auto canvas::process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> void {
  if (!is_enabled) {
    return;
  }

  _root->process_input_tree(mouse_position, is_down, was_down);
}

auto canvas::submit() -> void {
  if (!is_enabled) {
    return;
  }

  _root->submit_tree(_screen_size);
}

} // namespace sbx::ui
