// SPDX-License-Identifier: MIT
#include <libsbx/bitmaps/loaders/png_loader.hpp>

#include <fmt/format.h>

#include <stb_image.h>

namespace sbx::bitmaps {

auto png_loader::load(const std::filesystem::path& path) -> bitmap_data {
  auto data = bitmap_data{};

  stbi_set_flip_vertically_on_load(true);

  data.buffer = stbi_load(path.string().c_str(), reinterpret_cast<int*>(&data.width), reinterpret_cast<int*>(&data.height), reinterpret_cast<int*>(&data.channels), STBI_rgb_alpha);

  if (!data.buffer) {
    throw std::runtime_error{fmt::format("Failed to load image: {}", path.string())};
  }

  return data;
}


} // namespace sbx::bitmaps
