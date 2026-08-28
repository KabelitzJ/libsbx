// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_UI_LAYER_HPP_
#define LIBSBX_RENDER_UI_UI_LAYER_HPP_

#include <string_view>

namespace sbx::render {

/**
 * @brief One independent contributor to a frame's ImGui output. Register with
 * ui_system::add_layer; build() runs on the main thread, in registration order, once per frame,
 * between ui_system's ImGui::NewFrame() and ImGui::Render() — the same relationship graph_pass's
 * execute() has to render_graph's begin/end-rendering bookends, and add_layer<Layer>(args...)
 * mirrors render_graph::add_pass<Pass>(args...) deliberately, for the same reason: several
 * independent things (editor panels today, a future in-game debug overlay, ...) each contribute a
 * fixed slice of one frame, in a fixed order, and nothing about that needs to be a callback.
 */
class ui_layer {

public:

  virtual ~ui_layer() = default;

  /** @brief A name for profiler scopes — see render_pass::name(). */
  [[nodiscard]] virtual auto name() const -> std::string_view = 0;

  /** @brief Issue this frame's ImGui:: calls (windows, widgets, ...). */
  virtual auto build() -> void = 0;

}; // class ui_layer

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_UI_LAYER_HPP_
