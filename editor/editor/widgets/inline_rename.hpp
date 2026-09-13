// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_INLINE_RENAME_HPP_
#define EDITOR_WIDGETS_INLINE_RENAME_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include <imgui.h>

namespace editor::widgets {

/**
 * @brief Seeds a rename session's scratch buffer from @p current_name and arms @p focus_pending so
 * the next draw_rename_field() call focuses it. Callers still own their own "who/what is being
 * renamed" id (a node uuid, an asset path, ...) -- set that alongside this call.
 */
template<std::size_t N>
auto begin_rename(std::array<char, N>& buffer, bool& focus_pending, std::string_view current_name) -> void {
  const auto count = std::min(current_name.size(), buffer.size() - 1u);
  std::copy_n(current_name.data(), count, buffer.data());
  buffer[count] = '\0';

  focus_pending = true;
}

/** @brief What happened to a rename session this frame -- see draw_rename_field. */
struct rename_field_result {
  bool committed{false}; // Enter, or click-away that wasn't Escape -- caller should apply buffer's contents
  bool ended{false}; // this session is over either way -- caller should clear its renaming-target id
}; // struct rename_field_result

/**
 * @brief Draws the in-place rename InputText: focuses it once (per begin_rename), and reports
 * whether the edit committed and/or the session ended -- shared by hierarchy_panel's node rename
 * and asset_browser_panel's file/folder rename, which differ only in what committing does with the
 * buffer's text (rename a scene node vs. move a file), not in the widget mechanics themselves.
 */
template<std::size_t N>
auto draw_rename_field(std::array<char, N>& buffer, bool& focus_pending, std::float_t width) -> rename_field_result {
  ImGui::SetNextItemWidth(width);

  if (focus_pending) {
    ImGui::SetKeyboardFocusHere();
    focus_pending = false;
  }

  const auto submitted = ImGui::InputText("##rename", buffer.data(), buffer.size(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
  const auto deactivated = ImGui::IsItemDeactivated();
  const auto cancelled = deactivated && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

  return rename_field_result{
    .committed = submitted || (deactivated && !cancelled),
    .ended = submitted || deactivated,
  };
}

} // namespace editor::widgets

#endif // EDITOR_WIDGETS_INLINE_RENAME_HPP_
