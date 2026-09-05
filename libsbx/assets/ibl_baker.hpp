// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_IBL_BAKER_HPP_
#define LIBSBX_ASSETS_IBL_BAKER_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/assets/environment_map.hpp>

namespace sbx::assets {

/**
 * @brief Bakes IBL cubemaps via compute dispatch. Depends only on `graphics::graphics_module` —
 * knows nothing about asset cooking or GPU residency bookkeeping, it just turns raw equirectangular
 * pixel data into resident irradiance/prefiltered/BRDF-LUT bindless indices.
 */
class ibl_baker final : public utility::noncopyable {

public:

  ibl_baker() = default;

  /**
   * @brief Uploads the equirectangular radiance and bakes irradiance + prefiltered cubemaps for
   * @p record via compute dispatch, blocking until the GPU finishes. Runs on the async compute queue
   * (never the graphics queue); indices are resident and safe to read from any thread once this returns.
   */
  auto bake_environment(environment_map& record, const std::vector<std::byte>& pixels, std::uint32_t width, std::uint32_t height) -> void;

  /**
   * @brief The bindless index of the global BRDF LUT, baked once (lazily, on the first
   * environment-map load) and shared by every environment. `environment_map::invalid_index` if
   * nothing has been baked yet.
   */
  [[nodiscard]] auto brdf_lut_index() const noexcept -> std::uint32_t {
    return _brdf_lut_index;
  }

private:

  // IBL bake sizes. The prefiltered cube is a real mip chain now, not N discrete images, so these fix the base resolution and how many mips it carries.
  inline static constexpr auto radiance_cube_size = std::uint32_t{512u};
  inline static constexpr auto irradiance_cube_size = std::uint32_t{64u};
  inline static constexpr auto prefiltered_cube_size = std::uint32_t{512u};
  inline static constexpr auto prefiltered_mip_count = graphics::image::mip_levels_for(math::vector3{prefiltered_cube_size});
  inline static constexpr auto brdf_lut_size = std::uint32_t{512u};

  /** @brief Bakes the global BRDF LUT into @p command_buffer if it hasn't been baked yet. */
  auto _ensure_brdf_lut(graphics::command_buffer& command_buffer) -> void;

  graphics::image_handle _brdf_lut_image{};
  std::uint32_t _brdf_lut_index{environment_map::invalid_index};

}; // class ibl_baker

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_IBL_BAKER_HPP_
