// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/canvas/canvas_module.hpp>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <libsbx/platform/input.hpp>
#include <libsbx/platform/mouse_button.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/scenes/components.hpp>

namespace sbx::canvas {

auto canvas_module::update() -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& platform_module = core::engine::get_module<platform::platform_module>();
  auto& window = platform_module.window();

  const auto screen_size = math::vector2{static_cast<std::float_t>(window.width()), static_cast<std::float_t>(window.height())};
  const auto mouse_position = platform::input::mouse_position();

  _draw_list.clear();
  _wants_pointer_capture = false;

  // Cleared before re-evaluating this frame's clicks -- see ui_button's own doc comment: was_clicked
  // is true for exactly the frame a click completes, single-owner-cleared here, so no reader needs
  // to reset it themselves.
  for (auto&& [entity, button] : scene.query<ui_button>().each()) {
    button.was_clicked = false;
  }

  auto canvases = std::vector<std::pair<scenes::node, canvas>>{};

  for (auto&& [entity, root] : scene.query<canvas>().each()) {
    canvases.emplace_back(scene.node_of(entity), root);
  }

  // Lower sort_order first, so a higher one both draws on top and (since _visit for a later canvas
  // can still overwrite _wants_pointer_capture/hover state for whatever's underneath) effectively
  // wins hit-testing ties too.
  std::sort(canvases.begin(), canvases.end(), [](const auto& lhs, const auto& rhs) { return lhs.second.sort_order < rhs.second.sort_order; });

  const auto root_rect = resolved_rect{math::vector2{0.0f, 0.0f}, screen_size};

  for (const auto& [node, root] : canvases) {
    if (root.mode != render_mode::screen_space_overlay) {
      continue; // v1 gap -- see this module's own doc comment
    }

    if (!node.has_component<scenes::relationship>()) {
      continue;
    }

    for (const auto child_entity : node.get_component<scenes::relationship>().children) {
      _visit(scene, scene.node_of(child_entity), root_rect, screen_size, mouse_position);
    }
  }
}

auto canvas_module::_visit(scenes::scene& scene, scenes::node node, const resolved_rect& parent_rect, const math::vector2& screen_size, const math::vector2& mouse_position) -> void {
  if (!node.has_component<rect_transform>()) {
    return;
  }

  const auto rect = resolve_rect(node.get_component<rect_transform>(), parent_rect);

  const auto is_over =
    mouse_position.x() >= rect.position.x() && mouse_position.x() <= rect.position.x() + rect.size.x() &&
    mouse_position.y() >= rect.position.y() && mouse_position.y() <= rect.position.y() + rect.size.y();

  auto fill_color = std::optional<math::color>{};

  if (node.has_component<ui_button>()) {
    auto& button = node.get_component<ui_button>();

    if (button.interactable) {
      if (is_over) {
        _wants_pointer_capture = true;

        if (platform::input::is_mouse_button_pressed(platform::mouse_button::left)) {
          button.is_pressed = true;
        }
      }

      if (button.is_pressed && platform::input::is_mouse_button_released(platform::mouse_button::left)) {
        button.was_clicked = is_over; // only counts as a click if the release also lands on the button
        button.is_pressed = false;
      }

      button.is_hovered = is_over;
    }

    fill_color = button.is_pressed ? button.pressed_color : (button.is_hovered ? button.hovered_color : button.normal_color);
  } else if (node.has_component<ui_image>()) {
    fill_color = node.get_component<ui_image>().tint;
  }

  if (fill_color) {
    _draw_list.add_rect(rect.position, rect.size, *fill_color, screen_size);
  }

  if (node.has_component<scenes::relationship>()) {
    for (const auto child_entity : node.get_component<scenes::relationship>().children) {
      _visit(scene, scene.node_of(child_entity), rect, screen_size, mouse_position);
    }
  }
}

} // namespace sbx::canvas
