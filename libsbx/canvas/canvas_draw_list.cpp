// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/canvas/canvas_draw_list.hpp>

namespace sbx::canvas {

auto canvas_draw_list::add_rect(const math::vector2& top_left, const math::vector2& size, const math::color& color, const math::vector2& screen_size) -> void {
  // Pixel space is y-down (platform::input::mouse_position()'s convention) and so is Vulkan's NDC
  // (+Y points down), so this is a plain rescale -- no axis flip needed.
  const auto to_ndc = [&](const math::vector2& pixel) -> math::vector4 {
    return math::vector4{
      (pixel.x() / screen_size.x()) * 2.0f - 1.0f,
      (pixel.y() / screen_size.y()) * 2.0f - 1.0f,
      0.0f,
      1.0f
    };
  };

  const auto top_right = math::vector2{top_left.x() + size.x(), top_left.y()};
  const auto bottom_left = math::vector2{top_left.x(), top_left.y() + size.y()};
  const auto bottom_right = math::vector2{top_left.x() + size.x(), top_left.y() + size.y()};

  const auto ndc_top_left = to_ndc(top_left);
  const auto ndc_top_right = to_ndc(top_right);
  const auto ndc_bottom_left = to_ndc(bottom_left);
  const auto ndc_bottom_right = to_ndc(bottom_right);

  // Winding doesn't matter (canvas_pass's pipeline is cull_mode::none) -- kept CCW-as-drawn for hygiene.
  _vertices.push_back(canvas_vertex{ndc_top_left, color});
  _vertices.push_back(canvas_vertex{ndc_bottom_left, color});
  _vertices.push_back(canvas_vertex{ndc_top_right, color});

  _vertices.push_back(canvas_vertex{ndc_top_right, color});
  _vertices.push_back(canvas_vertex{ndc_bottom_left, color});
  _vertices.push_back(canvas_vertex{ndc_bottom_right, color});
}

} // namespace sbx::canvas
