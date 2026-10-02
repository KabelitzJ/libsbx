// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_TEXTURE2D_ARRAY_HPP_
#define LIBSBX_ASSETS_TEXTURE2D_ARRAY_HPP_

#include <cstdint>
#include <limits>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector2.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/**
 * @brief A 2D texture array: one GPU image with a layer per source texture2d, every layer the same
 * size, format and mip count (Godot's Texture2DArray). Identified by its index in the sampled
 * 2D-array binding -- `texture_arrays[index]` in descriptors.slang, sampled with float3(uv, layer).
 * Created by asset_residency::create_texture2d_array; sampled only once resident.
 */
class texture2d_array final : public loadable {

  friend class asset_residency;

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  texture2d_array() = default;

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _bindless_index != invalid_index;
  }

  [[nodiscard]] auto index() const noexcept -> std::uint32_t {
    return _bindless_index;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto layer_count() const noexcept -> std::uint32_t {
    return _layer_count;
  }

  [[nodiscard]] auto size() const noexcept -> const math::vector2u& {
    return _size;
  }

private:

  std::uint32_t _bindless_index{invalid_index};
  std::uint32_t _layer_count{0u};
  math::vector2u _size{};
  math::uuid _id{math::uuid::nil()};

}; // class texture2d_array

using texture2d_array_handle = asset_handle<texture2d_array>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_TEXTURE2D_ARRAY_HPP_
