// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_UI_LAYER_HPP_
#define LIBSBX_RENDER_UI_UI_LAYER_HPP_

#include <string_view>

namespace sbx::render {

/**
 * @brief One independent contributor to a frame's ImGui output. Register via
 * @ref ui_system::add_layer; build() runs on the main thread, in registration order, once per
 * frame, between ImGui::NewFrame() and ImGui::Render().
 */
class ui_layer {

public:

  virtual ~ui_layer() = default;

  /** @brief A name for profiler scopes; see @ref render_pass::name. */
  [[nodiscard]] virtual auto name() const -> std::string_view = 0;

  /** @brief Issue this frame's ImGui:: calls (windows, widgets, ...). */
  virtual auto build() -> void = 0;

}; // class ui_layer

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_UI_LAYER_HPP_
