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
 * @brief An HDR image-based-lighting source. Phase A wraps the equirectangular radiance map;
 * later it also holds the baked irradiance / prefiltered / BRDF resources.
 */
class environment_map final {

  friend class assets_module;

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  environment_map() = default;

  [[nodiscard]] auto radiance_index() const noexcept -> std::uint32_t {
    return _radiance_index;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

private:

  std::uint32_t _radiance_index{invalid_index};
  math::uuid _id{math::uuid::nil()};

}; // class environment_map

using environment_map_handle = asset_handle<environment_map>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ENVIRONMENT_MAP_HPP_^
