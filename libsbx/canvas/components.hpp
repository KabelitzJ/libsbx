// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CANVAS_COMPONENTS_HPP_
#define LIBSBX_CANVAS_COMPONENTS_HPP_

#include <cstdint>
#include <string>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp>

namespace sbx::canvas {

enum class render_mode : std::uint8_t {
  screen_space_overlay,
  screen_space_camera,
  world_space,
}; // enum class render_mode

/**
 * @brief Marks a node as the root of a UI hierarchy -- every rect_transform child (recursively)
 * resolves against this canvas' own rect. v1 only implements screen_space_overlay (the canvas
 * fills the whole window); screen_space_camera and world_space are reserved enum values for a
 * depth-tested per-camera sub-pass canvas_pass doesn't have yet -- see canvas_module's own doc
 * comment.
 */
struct canvas {
  render_mode mode{render_mode::screen_space_overlay};
  math::uuid camera{};      // only meaningful for screen_space_camera/world_space; unused in v1
  std::int32_t sort_order{0};
}; // struct canvas

/**
 * @brief A UI element's placement within its parent canvas/element. Unity RectTransform-compatible
 * anchor math -- see rect_resolve.hpp's resolve_rect for the exact formula, which handles both a
 * point anchor (anchor_min == anchor_max, size_delta is the element's literal size) and a stretched
 * one (anchor_min != anchor_max, size_delta becomes a margin on the stretched size) uniformly.
 */
struct rect_transform {
  math::vector2 anchor_min{0.5f, 0.5f};
  math::vector2 anchor_max{0.5f, 0.5f};
  math::vector2 anchored_position{0.0f, 0.0f};
  math::vector2 size_delta{100.0f, 30.0f};
  math::vector2 pivot{0.5f, 0.5f};
}; // struct rect_transform

/** @brief A solid-color filled rectangle. Texture support (tinted image, not just a flat color) is future work -- v1 only ever draws `tint`. */
struct ui_image {
  math::color tint{1.0f, 1.0f, 1.0f, 1.0f};
}; // struct ui_image

/**
 * @brief A text label. v1 gap: authored but not yet rendered -- canvas_pass has no glyph/font-atlas
 * pipeline yet (see canvas_module's own doc comment). Kept as a real component now so game code can
 * already author labels; wiring up actual glyph rendering is separate follow-up work.
 */
struct ui_text {
  std::string text{};
  std::float_t font_size{16.0f};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
}; // struct ui_text

/**
 * @brief A clickable rect. canvas_module::update() hit-tests this against the cursor every frame
 * and updates is_hovered/is_pressed/was_clicked -- polled state (matching this engine's existing
 * platform::input convention) rather than a signal, so it needs no special serialization and no
 * native/managed callback plumbing to expose to C#. was_clicked is true for exactly the frame a
 * click completes (press and release both while hovered); canvas_module clears it at the start of
 * every update() before recomputing, so no reader needs to reset it.
 */
struct ui_button {
  bool interactable{true};
  math::color normal_color{0.25f, 0.25f, 0.25f, 1.0f};
  math::color hovered_color{0.35f, 0.35f, 0.35f, 1.0f};
  math::color pressed_color{0.15f, 0.15f, 0.15f, 1.0f};

  bool is_hovered{false};
  bool is_pressed{false};
  bool was_clicked{false};
}; // struct ui_button

} // namespace sbx::canvas

#endif // LIBSBX_CANVAS_COMPONENTS_HPP_
