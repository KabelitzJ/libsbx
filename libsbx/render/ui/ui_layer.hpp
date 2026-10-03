// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_UI_LAYER_HPP_
#define LIBSBX_RENDER_UI_UI_LAYER_HPP_

#include <string_view>

namespace sbx::render {

/** @brief One contributor to a frame's ImGui output; build() runs on the main thread in registration order between NewFrame() and Render(). */
class ui_layer {

public:

  virtual ~ui_layer() = default;

  /**
   * @brief A name for profiler scopes.
   *
   * @return The name.
   */
  [[nodiscard]] virtual auto name() const -> std::string_view = 0;

  /** @brief Issues this frame's ImGui calls. */
  virtual auto build() -> void = 0;

}; // class ui_layer

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_UI_LAYER_HPP_
