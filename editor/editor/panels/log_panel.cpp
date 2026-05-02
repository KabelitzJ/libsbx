// SPDX-License-Identifier: MIT
#include <editor/panels/log_panel.hpp>

#include <spdlog/spdlog.h>

#include <libsbx/utility/logger.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

static auto _log_color(spdlog::level::level_enum level) -> ImVec4 {
  switch (level) {
    case spdlog::level::trace: return ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
    case spdlog::level::debug: return ImVec4{0.3f, 0.7f, 1.0f, 1.0f};
    case spdlog::level::info: return ImVec4{0.3f, 1.0f, 0.3f, 1.0f};
    case spdlog::level::warn: return ImVec4{1.0f, 0.8f, 0.2f, 1.0f};
    case spdlog::level::err: return ImVec4{1.0f, 0.3f, 0.3f, 1.0f};
    case spdlog::level::critical: return ImVec4{1.0f, 0.1f, 0.1f, 1.0f};
    default: return ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
  }
}

auto log_panel::draw() -> void {
  ImGui::Begin(ICON_MDI_INFORMATION " Log###log_panel");

  if (ImGui::Button("Clear")) {
    sbx::utility::detail::sink()->clear();
  }

  ImGui::SameLine();
  ImGui::Checkbox("Auto-Scroll", &_has_auto_scroll);

  ImGui::Separator();

  ImGui::BeginChild("ScrollRegion", ImVec2{0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

  const auto lines = sbx::utility::detail::sink()->lines();

  for (const auto& [text, level] : lines) {
    ImGui::PushStyleColor(ImGuiCol_Text, _log_color(level));
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
  }

  if (_has_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  ImGui::End();
}

} // namespace editor
