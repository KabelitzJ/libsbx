// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <fstream>
#include <system_error>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

inline constexpr auto texture_magic = utility::fourcc_v<"SBTX">;  // 'SBTX'
inline constexpr auto environment_magic = utility::fourcc_v<"SBEN">; // 'SBEN'

struct texture_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t channels;   // always 4 (RGBA) for now
  std::uint32_t data_size;  // bytes of pixel data following the header
}; // struct texture_header

auto asset_cooker::resolve_texture(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<pixel_data> {
  did_cook = false;

  if (needs_cook) {
    if (!_cook_texture(source, cooked)) {
      utility::logger<"assets">::warn("Could not cook texture '{}'", source.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  auto data = pixel_data{};

  // If the blob is unreadable/out-of-date (e.g. cooker version bumped), recook once.
  if (!_load_cooked_texture(cooked, data.pixels, data.width, data.height)) {
    if (!_cook_texture(source, cooked) || !_load_cooked_texture(cooked, data.pixels, data.width, data.height)) {
      utility::logger<"assets">::warn("Could not load cooked texture '{}'", cooked.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  return data;
}

auto asset_cooker::resolve_environment(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<pixel_data> {
  did_cook = false;

  if (needs_cook) {
    if (!_cook_environment_map(source, cooked)) {
      utility::logger<"assets">::warn("Could not cook environment map '{}'", source.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  auto data = pixel_data{};

  if (!_load_cooked_environment_map(cooked, data.pixels, data.width, data.height)) {
    if (!_cook_environment_map(source, cooked) || !_load_cooked_environment_map(cooked, data.pixels, data.width, data.height)) {
      utility::logger<"assets">::warn("Could not load cooked environment map '{}'", cooked.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  return data;
}

auto asset_cooker::_cook_texture(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool {
  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  auto* data = stbi_load(source.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (data == nullptr) {
    utility::logger<"assets">::warn("Cook: could not decode '{}'", source.generic_string());
    return false;
  }

  const auto data_size = static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height) * 4u;

  const auto header = texture_header{
    texture_magic,
    texture_cook_version,
    static_cast<std::uint32_t>(width),
    static_cast<std::uint32_t>(height),
    4u,
    data_size
  };

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    stbi_image_free(data);
    return false;
  }

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(data_size));

  stbi_image_free(data);

  utility::logger<"assets">::debug("Cooked texture '{}' -> '{}'", source.generic_string(), cooked.generic_string());

  return true;
}

auto asset_cooker::_load_cooked_texture(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = texture_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != texture_magic || header.version != texture_cook_version) {
    return false; // missing / corrupt / stale format -> caller recooks
  }

  pixels.resize(header.data_size);
  in.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(header.data_size));

  if (!in) {
    return false;
  }

  width = header.width;
  height = header.height;

  return true;
}

auto asset_cooker::_cook_environment_map(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool {
  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  // source is already fully resolved -- same as _cook_texture's source.string() above.
  auto* data = stbi_loadf(source.string().c_str(), &width, &height, &channels, 4);

  if (data == nullptr) {
    utility::logger<"assets">::warn("Cook: could not decode HDR '{}'", source.generic_string());
    return false;
  }

  const auto data_size = static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height) * 4u * static_cast<std::uint32_t>(sizeof(std::float_t));

  const auto header = texture_header{environment_magic, environment_cook_version, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 4u, data_size};

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    stbi_image_free(data);
    return false;
  }

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(data_size));

  stbi_image_free(data);

  utility::logger<"assets">::debug("Cooked environment '{}' -> '{}'", source.generic_string(), cooked.generic_string());
  return true;
}

auto asset_cooker::_load_cooked_environment_map(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = texture_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != environment_magic || header.version != environment_cook_version) {
    return false;
  }

  pixels.resize(header.data_size);
  in.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(header.data_size));

  if (!in) {
    return false;
  }

  width = header.width;
  height = header.height;

  return true;
}


} // namespace sbx::assets
