// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_PROPERTY_ROW_HPP_
#define EDITOR_WIDGETS_PROPERTY_ROW_HPP_

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>

#include <imgui.h>

namespace editor {

/**
 * @brief Lays out a widget that takes no label of its own (asset picker, button, ...) in the same
 * columns ImGui's labeled widgets use: draw() goes in the field column, label in the right-hand label
 * column, so it lines up with every DragFloat/Combo/Checkbox around it.
 */
template<typename Draw>
auto draw_property_row(const char* label, Draw&& draw) -> std::invoke_result_t<Draw> {
  const auto& style = ImGui::GetStyle();
  const auto label_x = ImGui::GetCursorScreenPos().x + ImGui::CalcItemWidth() + style.ItemInnerSpacing.x;

  ImGui::BeginGroup();
  auto result = std::invoke(std::forward<Draw>(draw));
  ImGui::EndGroup();

  ImGui::SameLine();
  ImGui::SetCursorScreenPos(ImVec2{std::max(label_x, ImGui::GetItemRectMax().x + style.ItemInnerSpacing.x), ImGui::GetCursorScreenPos().y});
  ImGui::TextUnformatted(label);

  return result;
}

/** @brief Unity-style blue bar in the left margin beside the last drawn item -- marks a value overridden on a prefab instance. */
inline auto draw_override_marker() -> void {
  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();

  ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{min.x - 5.0f, min.y}, ImVec2{min.x - 2.0f, max.y}, IM_COL32(52, 152, 219, 255));
}

} // namespace editor

#endif // EDITOR_WIDGETS_PROPERTY_ROW_HPP_
