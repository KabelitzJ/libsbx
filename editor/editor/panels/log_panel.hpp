// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_LOG_PANEL_HPP_
#define EDITOR_PANELS_LOG_PANEL_HPP_

namespace editor {

class log_panel {

public:

  log_panel() = default;

  auto draw() -> void;

private:

  bool _has_auto_scroll{true};

}; // class log_panel

} // namespace editor

#endif // EDITOR_PANELS_LOG_PANEL_HPP_
