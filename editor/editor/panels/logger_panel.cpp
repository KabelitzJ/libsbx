// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/logger_panel.hpp>

#include <libsbx/utility/logger.hpp>

#include <editor/fonts/material_design_icons.hpp>

namespace editor {

namespace {

auto level_color(spdlog::level::level_enum level) -> ImVec4 {
  switch (level) {
    case spdlog::level::trace: return ImVec4{0.55f, 0.55f, 0.55f, 1.0f};
    case spdlog::level::debug: return ImVec4{0.40f, 0.70f, 1.00f, 1.0f};
    case spdlog::level::warn: return ImVec4{1.00f, 0.80f, 0.20f, 1.0f};
    case spdlog::level::err: return ImVec4{1.00f, 0.35f, 0.35f, 1.0f};
    case spdlog::level::critical: return ImVec4{1.00f, 0.20f, 0.35f, 1.0f};
    case spdlog::level::info:
    default: return ImVec4{0.85f, 0.85f, 0.85f, 1.0f};
  }
}

auto level_label(spdlog::level::level_enum level) -> const char* {
  switch (level) {
    case spdlog::level::trace: return "Trace";
    case spdlog::level::debug: return "Debug";
    case spdlog::level::warn: return "Warn";
    case spdlog::level::err: return "Error";
    case spdlog::level::critical: return "Critical";
    case spdlog::level::info:
    default: return "Info";
  }
}

/** @brief The sink's pattern formatter appends an end-of-line; strip it for single-line display. */
auto trim_eol(const std::string& text) -> std::string_view {
  auto view = std::string_view{text};

  while (!view.empty() && (view.back() == '\n' || view.back() == '\r')) {
    view.remove_suffix(1u);
  }

  return view;
}

} // namespace

auto logger_panel::draw(editor_state&) -> void {
  ImGui::Begin(ICON_MDI_CONSOLE " Console###logger_panel");

  if (ImGui::Button(ICON_MDI_TRASH_CAN " Clear")) {
    sbx::utility::clear_logged_lines();
  }

  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &_is_auto_scroll);

  ImGui::SameLine();
  ImGui::SetNextItemWidth(200.0f);
  _text_filter.Draw(ICON_MDI_MAGNIFY " Filter");

  for (auto index = std::size_t{0u}; index < level_count; ++index) {
    const auto level = static_cast<spdlog::level::level_enum>(index);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, level_color(level));
    ImGui::Checkbox(level_label(level), &_level_enabled[index]);
    ImGui::PopStyleColor();
  }

  ImGui::Separator();

  ImGui::BeginChild("##logger_panel_lines", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0.0f, 0.0f});

  for (const auto& line : sbx::utility::logged_lines()) {
    const auto level_index = static_cast<std::size_t>(line.level);

    if (level_index < level_count && !_level_enabled[level_index]) {
      continue;
    }

    if (!_text_filter.PassFilter(line.text.c_str())) {
      continue;
    }

    const auto trimmed = trim_eol(line.text);

    ImGui::PushStyleColor(ImGuiCol_Text, level_color(line.level));
    ImGui::TextUnformatted(trimmed.data(), trimmed.data() + trimmed.size());
    ImGui::PopStyleColor();
  }

  ImGui::PopStyleVar();

  if (_is_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  ImGui::End();
}

} // namespace editor
