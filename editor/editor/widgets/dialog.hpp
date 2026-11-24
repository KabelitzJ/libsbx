// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_DIALOG_HPP_
#define EDITOR_WIDGETS_DIALOG_HPP_

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

#include <imgui.h>

namespace editor {

/** @brief One button of a dialog's button row. */
struct dialog_button {
  const char* label{""};
  bool is_primary{false}; // highlighted, and what Enter presses
  bool is_destructive{false}; // red, e.g. Delete
}; // struct dialog_button

/**
 * @brief Begins a modal dialog in the editor's one style: centred, padded, sized to its content. Pair with ImGui::EndPopup() when it returns true; open it with ImGui::OpenPopup(id).
 *
 * @param id The popup id (its title).
 * @param width The dialog's width; 0 fits the content.
 *
 * @return Whether the dialog is open this frame.
 */
inline auto begin_dialog(const char* id, float width = 440.0f) -> bool {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
  ImGui::SetNextWindowSize(ImVec2{width, 0.0f}, ImGuiCond_Appearing);

  // Pushed around Begin only: padding and spacing are read when the window opens.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{20.0f, 18.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0f, 8.0f});

  const auto is_open = ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

  ImGui::PopStyleVar(2);

  return is_open;
}

/**
 * @brief A dialog's button row: separated from the content, right-aligned like a native dialog; Enter presses the primary button and Escape the cancel button.
 *
 * Doesn't close the popup; the caller does that for the button it handles.
 *
 * @param buttons The buttons, left to right.
 * @param cancel_index Which button Escape presses, if any.
 *
 * @return The index of the button pressed this frame.
 */
inline auto draw_dialog_buttons(std::span<const dialog_button> buttons, std::optional<std::size_t> cancel_index = std::nullopt) -> std::optional<std::size_t> {
  constexpr auto button_width = 120.0f;

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const auto spacing = ImGui::GetStyle().ItemSpacing.x;
  const auto row_width = button_width * static_cast<float>(buttons.size()) + spacing * static_cast<float>(buttons.size() > 0u ? buttons.size() - 1u : 0u);

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - row_width));

  auto pressed = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < buttons.size(); ++index) {
    const auto& button = buttons[index];

    if (index > 0u) {
      ImGui::SameLine();
    }

    auto pushed = 0;

    if (button.is_destructive) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.62f, 0.18f, 0.18f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.74f, 0.24f, 0.24f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.84f, 0.28f, 0.28f, 1.0f});
      pushed = 3;
    } else if (button.is_primary) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
      pushed = 1;
    }

    if (ImGui::Button(button.label, ImVec2{button_width, 0.0f})) {
      pressed = index;
    }

    ImGui::PopStyleColor(pushed);

    if (button.is_primary) {
      ImGui::SetItemDefaultFocus();

      if (!pressed && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        pressed = index;
      }
    }
  }

  if (!pressed && cancel_index && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    pressed = cancel_index;
  }

  return pressed;
}

} // namespace editor

#endif // EDITOR_WIDGETS_DIALOG_HPP_
