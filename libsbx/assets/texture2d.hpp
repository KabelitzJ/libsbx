// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_TEXTURE2D_HPP_
#define LIBSBX_ASSETS_TEXTURE2D_HPP_

#include <cstdint>
#include <limits>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/** @brief A loaded 2D texture, identified by its bindless index (textures[] in descriptors.slang). Sampleable once resident. */
class texture2d final : public loadable {

  friend class asset_residency;

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  texture2d()
  : _bindless_index{invalid_index} { }

  texture2d(std::uint32_t bindless_index)
  : _bindless_index{bindless_index} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _bindless_index != invalid_index;
  }
  
  [[nodiscard]] auto index() const noexcept -> std::uint32_t {
    return _bindless_index;
  }

  /**
   * @brief The asset's id, shared by every record of the same file (one per format).
   *
   * @return The asset id.
   */
  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  /**
   * @brief This record's own id, unique even when one file is loaded in several formats. Scripts hold this one.
   *
   * @return The record id.
   */
  [[nodiscard]] auto handle() const noexcept -> const math::uuid& {
    return _handle;
  }

  /**
   * @brief The bindless storage-image index, for textures from create_storage_image.
   *
   * @return The index, or invalid_index for ordinary loaded textures.
   */
  [[nodiscard]] auto storage_index() const noexcept -> std::uint32_t {
    return _storage_index;
  }

private:

  std::uint32_t _bindless_index{invalid_index};
  std::uint32_t _storage_index{invalid_index};
  math::uuid _id{math::uuid::nil()};
  math::uuid _handle{math::uuid::create()};

}; // class texture2d

using texture2d_handle = asset_handle<texture2d>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_TEXTURE2D_HPP_
