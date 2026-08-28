// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ENVIRONMENT_MAP_HPP_
#define LIBSBX_ASSETS_ENVIRONMENT_MAP_HPP_

#include <cstdint>
#include <limits>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_handle.hpp>

namespace sbx::assets {

/**
 * @brief HDR image-based-lighting source: an equirect radiance map plus irradiance/prefiltered
 * cubemaps baked via compute at load time (see ibl_baker::bake_environment). Already fully
 * resident by the time load_environment_map returns — no async bake step to wait on.
 */
class environment_map final {

  friend class asset_residency;
  friend class ibl_baker;

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  environment_map() = default;

  [[nodiscard]] auto radiance_index() const noexcept -> std::uint32_t {
    return _radiance_index;
  }

  [[nodiscard]] auto irradiance_index() const noexcept -> std::uint32_t {
    return _irradiance_index;
  }

  [[nodiscard]] auto prefiltered_index() const noexcept -> std::uint32_t {
    return _prefiltered_index;
  }

  [[nodiscard]] auto prefiltered_mip_count() const noexcept -> std::uint32_t {
    return _prefiltered_mip_count;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

private:

  std::uint32_t _radiance_index{invalid_index};
  std::uint32_t _irradiance_index{invalid_index};
  std::uint32_t _prefiltered_index{invalid_index};
  std::uint32_t _prefiltered_mip_count{0u};
  math::uuid _id{math::uuid::nil()};

}; // class environment_map

using environment_map_handle = asset_handle<environment_map>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ENVIRONMENT_MAP_HPP_^
