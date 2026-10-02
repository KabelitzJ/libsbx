// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_TEXTURE_HPP_
#define LIBSBX_ASSETS_TEXTURE_HPP_

#include <cstdint>
#include <limits>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/**
 * @brief A loaded texture, identified by its bindless index. Valid to hold from load_texture().
 * Can be sampled only once resident.
 */
class texture final : public loadable {

  friend class asset_residency;

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

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  /** @brief Bindless storage-image index, for a texture a compute shader can write into as a UAV (see asset_residency::create_storage_image). invalid_index for an ordinary loaded texture -- it was never registered as a storage image. */
  [[nodiscard]] auto storage_index() const noexcept -> std::uint32_t {
    return _storage_index;
  }

private:

  std::uint32_t _bindless_index{invalid_index};
  std::uint32_t _storage_index{invalid_index};
  math::uuid _id{math::uuid::nil()};

}; // class texture

using texture_handle = asset_handle<texture>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_TEXTURE_HPP_
