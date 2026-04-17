// SPDX-License-Identifier: MIT
#ifndef LIBSBX_BITMAPS_LOADERS_JPG_LOADER_HPP_
#define LIBSBX_BITMAPS_LOADERS_JPG_LOADER_HPP_

#include <libsbx/bitmaps/bitmap.hpp>

namespace sbx::bitmaps {

class jpg_loader : public bitmap::loader<jpg_loader> {

  inline static const auto is_registered = jpg_loader::register_extensions(".jpg", ".jpeg");

public:

  static auto load(const std::filesystem::path& path) -> bitmap_data;

}; // class jpg_loader

} // namespace sbx::bitmaps

#endif // LIBSBX_BITMAPS_LOADERS_JPG_LOADER_HPP_
