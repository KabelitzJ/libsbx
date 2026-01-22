// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_DIALOG_HPP_
#define LIBSBX_EDITOR_DIALOG_HPP_

#include <string>
#include <functional>

#include <libsbx/editor/bindings/imgui.hpp>

namespace sbx::editor {

template<typename Type>
concept dialog_content_function = std::is_invocable_r_v<bool, Type>;

template<typename Type>
concept dialog_on_accept_function = std::is_invocable_r_v<void, Type>;

template<typename Type>
concept dialog_on_cancel_function = std::is_invocable_r_v<void, Type>;

template<dialog_content_function Content, dialog_on_accept_function OnAccept, dialog_on_cancel_function OnCancel>
auto dialog(const std::string& title, Content&& content, OnAccept&& on_accept, OnCancel&& on_cancel) -> void {
  const auto custom_backdrop_color = ImVec4{0.2f, 0.2f, 0.2f, 0.5f};

  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, custom_backdrop_color);

  ImGui::SetNextWindowSizeConstraints(ImVec2{200, 120}, ImVec2{FLT_MAX, FLT_MAX});

  if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    const auto result = std::invoke(std::forward<Content>(content));

    const auto button_width = 75.0f;
    const auto padding = 10.0f;
    const auto available_width = ImGui::GetContentRegionAvail().x;
    const auto total_width = (button_width * 2.0f) + padding;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available_width - total_width);

    const auto footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y - footer_height);

    if (ImGui::Button("Cancel", ImVec2{button_width, 0})) {
      std::invoke(std::forward<OnCancel>(on_cancel));
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (result) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Accept", ImVec2{button_width, 0})) {
      std::invoke(std::forward<OnAccept>(on_accept));
      ImGui::CloseCurrentPopup();
    }

    if (result) {
      ImGui::EndDisabled();
    }

    ImGui::EndPopup();
  }

  ImGui::PopStyleColor();
}

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_DIALOG_HPP_
