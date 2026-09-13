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
 * @brief Bounces @p value through a bounded scratch buffer for ImGui::InputText (which needs a raw
 * char* it can write into), truncating a currently-too-long value rather than growing the buffer.
 * Re-synced from @p value every call, so it's for a field bound to live data (edited every frame),
 * not a one-shot rename/entry session -- see inline_rename.hpp for that.
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

} // namespace editor

#endif // EDITOR_WIDGETS_TEXT_FIELD_HPP_
