// SPDX-License-Identifier: MIT
#ifndef EDITOR_EDITOR_SUBRENDERER_HPP_
#define EDITOR_EDITOR_SUBRENDERER_HPP_

#include <memory>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/render_graph.hpp>

#include <editor/editor_context.hpp>

#include <editor/panels/viewport_panel.hpp>
#include <editor/panels/hierarchy_panel.hpp>
#include <editor/panels/inspector_panel.hpp>
#include <editor/panels/log_panel.hpp>

namespace editor {

class editor_subrenderer final : public sbx::graphics::subrenderer {

public:

  editor_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachment_descriptions);

  ~editor_subrenderer() override;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

private:

  auto _draw_dockspace() -> void;

  editor_context _context;
  viewport_panel _viewport_panel;
  hierarchy_panel _hierarchy_panel;
  inspector_panel _inspector_panel;
  log_panel _log_panel;

}; // class editor_subrenderer

} // namespace editor

#endif // EDITOR_EDITOR_SUBRENDERER_HPP_
