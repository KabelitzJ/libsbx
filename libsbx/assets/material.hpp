// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_MATERIAL_HPP_
#define LIBSBX_ASSETS_MATERIAL_HPP_

#include <cstdint>
#include <limits>

#include <libsbx/math/color.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::assets {

/**
 * @brief A surface material. For now: a base-color factor and an albedo texture (unlit). Its GPU
 * record lives at material_data[index()] in the material buffer. Absent albedo falls back to the
 * white default at build time.
 */
class material final {

  friend class assets_module;

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  struct create_info {
    math::color base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    texture_handle albedo{};
  }; // struct create_info

  material() = default;

  explicit material(const create_info& create_info)
  : _base_color_factor{create_info.base_color_factor}, _albedo{create_info.albedo} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _index != invalid_index;
  }

  [[nodiscard]] auto index() const noexcept -> std::uint32_t {
    return _index;
  }

  [[nodiscard]] auto base_color_factor() const noexcept -> const math::color& {
    return _base_color_factor;
  }

  [[nodiscard]] auto albedo() const noexcept -> const texture_handle& {
    return _albedo;
  }

private:

  math::color _base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
  texture_handle _albedo{};
  std::uint32_t _index{invalid_index};

}; // class material

using material_handle = asset_handle<material>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_MATERIAL_HPP_
