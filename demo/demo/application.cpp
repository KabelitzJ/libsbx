// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <demo/application.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/input.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace demo {

static auto image = sbx::graphics::image_handle{};

application::application()
: sbx::core::application{},
  _is_paused{false},
  _time{0},
  _fps{0} {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();

  platform_module.window().on_window_closed() += []([[maybe_unused]] const auto& event) {
    sbx::core::engine::quit();
  };

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& resource_registry = graphics_module.resource_registry();

  image = resource_registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
    .extent = sbx::math::vector3u{1, 1, 1},
    .format = sbx::graphics::format::r32g32b32a32_sfloat,
    .usage = sbx::graphics::image_usage::sampled | sbx::graphics::image_usage::storage,
    .name = std::string{"Test"}
  });
}

auto application::update() -> void {
  using namespace sbx::units::literals;

  if (sbx::platform::input::is_key_pressed(sbx::platform::key::escape)) {
    sbx::core::engine::quit();
  }
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace demo
