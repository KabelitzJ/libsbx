// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CANVAS_CANVAS_DRAW_LIST_HPP_
#define LIBSBX_CANVAS_CANVAS_DRAW_LIST_HPP_

#include <vector>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

namespace sbx::canvas {

/**
 * @brief One vertex of the vertex-pulled buffer canvas_pass draws (matches
 * shaders/passes/canvas.slang's `canvas_vertex`). Position is already resolved to NDC (z = 0,
 * w = 1) by canvas_draw_list::add_rect -- unlike render::debug_vertex, there's no further
 * view/projection transform in the shader, since a screen-space-overlay element's position doesn't
 * depend on any camera.
 */
struct canvas_vertex {
  math::vector4 position;
  math::color color;
}; // struct canvas_vertex

/**
 * @brief CPU-side accumulator for this frame's resolved UI geometry -- built fresh by
 * canvas_module::update() every frame, drawn and cleared by canvas_pass. Same lifecycle as
 * render::debug_draw, just triangles instead of lines.
 */
class canvas_draw_list final {

public:

  /** @brief Two triangles covering [top_left, top_left + size], both in pixel space (y-down), converted to NDC here given the current screen_size. */
  auto add_rect(const math::vector2& top_left, const math::vector2& size, const math::color& color, const math::vector2& screen_size) -> void;

  [[nodiscard]] auto vertices() const noexcept -> const std::vector<canvas_vertex>& {
    return _vertices;
  }

  auto clear() noexcept -> void {
    _vertices.clear();
  }

private:

  std::vector<canvas_vertex> _vertices{};

}; // class canvas_draw_list

} // namespace sbx::canvas

#endif // LIBSBX_CANVAS_CANVAS_DRAW_LIST_HPP_
