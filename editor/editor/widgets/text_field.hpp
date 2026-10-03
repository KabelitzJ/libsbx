// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_TEXT_FIELD_HPP_
#define EDITOR_WIDGETS_TEXT_FIELD_HPP_

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

#include <imgui.h>

namespace editor {

/**
 * @brief InputText for a live std::string through a bounded buffer, truncating values that are too long. Re-synced every call; use inline_rename.hpp for rename sessions.
 *
 * @tparam N The buffer size.
 *
 * @param label The widget label.
 * @param value The string to edit.
 *
 * @return True if @p value changed.
 */
template<std::size_t N = 128u>
auto draw_text_field(const char* label, std::string& value) -> bool {
  auto buffer = std::array<char, N>{};
  std::strncpy(buffer.data(), value.c_str(), buffer.size() - 1u);
  buffer[buffer.size() - 1u] = '\0';

  if (ImGui::InputText(label, buffer.data(), buffer.size())) {
    value = buffer.data();
    return true;
  }

  return false;
}

/**
 * @brief draw_text_field with a placeholder @p hint while empty, for search boxes.
 *
 * @tparam N The buffer size.
 *
 * @param label The widget label.
 * @param hint The placeholder text.
 * @param value The string to edit.
 *
 * @return True if @p value changed.
 */
template<std::size_t N = 128u>
auto draw_text_field_with_hint(const char* label, const char* hint, std::string& value) -> bool {
  auto buffer = std::array<char, N>{};
  std::strncpy(buffer.data(), value.c_str(), buffer.size() - 1u);
  buffer[buffer.size() - 1u] = '\0';

  if (ImGui::InputTextWithHint(label, hint, buffer.data(), buffer.size())) {
    value = buffer.data();
    return true;
  }

  return false;
}

} // namespace editor

#endif // EDITOR_WIDGETS_TEXT_FIELD_HPP_
