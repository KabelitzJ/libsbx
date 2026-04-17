// SPDX-License-Identifier: MIT
#ifndef LIBSBX_BITMAPS_LOADERS_PNG_LOADER_HPP_
#define LIBSBX_BITMAPS_LOADERS_PNG_LOADER_HPP_

#include <libsbx/bitmaps/bitmap.hpp>

namespace sbx::bitmaps {

class png_loader : public bitmap::loader<png_loader> {

  inline static const auto is_registered = register_extensions(".png");

public:

  static auto load(const std::filesystem::path& path) -> bitmap_data;

}; // class png_loader

} // namespace sbx::bitmaps

#endif // LIBSBX_BITMAPS_LOADERS_PNG_LOADER_HPP_
