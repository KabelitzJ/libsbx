// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/ui_module.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::render {

auto thumbnail_sampler_create_info() -> graphics::sampler::create_info {
  return graphics::sampler::create_info{
    .mag_filter = graphics::filter::linear,
    .min_filter = graphics::filter::linear,
    .mipmap_mode = graphics::mipmap_mode::linear,
    .address_mode_u = graphics::address_mode::clamp_to_edge,
    .address_mode_v = graphics::address_mode::clamp_to_edge,
    .address_mode_w = graphics::address_mode::clamp_to_edge,
    .max_anisotropy = 1.0f,
    .max_lod = graphics::lod_clamp::none,
    .name = "Asset Tile Thumbnail Sampler",
  };
}

ui_module::ui_module()
: _thumbnail_sampler{thumbnail_sampler_create_info()} {
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
