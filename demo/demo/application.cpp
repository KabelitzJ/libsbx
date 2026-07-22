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
#include <libsbx/graphics/types.hpp>

#include <libsbx/render/render_module.hpp>

namespace demo {

application::application()
: sbx::core::application{},
  _is_paused{false},
  _time{0},
  _fps{0} {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();

  platform_module.window().on_window_closed() += []([[maybe_unused]] const auto& event) {
    sbx::core::engine::quit();
  };

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  auto* data = stbi_load("demo/assets/icons/logo-dark.png", &width, &height, &channels, STBI_rgb_alpha);

  if (data != nullptr) {
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;

    auto pixels = std::vector<std::byte>{reinterpret_cast<const std::byte*>(data), reinterpret_cast<const std::byte*>(data) + count};

    stbi_image_free(data);

    render_module.upload_texture(std::move(pixels), static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), sbx::graphics::format::r8g8b8a8_unorm);
  } else {
    sbx::utility::logger<"demo">::warn("Could not load image '{}'", "demo/assets/icons/logo-dark.png");
  }
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
