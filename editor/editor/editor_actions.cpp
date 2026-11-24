// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_actions.hpp>

#include <algorithm>
#include <utility>

#include <imgui_internal.h>

namespace editor {

auto chord_name(ImGuiKeyChord chord) -> std::string {
  return std::string{ImGui::GetKeyChordName(chord)};
}

auto parse_chord(std::string_view name) -> std::optional<ImGuiKeyChord> {
  auto chord = ImGuiKeyChord{0};
  auto key = ImGuiKey_None;

  while (!name.empty()) {
    const auto separator = name.find('+', 1u);
    const auto token = name.substr(0u, separator);

    if (token == "Ctrl") {
      chord |= ImGuiMod_Ctrl;
    } else if (token == "Shift") {
      chord |= ImGuiMod_Shift;
    } else if (token == "Alt") {
      chord |= ImGuiMod_Alt;
    } else if (token == "Super") {
      chord |= ImGuiMod_Super;
    } else if (key == ImGuiKey_None) {
      for (auto candidate = static_cast<int>(ImGuiKey_NamedKey_BEGIN); candidate < static_cast<int>(ImGuiKey_NamedKey_END); ++candidate) {
        if (token == ImGui::GetKeyName(static_cast<ImGuiKey>(candidate))) {
          key = static_cast<ImGuiKey>(candidate);
          break;
        }
      }

      if (key == ImGuiKey_None) {
        return std::nullopt;
      }
    } else {
      return std::nullopt;
    }

    name = (separator == std::string_view::npos) ? std::string_view{} : name.substr(separator + 1u);
  }

  if (key == ImGuiKey_None) {
    return std::nullopt;
  }

  return chord | key;
}

auto action_registry::add(editor_action action) -> void {
  _actions.push_back(std::move(action));
}

auto action_registry::find(std::string_view id) const -> sbx::memory::observer_ptr<const editor_action> {
  const auto entry = std::ranges::find(_actions, id, &editor_action::id);

  return entry != _actions.end() ? sbx::memory::make_observer(*entry) : nullptr;
}

auto action_registry::chords_of(const editor_action& action) const -> std::span<const ImGuiKeyChord> {
  if (const auto entry = _overrides.find(action.id); entry != _overrides.end()) {
    return entry->second;
  }

  return action.default_chords;
}

auto action_registry::shortcut_text(std::string_view id) const -> std::string {
  const auto action = find(id);

  if (!action) {
    return {};
  }

  const auto chords = chords_of(*action);

  return chords.empty() ? std::string{} : chord_name(chords.front());
}

auto action_registry::set_override(std::string_view id, std::vector<ImGuiKeyChord> chords) -> void {
  _overrides.insert_or_assign(std::string{id}, std::move(chords));
}

auto action_registry::clear_override(std::string_view id) -> void {
  _overrides.erase(std::string{id});
}

auto action_registry::is_enabled(const editor_action& action) const -> bool {
  return !action.is_enabled || action.is_enabled();
}

auto action_registry::run(std::string_view id) -> void {
  const auto action = find(id);

  if (!action || !is_enabled(*action)) {
    return;
  }

  std::erase(_recent, action->id);
  _recent.insert(_recent.begin(), action->id);

  if (_recent.size() > 8u) {
    _recent.pop_back();
  }

  if (action->run) {
    action->run();
  } else {
    _triggers.push_back(trigger{action->id, ImGui::GetFrameCount()});
  }
}

auto action_registry::consume(std::string_view id) -> bool {
  const auto entry = std::ranges::find(_triggers, id, &trigger::id);

  if (entry == _triggers.end()) {
    return false;
  }

  _triggers.erase(entry);

  return true;
}

auto action_registry::dispatch(std::span<const action_context> active_contexts, bool game_has_viewport) -> void {
  // A mark nobody consumed within a frame (its panel is closed) must not fire later.
  std::erase_if(_triggers, [](const trigger& entry) { return entry.frame < ImGui::GetFrameCount() - 1; });

  if (_is_capturing || ImGui::GetIO().WantTextInput) {
    return;
  }

  for (const auto& action : _actions) {
    if (game_has_viewport && !action.works_while_playing) {
      continue;
    }

    if (action.context != action_context::global && !std::ranges::contains(active_contexts, action.context)) {
      continue;
    }

    for (const auto chord : chords_of(action)) {
      if (ImGui::IsKeyChordPressed(chord)) {
        run(action.id);
        return;
      }
    }
  }
}

auto action_registry::menu_item(std::string_view id) -> void {
  const auto action = find(id);

  if (!action) {
    return;
  }

  // MenuItemEx reserves an icon column, so labels line up whether or not an item has an icon.
  const auto shortcut = shortcut_text(id);
  const auto is_checked = action->is_checked && action->is_checked();

  const auto label = action->menu_label ? action->menu_label() : action->label;

  if (ImGui::MenuItemEx(label.c_str(), action->icon, shortcut.empty() ? nullptr : shortcut.c_str(), is_checked, is_enabled(*action))) {
    run(id);
  }
}

auto action_registry::item_tooltip(std::string_view id) const -> void {
  if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    return;
  }

  const auto action = find(id);

  if (!action) {
    return;
  }

  const auto shortcut = shortcut_text(id);

  if (shortcut.empty()) {
    ImGui::SetTooltip("%s", action->label.c_str());
  } else {
    ImGui::SetTooltip("%s (%s)", action->label.c_str(), shortcut.c_str());
  }
}

} // namespace editor
