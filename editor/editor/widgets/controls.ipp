// SPDX-License-Identifier: MIT
#include <editor/widgets/controls.hpp>

#include <magic_enum/magic_enum.hpp>

#include <fmt/format.h>

namespace editor {

template<typename... Args>
auto controls::labeled_text(const char* label, fmt::format_string<Args...> fmt, Args&&... args) -> void {
  const auto text = fmt::format(fmt, std::forward<Args>(args)...);

  ImGui::TextDisabled("%s:", label);
  ImGui::SameLine();
  ImGui::TextUnformatted(text.c_str());
}

template<typename Enum>
requires (std::is_enum_v<Enum>)
auto controls::enum_combo(const char* label, Enum& value) -> bool {
  constexpr auto names = magic_enum::enum_names<Enum>();

  auto current = static_cast<std::underlying_t<Enum>>(magic_enum::enum_index(value).value_or(0));
  const auto current_label = std::string{names[static_cast<std::size_t>(current)]};

  auto changed = false;

  if (ImGui::BeginCombo(label, current_label.c_str())) {
    for (auto i = 0u; i < names.size(); ++i) {
      const auto is_selected = (current == static_cast<std::int32_t>(i));
      const auto entry_label = std::string{names[i]};

      if (ImGui::Selectable(entry_label.c_str(), is_selected)) {
        value = magic_enum::enum_value<Enum>(i);
        changed = true;
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  return changed;
}

} // namespace editor
