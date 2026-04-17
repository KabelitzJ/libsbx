// SPDX-License-Identifier: MIT
#ifndef LIBSBX_BITMAPS_BITMAP_HPP_
#define LIBSBX_BITMAPS_BITMAP_HPP_

#include <cinttypes>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/io/loader_factory.hpp>

// #include <libsbx/assets/asset.hpp>

namespace sbx::bitmaps {

struct bitmap_data {
  std::uint32_t width;
  std::uint32_t height;
  std::uint8_t channels;
  std::uint8_t* buffer;
}; // struct bitmap_data

class bitmap : public io::loader_factory<bitmap, bitmap_data> { //, public assets::asset<assets::asset_type::texture> {

public:

  bitmap(const std::filesystem::path& path);

  ~bitmap();

private:

  std::uint32_t _width;
  std::uint32_t _height;
  std::uint8_t _channels;
  std::uint8_t* _buffer;

}; // class bitmap

} // namespace sbx::bitmaps

#endif // LIBSBX_BITMAPS_BITMAP_HPP_
