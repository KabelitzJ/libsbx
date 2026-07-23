// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/assets_module.hpp>

#include <span>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/vector3.hpp>

namespace sbx::assets {

assets_module::assets_module() { }

assets_module::~assets_module() { }

auto assets_module::load_texture(const std::filesystem::path& path) -> texture_handle {
  const auto key = path.generic_string();

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _textures.find(key); entry != _textures.end()) {
      return texture_handle{entry->second};
    }
  }

  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  auto* data = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (data == nullptr) {
    utility::logger<"assets">::warn("Could not load texture '{}'", key);

    return texture_handle{};
  }

  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;

  auto pixels = std::vector<std::byte>{reinterpret_cast<const std::byte*>(data), reinterpret_cast<const std::byte*>(data) + count};

  stbi_image_free(data);

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});

  {
    auto lock = std::lock_guard{_mutex};

    _textures.emplace(key, record);
    _pending.push_back(pending_texture_upload{index, std::move(pixels), static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), graphics::format::r8g8b8a8_unorm});
  }

  return texture_handle{record};
}

auto assets_module::process_uploads(std::uint64_t frame_index) -> void {
  auto pending = std::vector<pending_texture_upload>{};

  {
    auto lock = std::lock_guard{_mutex};
    pending.swap(_pending);
  }

  if (pending.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& upload_context = graphics_module.upload_context();
  auto& bindless_table = graphics_module.bindless_table();

  for (auto& request : pending) {
    const auto handle = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{request.width, request.height, 1u},
      .format = request.format,
      .usage = graphics::image_usage::transfer_destination | graphics::image_usage::sampled,
      .name = "Texture"
    });

    const auto bytes = std::span<const std::byte>{request.pixels.data(), request.pixels.size()};

    upload_context.stage_image(handle, bytes, graphics::image_layout::shader_read_only_optimal);

    bindless_table.write_sampled_image(request.index, registry.get<graphics::image>(handle).view());

    auto lock = std::lock_guard{_mutex};

    _images.emplace(request.index, handle);
    _resident_frame.emplace(request.index, frame_index);
  }
}

auto assets_module::is_resident(const texture_handle& texture) const -> bool {
  if (!texture.is_valid()) {
    return false;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& frame_context = graphics_module.frame_context();

  const auto completed_value = frame_context.timeline_value();

  auto lock = std::lock_guard{_mutex};

  const auto entry = _resident_frame.find(texture->index());

  return entry != _resident_frame.end() && completed_value >= entry->second;
}

} // namespace sbx::assets
