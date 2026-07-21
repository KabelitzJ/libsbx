// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/resources/image.hpp>

#include <algorithm>
#include <bit>

#include <libsbx/utility/assert.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

static auto _image_type_for(const image_view_type view_type) noexcept -> VkImageType {
  switch (view_type) {
    case image_view_type::one_dimensional:
    case image_view_type::one_dimensional_array: {
      return VK_IMAGE_TYPE_1D;
    }
    case image_view_type::three_dimensional: {
      return VK_IMAGE_TYPE_3D;
    }
    default: {
      return VK_IMAGE_TYPE_2D;
    }
  }
}

static auto _image_flags_for(const image_view_type view_type) noexcept -> VkImageCreateFlags {
  if (view_type == image_view_type::cube || view_type == image_view_type::cube_array) {
    return VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  return VkImageCreateFlags{0u};
}

auto image::aspect_for(const graphics::format format) noexcept -> VkImageAspectFlags {
  switch (to_vk_enum<VkFormat>(format)) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT: {
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: {
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    case VK_FORMAT_S8_UINT: {
      return VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    default: {
      return VK_IMAGE_ASPECT_COLOR_BIT;
    }
  }
}

auto image::mip_levels_for(const math::vector3u& extent) noexcept -> std::uint32_t {
  const auto largest = std::max({extent.x(), extent.y(), extent.z()});

  return static_cast<std::uint32_t>(std::bit_width(largest));
}

image::image(const create_info& create_info)
: _extent{create_info.extent},
  _format{create_info.format},
  _usage{create_info.usage},
  _aspect{aspect_for(create_info.format)},
  _mip_levels{create_info.mip_levels},
  _array_layers{create_info.array_layers} {
  utility::assert_that(create_info.format != format::undefined, "Cannot create an image with an undefined format");
  utility::assert_that(create_info.extent.x() > 0u && create_info.extent.y() > 0u && create_info.extent.z() > 0u, "Cannot create an image with a zero extent");
  utility::assert_that(create_info.mip_levels > 0u, "An image needs at least one mip level");
  utility::assert_that(create_info.array_layers > 0u, "An image needs at least one array layer");

  const auto is_cube = create_info.view_type == image_view_type::cube || create_info.view_type == image_view_type::cube_array;

  utility::assert_that(!is_cube || (create_info.array_layers % 6u) == 0u, "A cube image needs a multiple of six array layers");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& allocator = graphics_module.allocator();

  auto image_info = VkImageCreateInfo{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.flags = _image_flags_for(create_info.view_type);
  image_info.imageType = _image_type_for(create_info.view_type);
  image_info.format = to_vk_enum<VkFormat>(create_info.format);
  image_info.extent = VkExtent3D{_extent.x(), _extent.y(), _extent.z()};
  image_info.mipLevels = create_info.mip_levels;
  image_info.arrayLayers = create_info.array_layers;
  image_info.samples = to_vk_enum<VkSampleCountFlagBits>(create_info.samples);
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = to_vk_enum<VkImageUsageFlags>(create_info.usage);
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  auto allocation_create_info = VmaAllocationCreateInfo{};
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  validate(vmaCreateImage(allocator, &image_info, &allocation_create_info, &_handle, &_allocation, nullptr), "vmaCreateImage");

  auto view_info = VkImageViewCreateInfo{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = _handle;
  view_info.viewType = to_vk_enum<VkImageViewType>(create_info.view_type);
  view_info.format = to_vk_enum<VkFormat>(create_info.format);
  view_info.subresourceRange = subresource_range();

  validate(vkCreateImageView(logical_device, &view_info, nullptr, &_view), "vkCreateImageView");

  if (!create_info.name.empty()) {
    logical_device.set_debug_name(_handle, create_info.name);
    logical_device.set_debug_name(_view, create_info.name + " View");
  }
}

image::~image() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& allocator = graphics_module.allocator();

  vkDestroyImageView(logical_device, _view, nullptr);

  vmaDestroyImage(allocator, _handle, _allocation);
}

auto image::subresource_range() const noexcept -> VkImageSubresourceRange {
  auto range = VkImageSubresourceRange{};
  range.aspectMask = _aspect;
  range.baseMipLevel = 0u;
  range.levelCount = _mip_levels;
  range.baseArrayLayer = 0u;
  range.layerCount = _array_layers;

  return range;
}

} // namespace sbx::graphics
