// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_RESOURCES_SAMPLER_HPP_
#define LIBSBX_GRAPHICS_RESOURCES_SAMPLER_HPP_

#include <cmath>
#include <string>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/types.hpp>

namespace sbx::graphics {

/**
 * @brief A single sampler type. Deduplicated by the bindless table's sampler cache, so callers
 * describe what they want and get a shared handle rather than owning one each.
 */
class sampler : public utility::noncopyable {

public:

  using handle_type = VkSampler;

  struct create_info {
    graphics::filter mag_filter{filter::linear};
    graphics::filter min_filter{filter::linear};
    graphics::mipmap_mode mipmap_mode{mipmap_mode::linear};
    graphics::address_mode address_mode_u{address_mode::repeat};
    graphics::address_mode address_mode_v{address_mode::repeat};
    graphics::address_mode address_mode_w{address_mode::repeat};
    std::float_t max_anisotropy{1.0f};
    std::float_t min_lod{0.0f};
    std::float_t max_lod{VK_LOD_CLAMP_NONE};
    std::string name{"Sampler"};
  }; // struct create_info

  explicit sampler(const create_info& create_info);

  sampler(sampler&& other) noexcept;

  ~sampler();

  auto operator=(sampler&& other) noexcept -> sampler&;

  [[nodiscard]] auto handle() const noexcept -> handle_type;

  operator handle_type() const noexcept;

private:

  handle_type _handle{};

}; // class sampler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_RESOURCES_SAMPLER_HPP_
