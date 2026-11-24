// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_PREFERENCES_PANEL_HPP_
#define EDITOR_PANELS_PREFERENCES_PANEL_HPP_

#include <cstddef>
#include <optional>
#include <string>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief Edit > Preferences: per-user editor settings (editor_preferences) plus the editor camera's own speed/sensitivity/lens. */
class preferences_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_TUNE " Preferences###preferences_panel";

  auto draw(editor_state& state) -> void override;

  bool is_open{false};

private:

  // The General tab: colours, snap steps and the editor camera.
  auto _draw_general() -> void;

  // The Shortcuts tab: every action's chords, recording new ones, conflicts and resets.
  auto _draw_shortcuts(editor_state& state) -> void;

  // Writes the registry's overrides into the preferences file.
  auto _save_shortcuts(editor_state& state) -> void;

  std::string _shortcut_filter{};
  std::string _recording_id{}; // the action whose chord is being recorded; empty when none
  std::size_t _recording_slot{0u}; // which of its chords; one past the last adds a chord

}; // class preferences_panel

} // namespace editor

#endif // EDITOR_PANELS_PREFERENCES_PANEL_HPP_
