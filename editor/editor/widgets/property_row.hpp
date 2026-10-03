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
 * @brief Lays out an unlabeled widget (asset picker, button) in the field column with @p label in the label column, aligned with ImGui's labeled widgets.
 *
 * @tparam Draw The widget-drawing callable.
 *
 * @param label The row label.
 * @param draw Draws the widget.
 *
 * @return Whatever @p draw returns.
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

/** @brief A Unity-style bar in the left margin beside the last item, marking a prefab instance override. */
inline auto draw_override_marker() -> void {
  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();

  ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{min.x - 5.0f, min.y}, ImVec2{min.x - 2.0f, max.y}, IM_COL32(52, 152, 219, 255));
}

} // namespace editor

#endif // EDITOR_WIDGETS_PROPERTY_ROW_HPP_
