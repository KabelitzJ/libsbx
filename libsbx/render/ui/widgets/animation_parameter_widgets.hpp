// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_WIDGETS_ANIMATION_PARAMETER_WIDGETS_HPP_
#define LIBSBX_RENDER_UI_WIDGETS_ANIMATION_PARAMETER_WIDGETS_HPP_

#include <libsbx/assets/animation_graph.hpp>

namespace sbx::render {

/**
 * @brief Draws the control matching @p value's alternative: DragFloat, Checkbox, DragInt or a disabled "(Trigger)" label.
 *
 * @param label The widget label.
 * @param value The value to edit.
 *
 * @return True if @p value changed this frame.
 */
[[nodiscard]] auto draw_animation_parameter_value(const char* label, sbx::assets::animation_parameter_value& value) -> bool;

/**
 * @brief A zero-initialized value of the same alternative, for resetting a condition when its parameter's type changes.
 *
 * @param like The value whose alternative to match.
 *
 * @return The default value.
 */
[[nodiscard]] auto default_for_same_alternative(const sbx::assets::animation_parameter_value& like) -> sbx::assets::animation_parameter_value;

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_WIDGETS_ANIMATION_PARAMETER_WIDGETS_HPP_
