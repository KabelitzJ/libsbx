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

/** @brief Bakes IBL cubemaps with compute, turning equirectangular pixels into resident irradiance, prefiltered and BRDF LUT bindless indices. */
class ibl_baker final : public utility::noncopyable {

public:

  ibl_baker() = default;

  /**
   * @brief Uploads the equirectangular radiance and bakes irradiance and prefiltered cubemaps on the compute queue, blocking until done.
   *
   * @param record The environment map receiving the indices.
   * @param pixels The equirectangular radiance pixels.
   * @param width The source width.
   * @param height The source height.
   */
  auto bake_environment(environment_map& record, const std::vector<std::byte>& pixels, std::uint32_t width, std::uint32_t height) -> void;

  /**
   * @brief The bindless index of the global BRDF LUT, baked on the first environment map and shared by all.
   *
   * @return The index, or `environment_map::invalid_index` before the first bake.
   */
  [[nodiscard]] auto brdf_lut_index() const noexcept -> std::uint32_t {
    return _brdf_lut_index;
  }

private:

  // The prefiltered cube's base resolution and mip count.
  inline static constexpr auto radiance_cube_size = std::uint32_t{512u};
  inline static constexpr auto irradiance_cube_size = std::uint32_t{64u};
  inline static constexpr auto prefiltered_cube_size = std::uint32_t{512u};
  inline static constexpr auto prefiltered_mip_count = graphics::image::mip_levels_for(math::vector3{prefiltered_cube_size});
  inline static constexpr auto brdf_lut_size = std::uint32_t{512u};

  /**
   * @brief Bakes the global BRDF LUT into @p command_buffer if it hasn't been baked yet.
   *
   * @param command_buffer The command buffer to record into.
   */
  auto _ensure_brdf_lut(graphics::command_buffer& command_buffer) -> void;

  graphics::image_handle _brdf_lut_image{};
  std::uint32_t _brdf_lut_index{environment_map::invalid_index};

}; // class ibl_baker

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_IBL_BAKER_HPP_
