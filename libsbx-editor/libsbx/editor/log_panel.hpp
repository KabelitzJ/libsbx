// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_LOG_PANEL_HPP_
#define LIBSBX_EDITOR_LOG_PANEL_HPP_

namespace sbx::editor {

class log_panel {

public:

  log_panel();

  auto render() -> void;

private:

  bool _has_auto_scroll;

}; // class log_panel

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_LOG_PANEL_HPP_