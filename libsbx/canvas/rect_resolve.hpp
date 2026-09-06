// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CANVAS_RECT_RESOLVE_HPP_
#define LIBSBX_CANVAS_RECT_RESOLVE_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/canvas/components.hpp>

namespace sbx::canvas {

/** @brief A resolved element rect in pixel space: top-left corner and size, y-down, matching platform::input::mouse_position()'s and platform::window's own pixel conventions. */
struct resolved_rect {
  math::vector2 position{};
  math::vector2 size{};
}; // struct resolved_rect

/**
 * @brief Resolves @p rect against @p parent (the canvas' own full-screen rect for a root element,
 * or a parent element's already-resolved rect for a nested one). Per axis:
 * `offset_min = anchored_position - pivot * size_delta`,
 * `offset_max = anchored_position + (1 - pivot) * size_delta`,
 * `edge = parent.position + anchor * parent.size + offset`.
 * This single formula is Unity RectTransform-compatible and needs no anchor_min == anchor_max
 * special case: when they're equal, size == size_delta (a literal size, anchored at that one
 * point); when they differ, size == (anchor_max - anchor_min) * parent.size + size_delta (size_delta
 * becomes a margin on the stretched size) -- pivot only ever shifts position, never size, since it
 * cancels out of offset_max - offset_min exactly.
 */
[[nodiscard]] inline auto resolve_rect(const rect_transform& rect, const resolved_rect& parent) -> resolved_rect {
  const auto offset_min = rect.anchored_position - math::vector2{rect.pivot.x() * rect.size_delta.x(), rect.pivot.y() * rect.size_delta.y()};
  const auto offset_max = rect.anchored_position + math::vector2{(1.0f - rect.pivot.x()) * rect.size_delta.x(), (1.0f - rect.pivot.y()) * rect.size_delta.y()};

  const auto min_x = parent.position.x() + rect.anchor_min.x() * parent.size.x() + offset_min.x();
  const auto min_y = parent.position.y() + rect.anchor_min.y() * parent.size.y() + offset_min.y();
  const auto max_x = parent.position.x() + rect.anchor_max.x() * parent.size.x() + offset_max.x();
  const auto max_y = parent.position.y() + rect.anchor_max.y() * parent.size.y() + offset_max.y();

  return resolved_rect{math::vector2{min_x, min_y}, math::vector2{max_x - min_x, max_y - min_y}};
}

} // namespace sbx::canvas

#endif // LIBSBX_CANVAS_RECT_RESOLVE_HPP_
