// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_RESOURCES_IMAGE_HPP_
#define LIBSBX_GRAPHICS_RESOURCES_IMAGE_HPP_

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/resource_handle.hpp>

namespace sbx::graphics {

/**
 * @brief A single image type covering 2D, arrays, cubes, 3D and depth targets.
 *
 * There are no subclasses. The image type, creation flags and aspect mask are all derived from
 * @ref image_create_info::view_type and @ref image_create_info::format rather than being encoded
 * in the C++ type, so a cube map and a depth attachment are the same class with different data.
 *
 * The image owns one view spanning every mip level and array layer, which is what both the
 * bindless table and dynamic rendering want. Per mip views, needed for mip chain generation,
 * are a later addition and do not change this interface.
 *
 * The image is created without contents and in `VK_IMAGE_LAYOUT_UNDEFINED`. Filling it and moving
 * it to a usable layout is the upload context's job.
 */
class image : public utility::noncopyable {

public:

  using handle_type = VkImage;
  using view_type = VkImageView;

  struct create_info {
    math::vector3u extent{};
    graphics::format format{format::undefined};
    graphics::image_usage usage{};
    std::uint32_t mip_levels{1u};
    std::uint32_t array_layers{1u};
    graphics::image_view_type view_type{image_view_type::two_dimensional};
    graphics::samples samples{samples::count_1};
    bool concurrent_sharing{false};
    std::string name{"Image"};
  }; // struct create_info

  explicit image(const create_info& create_info);

  ~image();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  [[nodiscard]] auto view() const noexcept -> view_type {
    return _view;
  }

  [[nodiscard]] auto extent() const noexcept -> const math::vector3u& {
    return _extent;
  }

  [[nodiscard]] auto format() const noexcept -> graphics::format {
    return _format;
  }

  [[nodiscard]] auto usage() const noexcept -> image_usage {
    return _usage;
  }

  [[nodiscard]] auto mip_levels() const noexcept -> std::uint32_t {
    return _mip_levels;
  }

  [[nodiscard]] auto array_layers() const noexcept -> std::uint32_t {
    return _array_layers;
  }

  [[nodiscard]] auto aspect() const noexcept -> VkImageAspectFlags {
    return _aspect;
  }

  /**
   * @brief The subresource range spanning every mip level and array layer.
   */
  [[nodiscard]] auto subresource_range() const noexcept -> VkImageSubresourceRange;

  /**
   * @brief Creates an additional, caller-owned view over a subrange of mips/layers — e.g. one mip
   * level across all 6 faces of a cube image, to bind as a compute storage-image write target.
   *
   * Not tracked by the image; unlike @ref view, this one is the caller's responsibility to
   * `vkDestroyImageView` once done with it — typically right after the compute dispatch that used
   * it, since these are transient, bake-time-only views.
   */
  [[nodiscard]] auto create_view(graphics::image_view_type type, std::uint32_t base_mip_level, std::uint32_t mip_levels, std::uint32_t base_array_layer, std::uint32_t array_layers) const -> view_type;

  /**
   * @brief The number of mip levels a full chain would have for @p extent.
   */
  [[nodiscard]] static auto mip_levels_for(const math::vector3u& extent) noexcept -> std::uint32_t;

  /**
   * @brief The aspect mask implied by @p format. Depth and stencil formats are recognised, and
   * everything else is treated as colour.
   */
  [[nodiscard]] static auto aspect_for(graphics::format format) noexcept -> VkImageAspectFlags;

private:

  handle_type _handle{};
  VmaAllocation _allocation{};
  view_type _view{};

  math::vector3u _extent{};
  graphics::format _format{};
  image_usage _usage{};
  VkImageAspectFlags _aspect{};

  std::uint32_t _mip_levels{};
  std::uint32_t _array_layers{};

}; // class image

using image_handle = resource_handle<image>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_RESOURCES_IMAGE_HPP_
