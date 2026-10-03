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
 * @brief Brackets a continuous edit (DragFloat, ColorEdit4, InputText...) into one undo entry; call right after the widget every frame.
 *
 * @p pending must outlive an activation (a function-local static); one per section suffices, since ImGui has one active item.
 *
 * @tparam Component The edited component.
 *
 * @param state The editor state.
 * @param target The scene.
 * @param node The edited node.
 * @param component The component's current value.
 * @param pending The snapshot taken when the edit started.
 * @param label The undo label.
 */
template<typename Component>
auto bracket_edit(editor_state& state, sbx::scenes::scene& target, const sbx::scenes::node& node, const Component& component, std::optional<Component>& pending, const char* label) -> void {
  if (ImGui::IsItemActivated() && !pending) {
    pending = component;
  }

  if (ImGui::IsItemDeactivatedAfterEdit() && pending) {
    state.push_command(target, std::make_unique<modify_component_command<Component>>(node.id(), *pending, component, label));
  }

  // Also clear on a deactivation without an edit, or the stale snapshot becomes the next edit's "before", possibly on another node.
  if (ImGui::IsItemDeactivated()) {
    pending.reset();
  }
}

// changed: any axis changed this frame. started/committed: an edit (or a same-frame Reset) began or finished, for one undo entry per edit.
// ended: deactivated, edited or not, so an uncommitted snapshot can be dropped.
struct vector_edit_result {
  bool changed{false};
  bool started{false};
  bool committed{false};
  bool ended{false};
}; // struct vector_edit_result

using vector3_edit_result = vector_edit_result;
using vector2_edit_result = vector_edit_result;

/** @brief An X/Y/Z drag row with axis markers; right-click resets every axis to reset_value. min >= max means unclamped. */
auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, std::float_t reset_value, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector3_edit_result;

/** @brief Per-axis reset values, e.g. gravity's (0, -9.81, 0). */
auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, const std::array<std::float_t, 3u>& reset_values, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector3_edit_result;

/** @brief draw_vector3_control as an X/Y row. */
auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, std::float_t reset_value, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector2_edit_result;

/** @brief Per-axis reset values, e.g. a 1920x1080 reference resolution. */
auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, const std::array<std::float_t, 2u>& reset_values, std::float_t speed, std::float_t min = 0.0f, std::float_t max = 0.0f) -> vector2_edit_result;

/** @brief R/G/B(/A) 0-255 drags with axis markers and a picker swatch; components is 3 or 4. */
auto draw_color_edit(const char* label, std::float_t* values, std::int32_t components) -> bool;

auto draw_color_field(const char* label, sbx::math::color& color) -> bool;

/** @brief A key list editor; keys may be in any order, since curve::evaluate() finds the bracketing pair. */
auto draw_curve_editor(const char* label, sbx::assets::curve& curve, std::float_t value_min, std::float_t value_max) -> bool;

auto draw_gradient_editor(const char* label, sbx::assets::gradient& gradient) -> bool;

} // namespace editor

#endif // EDITOR_WIDGETS_VECTOR_FIELDS_HPP_
