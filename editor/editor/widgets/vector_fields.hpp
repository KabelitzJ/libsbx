// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_VECTOR_FIELDS_HPP_
#define EDITOR_WIDGETS_VECTOR_FIELDS_HPP_

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

#include <libsbx/math/color.hpp>

#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>
#include <editor/commands/component_commands.hpp>

namespace editor::widgets {

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

// changed: any axis changed this frame. started/committed: whether a drag (or a same-frame reset
// button click, which is its own complete started+committed gesture) began/finished this frame --
// callers use these to bracket the whole row into one undo entry instead of one per frame.
struct vector3_edit_result {
  bool changed{false};
  bool started{false};
  bool committed{false};
}; // struct vector3_edit_result

/** @brief Color-coded X/Y/Z row: click an axis button to reset it to reset_value, followed by its drag field. */
auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, std::float_t reset_value, std::float_t speed) -> vector3_edit_result;

struct vector2_edit_result {
  bool changed{false};
  bool started{false};
  bool committed{false};
}; // struct vector2_edit_result

/** @brief Same shape as draw_vector3_control, trimmed to a color-coded X/Y row. */
auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, std::float_t reset_value, std::float_t speed) -> vector2_edit_result;

auto draw_color_field(const char* label, sbx::math::color& color) -> bool;

/**
 * @brief A plain key-list editor rather than an interactive curve graph -- keys don't need to be
 * authored in time order (assets::curve::evaluate() finds the bracketing pair regardless), so this
 * is enough to author any curve the graph widget would produce, just less visual.
 */
auto draw_curve_editor(const char* label, sbx::assets::curve& curve, std::float_t value_min, std::float_t value_max) -> bool;

auto draw_gradient_editor(const char* label, sbx::assets::gradient& gradient) -> bool;

} // namespace editor::widgets

#endif // EDITOR_WIDGETS_VECTOR_FIELDS_HPP_
