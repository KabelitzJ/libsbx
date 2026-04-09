// SPDX-License-Identifier: MIT
#include <libsbx/ui/ui_module.hpp>

namespace sbx::ui {

auto ui_module::update() -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (!scenes_module.has_scene()) {
    return;
  }

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  const auto screen_size = environment.render_target_size();

  const auto mouse_position = devices::input::mouse_position();
  const auto flipped_mouse = math::vector2{mouse_position.x(), static_cast<std::float_t>(screen_size.y()) - mouse_position.y()};

  const auto is_down = devices::input::is_mouse_button_down(devices::mouse_button::left);
  const auto was_down = _was_mouse_down;

  auto canvas_query = graph.query<ui::canvas>();

  for (auto&& [node, canvas] : canvas_query.each()) {
    canvas.update(screen_size);
    canvas.process_input(flipped_mouse, is_down, was_down);
    canvas.submit();
  }

  _was_mouse_down = is_down;
}

} // namespace sbx::ui
