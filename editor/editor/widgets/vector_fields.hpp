// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_VECTOR_FIELDS_HPP_
#define EDITOR_WIDGETS_VECTOR_FIELDS_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include <imgui.h>

#include <libsbx/math/color.hpp>

#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>
#include <editor/commands/component_commands.hpp>

namespace editor {

/**
 * @brief Brackets a continuous-drag-style edit (DragFloat, ColorEdit4, SliderAngle, InputText, ...)
 * into one undo entry per completed edit rather than one per frame: call every frame right after
 * the widget. @p pending must outlive one full activate/deactivate cycle (a caller's function-local
 * static) -- one shared instance per section is enough since ImGui only has one active item at a time.
 */
template<typename Component>
auto bracket_edit(editor_state& state, sbx::scenes::scene& target, const sbx::scenes::node& node, const Component& component, std::optional<Component>& pending, const char* label) -> void {
  if (ImGui::IsItemActivated() && !pending) {
    pending = component;
  }

  if (ImGui::IsItemDeactivatedAfterEdit() && pending) {
    state.push_command(target, std::make_unique<modify_component_command<Component>>(node.id(), *pending, component, label));
    pending.reset();
  }
}

// changed: any axis changed this frame. started/committed: whether a drag (or a same-frame "Reset"
// context-menu click, which is its own complete started+committed gesture) began/finished this frame --
// callers use these to bracket the whole row into one undo entry instead of one per frame.
struct vector_edit_result {
  bool changed{false};
  bool started{false};
  bool committed{false};
}; // struct vector_edit_result

using vector3_edit_result = vector_edit_result;
using vector2_edit_result = vector_edit_result;

/** @brief X/Y/Z drag row with axis_colors markers (same as draw_color_edit); right-click resets every axis to reset_value. min >= max means unclamped. */
auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, std::float_t reset_value, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector3_edit_result;

/** @brief Per-axis reset values, for defaults that differ by axis (e.g. gravity's 0, -9.81, 0). */
auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, const std::array<std::float_t, 3u>& reset_values, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector3_edit_result;

/** @brief Same as draw_vector3_control, trimmed to an X/Y row. */
auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, std::float_t reset_value, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector2_edit_result;

/** @brief Per-axis reset values, for defaults that differ by axis (e.g. a 1920x1080 reference resolution). */
auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, const std::array<std::float_t, 2u>& reset_values, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector2_edit_result;

/** @brief R/G/B(/A) 0-255 drags with axis_colors markers plus a swatch that opens the picker; components is 3 or 4. */
auto draw_color_edit(const char* label, std::float_t* values, std::int32_t components) -> bool;

auto draw_color_field(const char* label, sbx::math::color& color) -> bool;

/**
 * @brief A plain key-list editor rather than an interactive curve graph -- keys don't need to be
 * authored in time order (assets::curve::evaluate() finds the bracketing pair regardless), so this
 * is enough to author any curve the graph widget would produce, just less visual.
 */
auto draw_curve_editor(const char* label, sbx::assets::curve& curve, std::float_t value_min, std::float_t value_max) -> bool;

auto draw_gradient_editor(const char* label, sbx::assets::gradient& gradient) -> bool;

} // namespace editor

#endif // EDITOR_WIDGETS_VECTOR_FIELDS_HPP_
