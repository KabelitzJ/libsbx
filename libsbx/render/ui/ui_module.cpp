// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/ui_module.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::render {

ui_module::ui_module() {
  auto& presentation_module = core::engine::get_module<render::presentation_module>();

  presentation_module.set_ui_renderer(this);
}

ui_module::~ui_module() {
  auto& presentation_module = core::engine::get_module<render::presentation_module>();

  presentation_module.set_ui_renderer(nullptr);
}

auto ui_module::build_frame() -> ui_draw_data {
  return _system.build_frame();
}

auto ui_module::render(graphics::command_buffer& command_buffer, math::vector2u extent, const ui_draw_data& data) -> void {
  _system.render(command_buffer, extent, data);
}

} // namespace sbx::render
