// SPDX-License-Identifier: MIT
#include <libsbx/editor/log_panel.hpp>

#include <imgui.h>
#include <imnodes.h>
#include <implot.h>
// #include <ImGuizmo.h>

#include <spdlog/spdlog.h>

#include <libsbx/utility/logger.hpp>

namespace sbx::editor {

log_panel::log_panel()
: _has_auto_scroll{true} { }

static auto _log_color(const spdlog::level::level_enum level) -> ImVec4 {
  switch (level) {
    case spdlog::level::trace: return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    case spdlog::level::debug: return ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
    case spdlog::level::info: return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
    case spdlog::level::warn: return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
    case spdlog::level::err: return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    case spdlog::level::critical: return ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
    default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  }
}

static auto _lerp_color(const ImVec4& a, const ImVec4& b, std::float_t t) {
  return ImVec4{
    a.x + (b.x - a.x) * t,
    a.y + (b.y - a.y) * t,
    a.z + (b.z - a.z) * t,
    a.w + (b.w - a.w) * t
  };
}

auto log_panel::render() -> void {
  ImGui::Begin("Log");

  const auto lines = utility::detail::sink()->lines();

  if (ImGui::Button("Clear"))  {
    utility::detail::sink()->clear();
  }

  ImGui::SameLine();

  ImGui::Checkbox("Auto-Scroll", &_has_auto_scroll);

  ImGui::Separator();

  ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto& [msg, level] : lines) {
    ImGui::PushStyleColor(ImGuiCol_Text, _log_color(level));
    ImGui::TextUnformatted(msg.c_str());
    ImGui::PopStyleColor();
  }

  if (_has_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  ImGui::End();
}

} // namespace sbx::editor