// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_TEXTURE_HPP_
#define LIBSBX_ASSETS_TEXTURE_HPP_

#include <cstdint>
#include <limits>

#include <libsbx/assets/asset_handle.hpp>

namespace sbx::assets {

/**
 * @brief A loaded texture, identified by its bindless index. Valid to hold from load_texture().
 * Can be sampled only once resident.
 */
class texture final {

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  texture()
  : _bindless_index{invalid_index} { }

  texture(std::uint32_t bindless_index)
  : _bindless_index{bindless_index} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _bindless_index != invalid_index;
  }
  
  [[nodiscard]] auto index() const noexcept -> std::uint32_t {
    return _bindless_index;
  }

private:

  std::uint32_t _bindless_index{invalid_index};

}; // class texture

using texture_handle = asset_handle<texture>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_TEXTURE_HPP_
