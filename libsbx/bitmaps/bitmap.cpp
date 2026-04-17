// SPDX-License-Identifier: MIT
#include <libsbx/bitmaps/bitmap.hpp>

#include <libsbx/units/bytes.hpp>

#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::bitmaps {

bitmap::bitmap(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error{"Mesh file not found: " + path.string()};
  }

  const auto extension = path.extension().string();

  const auto entry = _loaders().find(extension);

  if (entry == _loaders().end()) {
    throw std::runtime_error{"No loader found for extension: " + extension};
  }

  auto& [load, unload] = entry->second;

  auto timer = utility::timer{};

  auto data = std::invoke(load, path);

  _width = data.width;
  _height = data.height;
  _channels = data.channels;
  _buffer = std::move(data.buffer);

  const auto kb = units::quantity_cast<units::kilobyte>(units::byte{data.height * data.width * data.channels});

  const auto elapsed = units::quantity_cast<units::millisecond>(timer.elapsed());

  utility::logger<"bitmap">::debug("Loaded bitmap: {}, width: {}, height: {}, channels: {}, size: {} kb in {:.2f}ms", path.string(), data.width, data.height, data.channels, kb.value(), elapsed.value());
}

bitmap::~bitmap() {

}

} // namespace sbx::bitmaps
