// SPDX-License-Identifier: MIT
#include <libsbx/ui/ui_module.hpp>

namespace sbx::ui {

auto ui_module::update() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  const auto screen_size = graphics_module.viewport();

  const auto mouse_position = devices::input::mouse_position();
  const auto flipped_mouse = math::vector2{mouse_position.x(), screen_size.y() - mouse_position.y()};

  const auto is_down = devices::input::is_mouse_button_down(devices::mouse_button::left);
  const auto was_down = _was_mouse_down;

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto canvas_query = graph.query<ui::canvas>();

  for (auto&& [node, canvas] : canvas_query.each()) {
    canvas.update(screen_size);
    canvas.process_input(flipped_mouse, is_down, was_down);
    canvas.submit();
  }

  _was_mouse_down = is_down;
}

} // namespace sbx::ui