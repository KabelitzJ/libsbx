// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_IMPORT_WINDOW_HPP_
#define EDITOR_PANELS_IMPORT_WINDOW_HPP_

#include <optional>

#include <editor/panels/editor_panel.hpp>
#include <editor/panels/import_settings_editor.hpp>

namespace editor {

/**
 * @brief The modal import window: works through editor_state::import_requests one file at a time, showing that type's import settings.
 *
 * Files whose type has no importer are imported directly without showing anything.
 */
class import_window final : public editor_panel {

public:

  auto draw(editor_state& state) -> void override;

private:

  auto _import_current(editor_state& state) -> void;

  auto _advance(editor_state& state) -> void;

  std::optional<import_settings_editor> _current{};
  bool _apply_to_remaining{false};

}; // class import_window

} // namespace editor

#endif // EDITOR_PANELS_IMPORT_WINDOW_HPP_
