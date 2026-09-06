// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_WIDGETS_ANIMATION_PARAMETER_WIDGETS_HPP_
#define LIBSBX_RENDER_UI_WIDGETS_ANIMATION_PARAMETER_WIDGETS_HPP_

#include <libsbx/assets/animation_graph.hpp>

namespace sbx::render::widgets {

/**
 * @brief Draws the right control for @p value's current alternative -- a DragFloat, a Checkbox, a
 * DragInt, or a disabled "(Trigger)" label -- without switching on any type enum (see
 * assets::animation_parameter_value's doc comment). Shared by animation_graph_panel's Parameters
 * section and its transition condition editor's expected-value field.
 * @return true if @p value changed this frame.
 */
[[nodiscard]] auto draw_animation_parameter_value(const char* label, sbx::assets::animation_parameter_value& value) -> bool;

/**
 * @brief A same-alternative, zero/false-initialized value -- for when a condition's target
 * parameter (and so its expected type) changes, so evaluate_animation_condition's
 * std::get_if<current_type> never silently fails against a stale alternative.
 */
[[nodiscard]] auto default_for_same_alternative(const sbx::assets::animation_parameter_value& like) -> sbx::assets::animation_parameter_value;

} // namespace sbx::render::widgets

#endif // LIBSBX_RENDER_UI_WIDGETS_ANIMATION_PARAMETER_WIDGETS_HPP_
