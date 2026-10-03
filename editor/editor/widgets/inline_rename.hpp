// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_INLINE_RENAME_HPP_
#define EDITOR_WIDGETS_INLINE_RENAME_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include <imgui.h>

namespace editor {

/**
 * @brief Seeds a rename buffer and arms focus for the next draw_rename_field(). Callers track what is being renamed themselves.
 *
 * @tparam N The buffer size.
 *
 * @param buffer The scratch buffer.
 * @param focus_pending Set so the next draw focuses the field.
 * @param current_name The current name.
 */
template<std::size_t N>
auto begin_rename(std::array<char, N>& buffer, bool& focus_pending, std::string_view current_name) -> void {
  const auto count = std::min(current_name.size(), buffer.size() - 1u);
  std::copy_n(current_name.data(), count, buffer.data());
  buffer[count] = '\0';

  focus_pending = true;
}

/** @brief What happened to a rename this frame. */
struct rename_field_result {
  bool committed{false}; // Enter, or a click-away that wasn't Escape: apply the buffer
  bool ended{false}; // the session is over either way: clear the rename target
}; // struct rename_field_result

/**
 * @brief The in-place rename field, shared by node and file renames; focuses once per begin_rename.
 *
 * @tparam N The buffer size.
 *
 * @param buffer The scratch buffer.
 * @param focus_pending Cleared once the field is focused.
 * @param width The field width.
 *
 * @return Whether the rename committed and whether it ended.
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

} // namespace editor

#endif // EDITOR_WIDGETS_INLINE_RENAME_HPP_
